# binance-data-feed

[![CI](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml/badge.svg)](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Binance `bookTicker` over websocket, decoded into a lock-free shared-memory ring, applied to a live order book. C++20, Linux only.

## Quick Start

Needs CMake 3.20+, a C++20 compiler, Boost, and OpenSSL. Run the binaries with `CAP_SYS_NICE` or root.

```sh
git clone --recurse-submodules https://github.com/K-Finger/binance-data-feed.git
cd binance-data-feed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Configuration

No env vars. Build-time CMake flags only: `HFT_BUILD_APPS`, `HFT_BUILD_TESTS`, `HFT_BUILD_BENCH` toggle what gets built, `HFT_SANITIZE=address,undefined` or `thread` builds with sanitizers. Core pinning is a constant, not a setting — edit `kIngestionCore`/`kConsumerCore` in `apps/*/main.cpp` for your machine.

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
