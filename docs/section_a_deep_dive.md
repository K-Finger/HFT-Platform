# Section A: C++ Ingestion Engine — Deep Dive

## 1. The Memory Hierarchy (foundation for everything)

Before touching any code, you need this mental model:

```
Registers       ~0.3 ns    (handful of slots, on-chip)
L1 cache        ~1 ns      (32-64 KB, per-core)
L2 cache        ~4 ns      (256 KB - 1 MB, per-core)
L3 cache        ~10-30 ns  (shared across cores)
RAM             ~60-100 ns
```

Every technique in Section A is about keeping your data in L1/L2 and never going to RAM during the hot path. If you take one RAM miss per message at 100 ns, you've already blown your 50 ns budget.

---

## 2. Huge Pages

### The problem: TLB misses

When your CPU reads a memory address, it must first translate the virtual address (what your program sees) to a physical address (actual RAM). This translation lives in the **TLB** — a tiny on-chip cache of ~64–1536 entries.

Normal pages are **4 KB**. So the TLB can cover at most:

```
1536 entries × 4 KB = 6 MB of address space
```

Your ring buffer is tiny now, but add an order book, a strategy, a logger — you're beyond 6 MB fast. Every TLB miss means the CPU must **walk the page table** — that's a RAM access, ~100 ns. In a tight loop this kills you.

**Huge Pages** are **2 MB** per page:

```
1536 entries × 2 MB = 3 GB of address space
```

One TLB entry now covers what 512 normal pages used to cover.

### How to allocate them

First, tell the kernel to reserve huge pages (run this as root):

```bash
echo 64 > /proc/sys/vm/nr_hugepages
```

Check they were allocated:

```bash
grep HugePages /proc/meminfo
# HugePages_Total:      64
# HugePages_Free:       64   ← you want these to be available
```

Now in your `shm_provider.cpp`, change the `mmap` call:

```cpp
void* ptr = mmap(
    nullptr,
    sizeof(RingBuffer),
    PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE,
    fd,
    0
);
```

Two new flags:
- `MAP_HUGETLB | MAP_HUGE_2MB` — use 2MB pages instead of 4KB
- `MAP_POPULATE` — pre-fault all pages at mmap time. Without this, the first access to each page triggers a page fault (kernel interrupt, ~1–10 µs). With it, all physical memory is wired in before your hot loop starts.

### The gotcha

`sizeof(RingBuffer)` must be a multiple of 2MB for huge pages to work. Your current `RingBuffer` is far smaller. Two options:

1. Pad the struct to 2MB with a `char padding[]` member
2. Round up the allocation size: `((sizeof(RingBuffer) + (1<<21)-1) & ~((1<<21)-1))`

---

## 3. Thread Pinning (`pthread_setaffinity_np`)

### The problem: cache eviction from migration

The Linux scheduler (CFS) moves threads between cores when it feels like it — load balancing, power management, whatever. Every migration means:

- Your L1/L2 cache (warm, fast) is on the old core
- New core's L1/L2 is cold — every access is a miss
- Re-warming takes thousands of cycles

For HFT you want your hot thread to **own** a core. Nothing else runs on it. The OS never touches it.

### How to pin

```cpp
#include <pthread.h>
#include <sched.h>

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);          // clear all bits
    CPU_SET(core_id, &cpuset);  // set bit for our core
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
```

Call this at the top of `main()` in both producer and consumer — but pin them to **different** cores:

```cpp
// producer.cpp
pin_thread_to_core(2);

// consumer.cpp
pin_thread_to_core(3);
```

### How to find your cores

```bash
lscpu | grep "CPU(s)"
# CPU(s): 8  ← you have cores 0-7

# Check NUMA topology (important on multi-socket systems)
lscpu | grep NUMA
```

**Rule:** pin producer and consumer to cores on the **same physical socket** — cross-socket memory access is 2–3× slower.

### Going further: isolating the core from the OS

For dev this is optional, but in production you'd add `isolcpus=2,3` to your kernel boot parameters. This tells Linux "never schedule anything on cores 2 and 3 unless a thread explicitly pins there." Eliminates OS jitter entirely.

---

## 4. Real-Time Scheduling (`SCHED_FIFO`)

### The problem: preemption

Even if you pin to a core, the default CFS scheduler can still preempt your thread:
- A timer interrupt fires → kernel runs for a few µs
- Another normal-priority thread needs CPU → scheduler kicks you off
- Result: jitter spikes of 10–100 µs at random

`SCHED_FIFO` is a **real-time scheduling policy**:
- Once your thread is running, it runs until it **voluntarily yields** or a higher-priority RT thread preempts it
- No timeslice expiry
- Normal (CFS) threads cannot preempt it

Priorities go 1–99. 99 is highest. Use ~80 for your hot threads — leave room above for kernel watchdogs.

### How to set it

```cpp
#include <pthread.h>
#include <sched.h>

void set_realtime(int priority) {
    struct sched_param sp;
    sp.sched_priority = priority;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (ret != 0) {
        // Will fail without root / CAP_SYS_NICE
        perror("pthread_setschedparam");
    }
}
```

Call it right after `pin_thread_to_core`:

```cpp
pin_thread_to_core(2);
set_realtime(80);
```

### The critical danger

`SCHED_FIFO` + a busy-poll loop (like `while (received < 10)`) = **that thread never yields**. It will starve the entire system including the shell. You will lose control of the machine.

Two safeguards for dev:
1. Run inside a VM or WSL — you're protected by the hypervisor
2. Add a message limit so the loop terminates

In production: the thread blocks on a condition variable or does a timed `sched_yield()` when the queue is empty.

---

## 5. Pointer-Cast Parsing (Zero-Copy Message Ingestion)

### The problem: parsing overhead

Most data parsing involves:
1. Read raw bytes
2. Extract field by field (branching, copying)
3. Store in a struct

In HFT, exchanges send **binary packets** with a fixed, documented byte layout. You don't need to parse — you can `reinterpret_cast` the raw buffer directly into your struct. Zero copies. Zero branches. The CPU just reads the struct fields from the buffer in-place.

### How it works

Say an exchange sends this 16-byte binary message:

```
Bytes 0-7:   price   (double, little-endian)
Bytes 8-11:  qty     (uint32, little-endian)
Byte  12:    side    (uint8, 0=buy 1=sell)
Bytes 13-15: padding
```

Your struct must **exactly match** that layout:

```cpp
struct alignas(8) ExchangeMessage {
    double   price;    // 8 bytes, offset 0
    uint32_t qty;      // 4 bytes, offset 8
    uint8_t  side;     // 1 byte,  offset 12
    uint8_t  pad[3];   // 3 bytes, offset 13 — match exchange padding
};
```

Then parsing is just a cast:

```cpp
uint8_t* raw_buffer = /* bytes from network or pcap */;

// Zero copy — no field extraction, no branching
const ExchangeMessage* msg = reinterpret_cast<const ExchangeMessage*>(raw_buffer);

std::cout << msg->price << "\n";  // reads directly from buffer
```

### The struct layout rules

Three things must match between your struct and the wire format:

1. **Field order** — same sequence as the protocol spec
2. **Sizes** — use exact-width types (`uint32_t` not `int`, `double` not `float`)
3. **Padding** — compilers insert padding between fields for alignment. Use `static_assert` to verify:

```cpp
static_assert(sizeof(ExchangeMessage) == 16, "struct layout mismatch");
static_assert(offsetof(ExchangeMessage, qty) == 8, "field offset mismatch");
```

If the exchange doesn't align fields, use `__attribute__((packed))`:

```cpp
struct __attribute__((packed)) ExchangeMessage { ... };
```

Avoid packed if you can — unaligned reads are slower on most CPUs.

### Endianness

Most exchanges use **big-endian** (network byte order). x86 is little-endian. You need to byte-swap fields after the cast:

```cpp
#include <arpa/inet.h>

uint32_t qty_host = ntohl(msg->qty);         // 32-bit network→host
uint64_t ts_host  = be64toh(msg->timestamp); // 64-bit
```

For Binance WebSocket the data comes as JSON so this doesn't apply — but for ITCH or FIX binary it's critical.

---

## Putting it all together

Here's what the hardened producer `main()` looks like with all of Section A applied:

```cpp
int main() {
    pin_thread_to_core(2);
    set_realtime(80);

    RingBuffer* ring = open_shared_memory(true); // uses huge pages internally

    // Hot loop — pinned, real-time, huge-page backed memory, pointer-cast messages
    for (int i = 0; i < 10; ++i) {
        uint8_t raw[sizeof(Message)]; // simulate incoming bytes
        // ... fill raw from network/pcap ...
        const Message* msg = reinterpret_cast<const Message*>(raw); // zero copy
        ring->push(*msg);
    }
}
```

---

## Your implementation checklist

1. **`shm_provider.cpp`** — add `MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE` to the `mmap` call. Handle the size alignment (round up to 2MB boundary).

2. **New file `include/thread_utils.hpp`** — `pin_thread_to_core(int)` and `set_realtime(int)` as inline functions. Include in both producer and consumer.

3. **`producer.cpp`** — call pin + realtime at top of main. Replace `__asm__("rdrand")` with a `reinterpret_cast` from a raw byte buffer to simulate pointer-cast ingestion.

4. **`include/shm_ring.hpp`** — add `static_assert` checks on the `Message` struct size and field offsets.

5. Run `grep HugePages /proc/meminfo` first — some WSL configs don't support huge pages. If `HugePages_Total` stays 0 after setting `nr_hugepages`, skip `MAP_HUGETLB` and document why.
