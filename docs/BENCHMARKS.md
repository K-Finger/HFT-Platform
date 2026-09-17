# Benchmarks

## Prerequisites

- A Release build with `HFT_BUILD_BENCH=ON`
- Huge pages reserved and at least one isolated core, for numbers worth quoting
- `cpupower`, to pin the clock speed

## What each benchmark measures

- `BM_Rdtsc` — overhead of the measurement itself; subtract it from everything else.
- `BM_HistogramRecord` — cost of instrumenting the hot loop.
- `BM_PushPopRoundTrip` — floor of one IPC hop with the ring hot in L1.
- `BM_PushUntilFull` — per-message publish cost across a full ring sweep.
- `BM_ParseBookTicker` — per-frame parse cost, the only work between the
  socket read and the ring push.

The consumer app reports the one figure the benchmarks can't reach: the cycle
cost of `pop` on a live cross-core ring, printed every 50 messages. Quote
that number for the ingestion path — it includes the cache line transfer
between cores.

## Run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build

./build/bench/spsc_ring_bench
./build/bench/book_ticker_parser_bench
./build/bench/tsc_bench
```

For recordable numbers, pin the clock and the core, and repeat the run:

```bash
sudo cpupower frequency-set --governor performance
sudo taskset -c 2 ./build/bench/spsc_ring_bench \
    --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
```

## Results

Results are hardware specific — record yours here with the machine that
produced them, so a regression shows up.

The run below is indicative only: WSL2, no huge pages, no isolated core, no
governor pinned. There is no `cpufreq`/`cpupower` and no `isolcpus` under
WSL2, so a number worth quoting needs a real Linux host.

- CPU, kernel, compiler: i9-13900H, WSL2 5.15, GCC 13.3, `-O3 -march=native` + LTO
- Huge pages / core pinning: none
- `BM_Rdtsc`: 4.95 ns
- `BM_HistogramRecord`: 0.169 ns
- `BM_PushPopRoundTrip`: 1.88 ns
- `BM_PushUntilFull`, per item: 1.70 ns (589 M items/s)
- `BM_ParseBookTicker`, per frame: 174 ns
- `BM_SnapshotBookApply`, per snapshot: 25.8 ns
- `BM_TickScaleConvert`: 0.185 ns
- `BM_Microprice`: 0.095 ns
- Consumer `pop`, 99th percentile: not yet measured on a pinned host

The parse dominates the ingestion path at ~174ns, an order of magnitude
above the ring hop — that's where the next latency work belongs. The book
apply is ~26ns rather than the hundreds of ns the vendored book used to cost,
thanks to its slab allocator.

## Targets

- 99th percentile ingestion path under 50ns.
- Zero TLB misses in the ring loop (huge pages actually granted).
- Zero copies between the socket payload and the ring slot beyond the single
  `Message` write.

## Troubleshooting

**Numbers swing between runs.** Governor is scaling the clock, or the core
is shared — set `performance` and `taskset` to an isolated core.

**Cycle counts don't match nanoseconds.** `rdtsc` counts reference cycles,
not core cycles. Convert with the invariant TSC frequency.
