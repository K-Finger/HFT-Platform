# User guide

## Prerequisites

- Linux with POSIX shared memory and pthreads
- CMake 3.20 or newer
- A C++20 compiler (GCC 11+ or Clang 14+)
- Boost headers and OpenSSL, required only by `hft::feed`
- `CAP_SYS_NICE` or root, required to run the apps
- Git submodule `lib/order-book`, required only when building the apps

Install the dependencies on Debian or Ubuntu:

```bash
sudo apt-get install -y cmake g++ libboost-dev libssl-dev
```

`hft::core` and `hft::ipc` need nothing beyond libc, pthreads and librt.
GoogleTest and Google Benchmark download automatically, and only when
`HFT_BUILD_TESTS` or `HFT_BUILD_BENCH` is on.

## Build

1. Clone with submodules.

   ```bash
   git clone --recurse-submodules https://github.com/K-Finger/HFT-Platform.git
   cd HFT-Platform
   ```

2. Configure. Release adds `-O3 -march=native` and link-time optimisation, which
   ties the binaries to the machine that built them.

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```

3. Build.

   ```bash
   cmake --build build
   ```

Turn parts of the tree off with `-DHFT_BUILD_APPS=OFF`,
`-DHFT_BUILD_TESTS=OFF` or `-DHFT_BUILD_BENCH=OFF`. All three default to `ON` at
top level and `OFF` when the project is embedded in another build.

## Tune the host

Skip this and the apps still run. The latency targets do not hold without it.

1. Reserve huge pages. The ring maps 2MB pages when they are available.

   ```bash
   sudo sysctl -w vm.nr_hugepages=64
   grep Huge /proc/meminfo
   ```

   `HugePages_Free` must be non-zero.

2. Inspect the topology. The producer pins to core 2 and the consumer to core 3,
   set by `kIngestionCore` and `kConsumerCore` in `apps/`. Keep the two off the
   same hyperthread pair.

   ```bash
   lscpu --extended=CPU,CORE,SOCKET
   ```

3. Hand those cores to the apps by isolating them from the scheduler. Add to the
   kernel command line and reboot.

   ```
   isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3
   ```

4. Pin the clock speed so cycle counts stay comparable.

   ```bash
   sudo cpupower frequency-set --governor performance
   ```

## Run

1. Start the producer first. It creates the shared memory object.

   ```bash
   sudo ./build/apps/ingestion
   ```

2. Start the consumer.

   ```bash
   sudo ./build/apps/consumer
   ```

The consumer prints a mid price per message and a latency histogram every 50
messages.

Grant `CAP_SYS_NICE` to the binaries to drop the `sudo`:

```bash
sudo setcap cap_sys_nice+ep ./build/apps/ingestion ./build/apps/consumer
```

The ring outlives both processes. Clear it between runs:

```bash
rm -f /dev/shm/hft_ring
```

## Use the library in your own project

Embed the source tree:

```cmake
add_subdirectory(HFT-Platform)
target_link_libraries(my_strategy PRIVATE hft::ipc)
```

Or install once and consume the package:

```bash
cmake --install build --prefix /usr/local
```

```cmake
find_package(hft REQUIRED)
target_link_libraries(my_strategy PRIVATE hft::ipc hft::feed)
```

Include headers through their full path, which is also their module:

```cpp
#include "hft/ipc/shared_memory.hpp"
#include "hft/sys/affinity.hpp"
#include "hft/time/tsc.hpp"
```

## Read the ring from another language

The ring is a plain memory layout, so anything that can map
`/dev/shm/hft_ring` can consume it. Reproduce the layout exactly:

- `Message` is four little-endian `f64` (bid price, ask price, bid qty, ask qty),
  then two `u64` (timestamp ns, update id), padded to 64 bytes.
- The ring is 1024 message slots, then `tail`, then `head`. Each cursor is a `u64`
  alone on a 64-byte line.
- Load `head` with acquire ordering before reading a slot. Store `tail` with
  release ordering after the copy.

## Troubleshooting

**`lib/order-book is empty` during configure.** The clone skipped the submodule.

```bash
git submodule update --init --recursive
```

Or configure with `-DHFT_BUILD_APPS=OFF` to build the libraries alone.

**`pthread_setschedparam(SCHED_FIFO, 80) failed, CAP_SYS_NICE is required`.** Run
under `sudo` or grant the capability with `setcap`, shown above.

**`pthread_setaffinity_np failed for core N`.** The core does not exist or is
outside the process affinity mask. Check `lscpu` and lower `kIngestionCore` or
`kConsumerCore`.

**`shm_open(O_RDWR) failed, the producer must create the ring first`.** The
consumer started before the producer. Start `ingestion` first.

**Consumer prints stale prices on startup.** A previous run left messages in the
ring. Remove `/dev/shm/hft_ring` and restart the producer.

**`ring full, dropped update_id=...` on the producer.** No consumer is draining,
or the consumer cannot keep up. Start a consumer, or check that it is pinned to a
core the scheduler is not oversubscribing.

**Boost or OpenSSL not found during configure.** Install `libboost-dev` and
`libssl-dev`, or build without the feed module by depending on `hft::ipc` only.

**Latencies look far worse than expected.** Confirm huge pages are actually
granted (`HugePages_Free` drops after startup), the cores are isolated, and the
governor is `performance`.
