# binance-data-feed

[![CI](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml/badge.svg)](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

A C++20 pipeline for low-latency market data on Linux: exchange websocket in, shared-memory ring out, order book on the read side. Every module is a separate CMake target, so a consumer can take the ring without the feed, or the whole pipeline.

## Contents

- Lock-free SPSC ring in huge-page shared memory, cursors on separate cache lines
- Zero-copy Binance `bookTicker` parsing straight into a cache-line aligned POD
- Capture and replay, so a latency regression is reproducible against fixed input
- Snapshot-driven order book with top of book, spread and size-weighted microprice
- Core pinning and `SCHED_FIFO` that throw instead of running unpinned and lying about the numbers
- TSC timing and a power-of-two latency histogram cheap enough to leave in the hot loop

## Architecture

```
exchange ──► hft::feed ──► hft::ipc ──► consumer ──► (planned) storage path
             TLS websocket    /dev/shm ring   order book     Rust daemon, Kafka,
             in-place parse   2MB huge pages   histogram      TimescaleDB, TCA
```

The ring is the seam. Upstream of it, everything runs single-threaded per core
and is measured in nanoseconds. Downstream of it, a consumer can allocate, log,
or fall behind — a slow consumer only fills the ring, it never stalls ingestion.

## Build

```sh
sudo apt-get install -y cmake g++ libboost-dev libssl-dev
git clone --recurse-submodules https://github.com/K-Finger/binance-data-feed.git
cd binance-data-feed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```sh
sudo ./build/apps/ingestion   # websocket -> /dev/shm ring
sudo ./build/apps/consumer    # ring -> order book, latency histogram
```

Host tuning — huge pages, isolated cores — is in [docs/user_guide.md](docs/user_guide.md).

## Use

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

```cpp
hft::ipc::SpscRing* ring = hft::ipc::open_shared_ring();

hft::Message msg{};
while (!ring->pop(msg))
    __builtin_ia32_pause();
```

## Modules

| Target | Provides | Depends on |
|---|---|---|
| `hft::core` | `Message` POD, clocks, core pinning, `SCHED_FIFO` | pthreads |
| `hft::ipc` | `SpscRing`, huge-page shared memory mapping | `hft::core`, librt |
| `hft::capture` | Capture writer, mmapped reader | `hft::core` |
| `hft::book` | Snapshot-driven book, top of book, tick scale | `hft::core`, vendored order book |
| `hft::feed` | Websocket client, `bookTicker` parser | `hft::core`, Boost, OpenSSL |

## Integrate

```cmake
add_subdirectory(binance-data-feed)
target_link_libraries(your_app PRIVATE hft::ipc hft::feed)
```

Or install and use `find_package`:

```sh
cmake --install build --prefix /usr/local
```

```cmake
find_package(hft REQUIRED)
target_link_libraries(your_app PRIVATE hft::ipc)
```

## Testing

40 GoogleTest cases across six binaries, one per module, so a failure points at
a single component instead of the whole tree.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

CI builds Release and Debug on every push, runs the suite under both, runs it
again under AddressSanitizer+UndefinedBehaviorSanitizer and separately under
ThreadSanitizer, and gates on clang-format and clang-tidy with warnings as
errors.

## Benchmarks

Single core, WSL2, GCC `-O3` — a floor for relative comparison, not a number to
quote. Numbers worth quoting need huge pages reserved and an isolated, pinned
core; methodology and how to get there are in [docs/BENCHMARKS.md](docs/BENCHMARKS.md).

| Operation | Latency |
|---|---|
| Ring push/pop round trip | 1.9 ns |
| Parse one `bookTicker` frame | 174 ns |
| Apply one snapshot to the book | 26 ns |

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build
./build/bench/spsc_ring_bench
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Ingestion](docs/ingestion.md)
- [Order book](docs/book.md)
- [Replay](docs/replay.md)
- [User guide](docs/user_guide.md)
- [Benchmarks](docs/BENCHMARKS.md)

## References

- [liborderbook](https://github.com/K-Finger/liborderbook) - the matching engine vendored under `lib/order-book`
- [Binance websocket streams](https://developers.binance.com/docs/binance-spot-api-docs/websocket-streams)
- [Linux huge pages](https://docs.kernel.org/admin-guide/mm/hugetlbpage.html)

## License

MIT
