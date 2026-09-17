# binance-data-feed

[![CI](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml/badge.svg)](https://github.com/K-Finger/binance-data-feed/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Binance `bookTicker` over websocket, decoded into a lock-free shared-memory ring, applied to a live order book on the other side. C++20, Linux only.

## Quick Start

### Prerequisites

- Linux, CMake 3.20+, a C++20 compiler
- Boost and OpenSSL headers
- `CAP_SYS_NICE` or root, to run the binaries at real-time priority

### Install

```sh
sudo apt-get install -y cmake g++ libboost-dev libssl-dev
git clone --recurse-submodules https://github.com/K-Finger/binance-data-feed.git
cd binance-data-feed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Configuration

No env vars. Configuration is CMake flags at build time:

- `HFT_BUILD_APPS` — build the ingestion/consumer/recorder/replay binaries. On by default.
- `HFT_BUILD_TESTS` — build the test suite. On by default.
- `HFT_BUILD_BENCH` — build the microbenchmarks. Off by default.
- `HFT_SANITIZE` — sanitizers to build with, e.g. `address,undefined` or `thread`.

Core IDs are compile-time constants, not runtime config: `kIngestionCore` and `kConsumerCore` in `apps/*/main.cpp`. Set them to cores on your machine before running.

## Usage

Run the pipeline, one process per terminal:

```sh
sudo ./build/apps/ingestion   # connects to Binance, writes into /dev/shm
sudo ./build/apps/consumer    # drains the ring, updates the book, prints the BBO
```

Read the ring from your own process:

```cpp
#include "hft/ipc/shared_memory.hpp"

hft::ipc::SpscRing* ring = hft::ipc::open_shared_ring();

hft::Message msg{};
while (!ring->pop(msg))
    __builtin_ia32_pause();

// msg.bid_price, msg.ask_price, msg.timestamp are ready here
```

Link against one module instead of the whole tree:

```cmake
add_subdirectory(binance-data-feed)
target_link_libraries(your_app PRIVATE hft::ipc)
```
