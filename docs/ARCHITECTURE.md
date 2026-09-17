# Architecture

## Goal

Isolate the latency-critical leg of the market data path from the
throughput-heavy storage leg. Nothing on the critical leg allocates, blocks
on the kernel, or migrates threads. Everything downstream of shared memory is
allowed to be slow.

## Data flow

```
exchange ──► hft::feed ──► hft::ipc ──► consumer ──► (planned) storage path
             TLS ws        /dev/shm     strategy       Rust daemon, Kafka,
             in-place      SPSC ring    order book     TimescaleDB, TCA
             parse         huge pages   histogram
```

The ring is the seam. Upstream is nanosecond-scale and single-threaded per
core. Downstream may allocate, use any language, and fall behind without
stalling ingestion.

## Modules

- **hft::core** — `Message`, the `alignas(64)` wire type crossing the seam
  (four doubles, a receive timestamp, an exchange sequence number, no
  pointers). `hft::time` gives `rdtsc`, `now_ns`, and `LatencyHistogram`.
- **hft::sys** — pins a thread to a core and raises it to `SCHED_FIFO`. Both
  throw on failure; a silently unpinned thread invalidates every latency
  number.
- **hft::ipc** — `SpscRing`, a 1024-slot single-producer single-consumer ring
  over `/dev/shm`, mapped with `MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE`.
  Producer and consumer cursors sit on separate cache lines; publish is a
  release store, consume is an acquire load.
- **hft::book** — `SnapshotBook` turns each bookTicker frame (state, not
  events) into a replace: cancel the previous synthetic quotes, insert new
  ones. `TickScale` converts the feed's doubles into integer ticks once, at
  the boundary. See [book.md](book.md).
- **hft::feed** — `WebSocketFeed` owns one blocking TLS connection.
  `parse_book_ticker` scans the frame by key and converts in place; a missing
  field throws instead of publishing a zeroed price.
- **apps** — `ingestion` is the sole ring writer; `consumer` pops into the
  order book and histograms the pop and the apply separately; `recorder` and
  `replay` are the capture harness. See [replay.md](replay.md). No app holds
  logic worth testing — that all lives in the library modules.

## Latency budget

Target: 99th percentile ingestion under 50ns, holding only while every step
stays on its fast path.

- Address translation: 0 on a TLB-resident huge page, ~100ns on a page walk.
- Ring slot access: ~1ns from L1, 60–100ns from RAM.
- Cursor handshake: ~1ns uncontended, tens of ns under false sharing.
- Scheduling: 0 pinned + `SCHED_FIFO`, microseconds on a migration.
- Parse: ~100ns key scan, microseconds with a JSON DOM.

[ingestion.md](ingestion.md) develops the reasoning behind each line.

## Invariants

- Nothing on the hot path allocates, locks, or makes a syscall after startup.
- One process writes the ring. Only the ring crosses a process boundary.
- The `Message` layout is a contract shared by every reader, in any language.
- Failures throw with the failing call, the object name, and `errno`.
