# HFT-Platform

[![CI](https://github.com/K-Finger/HFT-Platform/actions/workflows/ci.yml/badge.svg)](https://github.com/K-Finger/HFT-Platform/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

Modular C++20 building blocks for low-latency market data ingestion on Linux.

The library ships a lock-free SPSC ring in huge-page shared memory, a zero-copy
exchange feed parser, and the OS primitives that keep the hot path off the kernel
and inside L1: core pinning, `SCHED_FIFO`, TSC timing.

Every module is a separate CMake target. Take the ring, take the feed, or take
the whole platform.

## Features

- **Lock-free SPSC ring.** 1024 inline slots, acquire/release cursors on separate
  cache lines. No locks, no allocation, no syscalls on the hot path.
- **Huge-page shared memory.** POSIX `shm_open` object mapped with `MAP_HUGETLB`
  and `MAP_POPULATE`, so the whole ring costs one TLB entry.
- **Zero-copy parsing.** Binance `bookTicker` frames convert in place into a
  cache-line aligned POD. No JSON document, no intermediate object.
- **Reproducible replay.** Record raw frames to a capture file, then replay them
  through the same parser and ring at full speed. Fixed input, comparable numbers.
- **Live order book.** Snapshots drive a limit order book through integer ticks,
  with best bid and offer, spread and size-weighted microprice on the read side.
- **Latency-critical OS setup.** Core pinning and real-time priority that fail
  loudly instead of running unpinned and reporting meaningless numbers.
- **TSC instrumentation.** `rdtsc` reads and a power-of-two histogram cheap enough
  to leave inside the measured loop.
- **Contained dependencies.** Core and IPC need libc, pthreads and librt. Boost
  and OpenSSL stay inside the feed module.

## Modules

`hft::core` — header only. `Message` POD, version, wall clock, TSC, latency
histogram, core pinning and `SCHED_FIFO`. Depends on pthreads.

`hft::ipc` — `SpscRing` plus the shared memory mapping. Depends on `hft::core`
and librt.

`hft::capture` — `CaptureWriter` and the mmapped `CaptureReader` behind the replay
harness. Depends on `hft::core`.

`hft::book` — `SnapshotBook`, `TopOfBook` and the `TickScale` integer boundary.
Depends on `hft::core` and the vendored order book submodule.

`hft::feed` — `WebSocketFeed` and `parse_book_ticker`. Depends on `hft::core`,
Boost and OpenSSL.

## Architecture

```
[ hft::feed ]            [ hft::ipc ]              [ consumers ]
websocket + parser  ──►  shared memory ring   ──►  strategy / logging / analytics
├─ TLS websocket         ├─ /dev/shm/hft_ring      ├─ lock-free reader
├─ in-place parse        ├─ 2MB huge pages         ├─ TSC latency histogram
└─ receive timestamp     └─ acquire / release      └─ limit order book
                                                        │
                         [ hft::core ]                  ▼
                         POD types, clocks,        (planned) Rust daemon,
                         pinning, SCHED_FIFO        Kafka, TimescaleDB
```

The ring is the seam. Upstream runs in nanoseconds, single-threaded per core.
Downstream may allocate and fall behind — a slow consumer only fills the ring.

## Layout

```
include/hft/         Public headers, the only thing consumers include
├── core/            Message POD, version
├── ipc/             SPSC ring, shared memory mapping
├── book/            Snapshot driven order book, top of book, tick scale
├── capture/         Capture file format, writer, mmap reader
├── feed/            Exchange websocket client, wire parsers
├── sys/             Core pinning, real-time scheduling, descriptor guard
└── time/            TSC, wall clock, latency histogram
src/                 Implementation, mirrors include/hft
apps/                Runnable processes built on the library
├── ingestion/       Producer: websocket to shared memory
├── consumer/        Consumer: ring into the order book plus latency report
├── recorder/        Writes raw frames to a capture file
└── replay/          Replays a capture through the parser into the ring
tests/               GoogleTest unit tests, one binary per module
bench/               Google Benchmark microbenchmarks
cmake/               Package config and shared target options
docs/                Architecture, user guide, benchmarks, deep dives
lib/                 Third-party submodules (limit order book)
```

## Prerequisites

- Linux with POSIX shared memory
- CMake 3.20 or newer, a C++20 compiler
- Boost headers and OpenSSL, for `hft::feed` only
- `CAP_SYS_NICE`, to run the apps with real-time priority

## Quick start

1. Install the dependencies.

   ```bash
   sudo apt-get install -y cmake g++ libboost-dev libssl-dev
   ```

2. Clone with submodules.

   ```bash
   git clone --recurse-submodules https://github.com/K-Finger/HFT-Platform.git
   cd HFT-Platform
   ```

3. Build.

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

4. Start the producer. It creates `/dev/shm/hft_ring`.

   ```bash
   sudo ./build/apps/ingestion
   ```

5. Start the consumer on another terminal.

   ```bash
   sudo ./build/apps/consumer
   ```

Host tuning — huge pages, isolated cores — is in
[docs/user_guide.md](docs/user_guide.md).

## Usage

```cpp
#include "hft/ipc/shared_memory.hpp"
#include "hft/time/clock.hpp"

hft::ipc::SpscRing* ring = hft::ipc::create_shared_ring();

hft::Message msg{};
msg.bid_price = 63501.10;
msg.ask_price = 63502.45;
msg.timestamp = hft::time::now_ns();

if (!ring->push(msg))
    /* ring is full, the consumer is falling behind */;
```

Read the other side:

```cpp
hft::ipc::SpscRing* ring = hft::ipc::open_shared_ring();

hft::Message msg{};
while (!ring->pop(msg))
    __builtin_ia32_pause();
```

## Integration

Embed the source tree:

```cmake
add_subdirectory(HFT-Platform)
target_link_libraries(my_app PRIVATE hft::ipc hft::feed)
```

Or install once and consume the package:

```bash
cmake --install build --prefix /usr/local
```

```cmake
find_package(hft REQUIRED)
target_link_libraries(my_app PRIVATE hft::ipc)
```

Build options `HFT_BUILD_APPS`, `HFT_BUILD_TESTS` and `HFT_BUILD_BENCH` default
to `ON` at top level and `OFF` when the project is embedded.

## Tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Benchmarks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build
./build/bench/spsc_ring_bench
```

Methodology and the numbers to record live in
[docs/BENCHMARKS.md](docs/BENCHMARKS.md).

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — component design and latency budget
- [Ingestion](docs/ingestion.md) — the latency-critical path in detail
- [Order book](docs/book.md) — snapshots into orders, integer ticks, book cost
- [Replay](docs/replay.md) — recording and replaying captures for reproducible
  numbers
- [User guide](docs/user_guide.md) — build, host tuning, running the apps
- [Benchmarks](docs/BENCHMARKS.md) — how latency is measured and reported

## References

- [liborderbook](https://github.com/K-Finger/liborderbook) — the matching engine
  this platform feeds
- [Binance websocket streams](https://developers.binance.com/docs/binance-spot-api-docs/websocket-streams)
- [Linux huge pages](https://docs.kernel.org/admin-guide/mm/hugetlbpage.html)

## License

MIT — see [LICENSE](LICENSE).
