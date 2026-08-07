# Ingestion

The ingestion engine turns exchange frames into cache-line sized messages in
shared memory. It is the latency-critical leg of the platform: one pinned thread,
no allocation after startup, no syscall between the socket read and the ring
push.

This document covers what the engine does per message, why each step is built the
way it is, how to configure it, and how to extend it to another stream or venue.

## Prerequisites

- Boost headers and OpenSSL, for the TLS websocket
- `CAP_SYS_NICE` or root, for `SCHED_FIFO`
- One core the scheduler is not oversubscribing
- Huge pages reserved, to keep the ring behind a single TLB entry

## The path of one message

1. `WebSocketFeed::read` blocks on the TLS socket and returns the decoded frame.
2. `hft::time::now_ns` stamps arrival, before any parsing.
3. `parse_book_ticker` scans the frame for its five keys and converts them in
   place into a `Message`.
4. The caller writes the stamp into `Message::timestamp`.
5. `SpscRing::push` copies the message into the next slot and releases the head
   cursor.

Steps 2 through 5 hold the latency budget. Step 1 blocks for as long as the
exchange takes.

## Setup before the loop

`apps/ingestion/main.cpp` does four things, in order, and each one throws on
failure rather than degrading:

```cpp
hft::sys::pin_to_core(kIngestionCore);
hft::sys::request_realtime_priority(kRealtimePriority);
hft::ipc::SpscRing* ring = hft::ipc::create_shared_ring();

hft::feed::WebSocketFeed feed(kHost, kPort, kTarget);
feed.connect();
```

**Pin first.** An unpinned thread migrates between cores, and every migration
starts on a cold L1 and L2. The ring, the parser and the socket buffers all live
in that working set, so a migration costs microseconds against a nanosecond
budget.

**Then take real-time priority.** `SCHED_FIFO` stops the kernel from preempting
the thread for ordinary work. Without it a timer tick or an unrelated process
lands in the middle of a burst.

**Then create the ring.** `create_shared_ring` maps `/dev/shm/hft_ring` on 2MB
huge pages with `MAP_POPULATE`, so the whole buffer costs one TLB entry and every
page is faulted in before the first message rather than during it.

**Then connect.** Resolve, TLS handshake, websocket upgrade. All of it happens
once, before the loop, so the hot path never touches DNS or a handshake.

Order matters: the process fails on a misconfigured host before it opens a socket
to the exchange.

## The loop

```cpp
hft::Message msg{};

while (true)
{
    const std::string&  frame       = feed.read();
    const std::uint64_t received_ns = hft::time::now_ns();

    hft::feed::parse_book_ticker(frame.c_str(), msg);
    msg.timestamp = received_ns;

    if (!ring->push(msg))
        std::fprintf(stderr, "ring full, dropped update_id=%llu\n",
                     static_cast<unsigned long long>(msg.update_id));
}
```

The parser fills a caller-owned `Message` rather than returning one. `Message` is
aligned to a cache line, the ABI only guarantees the stack 16-byte aligned, and
returning an over-aligned type by value makes the compiler emit an aligned vector
store into a slot it did not align. A named `Message` gets its alignment honoured,
so the object is hoisted out of the loop and reused.

The stamp is taken before the parse, so the timestamp measures arrival and not
parse cost. Comparing `timestamp` against the consumer's own `now_ns` gives the
end-to-end age of a message; comparing consecutive `update_id` values detects
dropped exchange updates.

A failed push means the consumer is falling behind. The producer reports it and
moves on. It never blocks, never retries and never grows the buffer: stalling the
producer to protect a slow reader would hand the exchange's pace to the slowest
consumer.

## Parsing

`parse_book_ticker` reads a Binance `bookTicker` frame:

```json
{"u":400900217,"s":"BTCUSDT","b":"63501.10","B":"1.20","a":"63502.45","A":"3.40"}
```

It locates each key with `strstr` and converts the value where it sits, with
`strtoull` for the sequence number and `strtod` for the four prices and
quantities. The frame never becomes a JSON document, nothing allocates, and the
payload is never copied into an intermediate object. A DOM parser here costs
microseconds per frame and heap traffic on every message.

A missing key throws `std::runtime_error` with the offending frame. Publishing a
zeroed price would be worse than stopping: downstream cannot distinguish it from a
real quote.

The function leaves `timestamp` at zero. Stamping belongs to the caller, which
knows when the frame actually arrived.

## The message contract

`hft::Message` is the only type crossing into shared memory:

```cpp
struct alignas(64) Message
{
    double        bid_price;
    double        ask_price;
    double        bid_qty;
    double        ask_qty;
    std::uint64_t timestamp;
    std::uint64_t update_id;
};
```

The 64-byte alignment gives every message its own cache line, so a producer
writing slot N never dirties the line a consumer is reading at slot N-1. The
struct holds no pointers, which is what makes it valid in a mapping another
process reads at a different virtual address.

Changing this layout changes the contract for every reader, in every language.
Treat it as a wire format.

## Publishing

`SpscRing::push` copies the message into `slots[head % 1024]`, then stores
`head + 1` with `memory_order_release`. The release store guarantees the slot
write is visible before the cursor that advertises it, so a consumer can never
read a half-written slot.

The producer owns `head` and reads `tail` only to check for space. Each cursor
sits on its own cache line, so the two cores never invalidate each other's line
on every message.

One process pushes. Multiple consumers each need their own ring.

## Configuration

The knobs live at the top of `apps/ingestion/main.cpp`:

```cpp
constexpr int  kIngestionCore    = 2;
constexpr int  kRealtimePriority = 80;
constexpr auto kHost             = "stream.binance.us";
constexpr auto kPort             = "9443";
constexpr auto kTarget           = "/ws/btcusdt@bookTicker";
```

Check the topology before changing the core, and keep the producer and consumer
off the same hyperthread pair:

```bash
lscpu --extended=CPU,CORE,SOCKET
```

Ring capacity is `SpscRing::kCapacity` in `include/hft/ipc/spsc_ring.hpp`. It is
1024 slots, which is 64KB of messages. Raise it only if bursts legitimately
outrun the consumer; a bigger ring hides a slow reader instead of fixing it.

## Another stream from the same venue

Every Binance stream that carries the same fields works by changing the target
only:

```cpp
constexpr auto kTarget = "/ws/ethusdt@bookTicker";
```

## Another venue

Two pieces are venue specific. Add a parser beside `book_ticker_parser` in
`src/feed/` returning `hft::Message`, then point `WebSocketFeed` at the new host,
port and target. Everything downstream of `parse` stays unchanged, because the
ring only ever sees a `Message`.

For a binary protocol, skip text scanning entirely: reinterpret-cast the receive
buffer into an `alignas(64)` POD matching the venue's byte layout and read fields
through it.

## Verify it is actually fast

1. Confirm the huge page mapping was granted. `HugePages_Free` drops after
   startup.

   ```bash
   grep Huge /proc/meminfo
   ```

2. Confirm the thread is pinned and running `SCHED_FIFO`.

   ```bash
   ps -Lo pid,tid,psr,class,rtprio,comm -C ingestion
   ```

   `psr` must equal `kIngestionCore`, `class` must be `FF`.

3. Read the consumer's histogram. It prints the cycle cost of `pop` every 50
   messages, which includes the cache line transfer from the producer core.

4. Measure the parse in isolation.

   ```bash
   ./build/bench/book_ticker_parser_bench
   ```

## Troubleshooting

**`pthread_setaffinity_np failed for core N`.** The core does not exist or sits
outside the process affinity mask. Check `lscpu` and lower `kIngestionCore`.

**`pthread_setschedparam(SCHED_FIFO, 80) failed, CAP_SYS_NICE is required`.** Run
under `sudo`, or grant the capability once:

```bash
sudo setcap cap_sys_nice+ep ./build/apps/ingestion
```

**`ftruncate failed` or `mmap failed` for `/hft_ring`.** A previous run left an
object with a different size, or `/dev/shm` is full. Remove it and restart:

```bash
rm -f /dev/shm/hft_ring
```

**`bookTicker frame missing key ...`.** The stream is not a `bookTicker` stream,
or the venue changed its field names. Check `kTarget` against the frame printed in
the error.

**`failed to set SNI hostname`.** OpenSSL rejected the host string. Confirm
`kHost` carries no scheme and no path, just the hostname.

**Handshake fails or hangs.** Confirm the port matches the stream style Binance
publishes for that host, and that outbound TLS to the venue is allowed.

**`ring full, dropped update_id=...`.** No consumer is draining, or the consumer
cannot keep up. Start a consumer, verify it is pinned to a different core, and
check it is not blocking on stdout.

**Latencies far worse than the benchmarks.** The huge page mapping silently fell
back to 4KB pages, the core is not isolated, or frequency scaling is on. Work
through the verification steps above.
