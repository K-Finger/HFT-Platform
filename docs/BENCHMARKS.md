# Benchmarks

Needs a Release build with `HFT_BUILD_BENCH=ON`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build
./build/bench/spsc_ring_bench
./build/bench/book_ticker_parser_bench
```

## Results

i9-13900H, WSL2 5.15, GCC 13.3, `-O3 -march=native` + LTO. Unpinned, no huge
pages — WSL2 exposes no `cpufreq` and no `isolcpus`, so treat these as a floor
for relative comparison, not numbers to quote.

- Ring push/pop round trip: 1.88 ns
- Ring push, full sweep: 1.70 ns/item (589M items/s)
- Parse one `bookTicker` frame: 174 ns
- Apply one snapshot to the book: 25.8 ns
