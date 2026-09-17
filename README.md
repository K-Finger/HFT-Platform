# binance-data-feed

[![CI](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml/badge.svg)](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A low-latency market data pipeline for Binance. A pinned, allocation-free
producer thread decodes `bookTicker` frames off a websocket and publishes them
into a lock-free shared-memory ring; a consumer process pops from the ring and
applies each update to a live order book. C++20, Linux only.

## Architecture

```
exchange ──► hft::feed ──► hft::ipc ──► consumer ──► order book
             TLS ws        /dev/shm     strategy       (BBO, histograms)
             in-place      SPSC ring
             parse         huge pages
```

The shared-memory ring is the seam between the two legs. Upstream of it,
everything runs single-threaded per core, allocates nothing, and never makes a
syscall after startup. Downstream of it, a slow consumer only fills the ring —
it never stalls ingestion.

- **`hft::feed`** — owns the TLS websocket connection and parses each frame
  in place by key scan, with no intermediate JSON document.
- **`hft::ipc`** — an SPSC ring over a huge-page mapping. Producer and
  consumer cursors sit on separate cache lines; publication and consumption
  are ordered with a release/acquire handshake.
- **`hft::book`** — applies bookTicker replace frames to a two-level order
  book and exposes the BBO.
- **`hft::core` / `hft::sys`** — the shared `Message` wire type, cycle-accurate
  timing, and the core-pinning / `SCHED_FIFO` calls that make the rest of this
  hold.

Target for the ingestion path is a 99th-percentile latency under 50ns,
measured end to end from socket read to ring pop.

## Quick Start

Needs CMake 3.20+, a C++20 compiler, Boost, and OpenSSL. Run the binaries with
`CAP_SYS_NICE` or root.

```sh
git clone --recurse-submodules https://github.com/K-Finger/binance-data-feed.git
cd binance-data-feed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Configuration

No env vars. Build-time CMake flags only: `HFT_BUILD_APPS`, `HFT_BUILD_TESTS`,
`HFT_BUILD_BENCH` toggle what gets built, `HFT_SANITIZE=address,undefined` or
`thread` builds with sanitizers. Core pinning is a constant, not a setting —
edit `kIngestionCore`/`kConsumerCore` in `apps/*/main.cpp` for your machine.

## Usage

```sh
sudo ./build/apps/ingestion   # Binance -> /dev/shm ring
sudo ./build/apps/consumer    # ring -> order book, prints the BBO
```

To read the ring from your own code:

```cpp
hft::ipc::SpscRing* ring = hft::ipc::open_shared_ring();
hft::Message msg{};
while (!ring->pop(msg)) __builtin_ia32_pause();
```

`recorder` and `replay` round out the toolset: `recorder` captures raw frames
to a file, `replay` pushes a capture back through the live parser into the
ring at full speed, for reproducing a run offline.

## Benchmarks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build
./build/bench/spsc_ring_bench
./build/bench/book_ticker_parser_bench
./build/bench/tsc_bench
```

For numbers worth quoting, pin the CPU governor and an isolated core:

```sh
sudo cpupower frequency-set --governor performance
sudo taskset -c 2 ./build/bench/spsc_ring_bench \
    --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
```

Results are hardware-specific; a WSL2 host has no `cpupower` or `isolcpus` and
its numbers should be treated as a relative floor, not a figure to quote.

## License

MIT — see [LICENSE](LICENSE).
