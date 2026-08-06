# Architecture

## Goal

Isolate the latency-critical leg of the market data path from the
throughput-heavy storage leg. The critical leg eliminates kernel context
switches, heap allocation, thread migration and serialisation copies. Everything
downstream of shared memory is allowed to be slow.

## Data flow

```
exchange ──► hft::feed ──► hft::ipc ──► consumer ──► (planned) storage path
             TLS ws        /dev/shm     strategy       Rust daemon, Kafka,
             in-place      SPSC ring    order book     TimescaleDB, TCA
             parse         huge pages   histogram
```

The ring is the seam. Upstream of it everything is measured in nanoseconds and
runs single-threaded per core. Downstream of it processes may be written in any
language, may allocate, and may fall behind without stalling ingestion. A slow
consumer only fills the ring.

## hft::core — data and time

`Message` is the single wire type crossing the seam: four doubles for top of
book, a receive timestamp, and the exchange sequence number. It is `alignas(64)`,
so one message occupies exactly one cache line and neighbouring slots never share
a line. It holds no pointers, which is what makes it valid to place directly
inside a mapping another process reads.

`hft::time` ships the two clocks the platform needs and nothing else. `rdtsc`
gives cycle-accurate hot-path measurement. `now_ns` gives cross-process receive
stamps. `LatencyHistogram` buckets by power of two, and its record path is a
count-leading-zeros plus two increments.

`hft::sys` ships the two OS calls that decide whether any of the above holds:
pinning the thread to a core so its L1 and L2 working set survives, and moving it
to `SCHED_FIFO` so the kernel cannot preempt it for ordinary work. Both throw on
failure. A silently unpinned thread invalidates every latency number the platform
reports.

## hft::ipc — the transport

`SpscRing` is an asymmetric single-producer single-consumer circular buffer over a
fixed 1024-slot inline array. The producer owns `head`, the consumer owns `tail`,
and each cursor sits on its own cache line so the two cores never invalidate each
other's line.

Publication stores the cursor with `memory_order_release` after the slot write.
Consumption loads the cursor with `memory_order_acquire` before reading the slot.
That is the minimum ordering that stops the CPU from reordering the payload past
the cursor.

The mapping comes from `shm_open` on `/hft_ring` plus `ftruncate`, mapped with
`MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE`. Huge pages put the entire ring behind
one TLB entry, so the tight loop never pays a page walk. `MAP_POPULATE` faults the
pages in at startup instead of during the first trade. Operators must reserve huge
pages, so the mapping falls back to 4KB pages when none exist, and fails hard when
even that mapping cannot be made.

## hft::feed — the exchange side

`WebSocketFeed` owns one blocking TLS websocket connection, driven by the pinned
producer thread.

`parse_book_ticker` locates each field by key scan and converts it in place. A
frame never becomes a JSON document and never gets copied field by field into an
intermediate object. Missing fields throw instead of publishing a zeroed price.

The caller stamps receive time immediately after the socket read, before parsing,
so the timestamp measures arrival rather than parse cost.

## apps — the processes

`ingestion` pins itself, creates the ring, connects the feed and pushes. It is the
only writer.

`consumer` pins itself to a different core, opens the ring, pops, and records the
pop cost into the histogram.

Neither app holds logic worth testing. All of it lives in the library modules.

## Latency budget

The target for the ingestion block is a 99th percentile under 50ns. It holds only
while every step below stays on its fast path.

- Address translation: 0 on a TLB-resident huge page, roughly 100ns on a page
  walk.
- Ring slot access: about 1ns from L1, 60 to 100ns from RAM.
- Cursor handshake: about 1ns uncontended, tens of nanoseconds under false
  sharing.
- Scheduling: 0 when pinned and running `SCHED_FIFO`, microseconds on a
  migration.
- Parse: roughly 100ns of key scan with no allocation, microseconds with a JSON
  DOM.

[ingestion.md](ingestion.md) develops the reasoning behind each line.

## Invariants

- Nothing on the hot path allocates, locks or makes a syscall after startup.
- One process writes the ring. Only the ring crosses a process boundary.
- The `Message` layout is a contract shared by every reader, in any language.
- Failures throw with the failing call, the object name and `errno`. No fallback
  hides a misconfigured host.
