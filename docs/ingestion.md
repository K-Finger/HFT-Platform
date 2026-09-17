# Ingestion

Turns exchange frames into cache-line sized messages in shared memory: one
pinned thread, no allocation after startup, no syscall between the socket
read and the ring push.

## Prerequisites

- Boost headers and OpenSSL, for the TLS websocket
- `CAP_SYS_NICE` or root, for `SCHED_FIFO`
- One core the scheduler is not oversubscribing
- Huge pages reserved, to keep the ring behind a single TLB entry

## The path of one message

1. `WebSocketFeed::read` blocks on the TLS socket, returns the decoded frame.
2. `hft::time::now_ns` stamps arrival, before parsing.
3. `parse_book_ticker` scans the frame's five keys and converts in place into
   a `Message`.
4. The caller writes the stamp into `Message::timestamp`.
5. `SpscRing::push` copies into the next slot and releases the head cursor.

Step 1 blocks for as long as the exchange takes; steps 2–5 hold the latency
budget.

## Setup and loop

`apps/ingestion/main.cpp` pins the thread, takes real-time priority, creates
the ring, then connects — in that order, each throwing on failure, so a
misconfigured host fails before it ever opens a socket:

```cpp
hft::sys::pin_to_core(kIngestionCore);
hft::sys::request_realtime_priority(kRealtimePriority);
hft::ipc::SpscRing* ring = hft::ipc::create_shared_ring();

hft::feed::WebSocketFeed feed(kHost, kPort, kTarget);
feed.connect();

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

`msg` is declared once and reused so its 64-byte alignment is honoured and
it's hoisted out of the loop. A failed push means the consumer is falling
behind — the producer reports it and moves on; it never blocks, retries, or
grows the buffer.

## Parsing

`parse_book_ticker` reads a Binance `bookTicker` frame —
`{"u":400900217,"s":"BTCUSDT","b":"63501.10","B":"1.20","a":"63502.45","A":"3.40"}`
— locating each key with `strstr` and converting in place (`strtoull`,
`strtod`): no JSON document, no allocation, no intermediate copy. A missing
key throws `std::runtime_error` rather than publishing a zeroed price.
`timestamp` is left at zero; stamping is the caller's job.

## The message contract

The only type crossing into shared memory — treat as a wire format shared by
every reader, in every language:

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

One message per cache line, no pointers, so it's valid in a mapping another
process reads at a different virtual address.

## Publishing

`SpscRing::push` copies into `slots[head % 1024]`, then stores `head + 1`
with `memory_order_release`. The producer owns `head`; each cursor sits on
its own cache line. One process pushes — multiple consumers each need their
own ring.

## Configuration

```cpp
constexpr int  kIngestionCore    = 2;
constexpr int  kRealtimePriority = 80;
constexpr auto kHost             = "stream.binance.us";
constexpr auto kPort             = "9443";
constexpr auto kTarget           = "/ws/btcusdt@bookTicker";
```

in `apps/ingestion/main.cpp`. Check topology first
(`lscpu --extended=CPU,CORE,SOCKET`) and keep producer/consumer off the same
hyperthread pair. Ring capacity is `SpscRing::kCapacity` in
`include/hft/ipc/spsc_ring.hpp` (1024 slots) — raise only if bursts
legitimately outrun the consumer.

Same venue, another stream: change `kTarget` only. New venue: add a parser
beside `book_ticker_parser` returning `hft::Message`, then point
`WebSocketFeed` at the new host/port/target — everything downstream of
`parse` is unchanged. Binary protocol: reinterpret-cast the receive buffer
into an `alignas(64)` POD instead of text-scanning.

## Verify it is actually fast

```bash
grep Huge /proc/meminfo                                # HugePages_Free drops after startup
ps -Lo pid,tid,psr,class,rtprio,comm -C ingestion       # psr = kIngestionCore, class = FF
./build/bench/book_ticker_parser_bench                 # parse in isolation
```

The consumer's histogram prints the cycle cost of `pop` every 50 messages,
including the cache line transfer from the producer core.

## Troubleshooting

**`CAP_SYS_NICE is required`.** Run under `sudo`, or
`sudo setcap cap_sys_nice+ep ./build/apps/ingestion`.

**`ftruncate`/`mmap failed` for `/hft_ring`.** Stale object or full
`/dev/shm`: `rm -f /dev/shm/hft_ring`.

**`ring full, dropped update_id=...`.** No consumer draining, or it can't
keep up — start one, pinned to a different core.

**Latencies far worse than the benchmarks.** Huge pages fell back to 4KB,
the core isn't isolated, or frequency scaling is on.
