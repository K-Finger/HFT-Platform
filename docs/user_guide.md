# User guide

## Prerequisites

- Linux with POSIX shared memory and pthreads
- CMake 3.20+, a C++20 compiler (GCC 11+ or Clang 14+)
- Boost headers and OpenSSL, required only by `hft::feed`
- `CAP_SYS_NICE` or root, to run the apps
- Git submodule `lib/order-book`, required only to build the apps

```bash
sudo apt-get install -y cmake g++ libboost-dev libssl-dev
```

`hft::core`/`hft::ipc` need nothing beyond libc, pthreads, librt. GoogleTest
and Google Benchmark download automatically, only when `HFT_BUILD_TESTS` or
`HFT_BUILD_BENCH` is on.

## Build

```bash
git clone --recurse-submodules https://github.com/K-Finger/binance-data-feed.git
cd binance-data-feed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # -O3 -march=native + LTO
cmake --build build
```

Toggle parts with `-DHFT_BUILD_APPS=OFF`, `-DHFT_BUILD_TESTS=OFF`,
`-DHFT_BUILD_BENCH=OFF` — all `ON` by default at top level, `OFF` when
embedded in another build.

## Tune the host

Skip this and the apps still run; the latency targets just won't hold.

```bash
sudo sysctl -w vm.nr_hugepages=64      # ring maps 2MB pages when available
grep Huge /proc/meminfo                # HugePages_Free must be non-zero
lscpu --extended=CPU,CORE,SOCKET       # check topology before pinning
sudo cpupower frequency-set --governor performance
```

Producer pins to core 2, consumer to core 3 (`kIngestionCore`/`kConsumerCore`
in `apps/`) — keep both off the same hyperthread pair, and isolate them from
the scheduler via the kernel command line: `isolcpus=2,3 nohz_full=2,3
rcu_nocbs=2,3`.

## Run

```bash
sudo ./build/apps/ingestion   # creates the shared memory object
sudo ./build/apps/consumer    # prints mid price + a latency histogram every 50 msgs

sudo setcap cap_sys_nice+ep ./build/apps/ingestion ./build/apps/consumer  # drop the sudo
rm -f /dev/shm/hft_ring       # clear the ring between runs
```

## Use the library in your own project

```cmake
add_subdirectory(binance-data-feed)
target_link_libraries(my_strategy PRIVATE hft::ipc)
```

Or `cmake --install build --prefix /usr/local` and:

```cmake
find_package(hft REQUIRED)
target_link_libraries(my_strategy PRIVATE hft::ipc hft::feed)
```

## Read the ring from another language

Mappable from any language that reproduces the layout: `Message` is four
little-endian `f64` (bid price, ask price, bid qty, ask qty) then two `u64`
(timestamp ns, update id), padded to 64 bytes. The ring is 1024 slots, then
`tail`, then `head`, each cursor alone on a 64-byte line. Load `head` with
acquire ordering before reading a slot; store `tail` with release ordering
after the copy.

## Troubleshooting

**`lib/order-book is empty` during configure.** Submodule not fetched: `git
submodule update --init --recursive`, or build with `-DHFT_BUILD_APPS=OFF`.

**`CAP_SYS_NICE is required`.** Run under `sudo` or grant with `setcap`,
shown above.

**`shm_open(O_RDWR) failed, the producer must create the ring first`.** Start
`ingestion` before `consumer`.

**Consumer prints stale prices on startup.** Previous run left messages in
the ring: `rm -f /dev/shm/hft_ring` and restart the producer.

**Latencies far worse than expected.** Confirm huge pages are granted, cores
are isolated, and the governor is `performance`.
