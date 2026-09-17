# Benchmarks

## Prerequisites

- A Release build with `HFT_BUILD_BENCH=ON`
- Huge pages reserved and at least one isolated core, for numbers worth quoting
- `cpupower`, to pin the clock speed

## What each benchmark measures

`BM_Rdtsc` — overhead of the measurement itself. Subtract it from everything
else.

`BM_HistogramRecord` — cost of instrumenting the hot loop.

`BM_PushPopRoundTrip` — floor of one IPC hop with the ring hot in L1.

`BM_PushUntilFull` — per-message publish cost across a full ring sweep.

`BM_ParseBookTicker` — per-frame parse cost, the only work between the socket
read and the ring push.

The consumer app reports the figure the benchmarks cannot reach: the cycle cost of
`pop` on a live cross-core ring, bucketed by `LatencyHistogram` and printed every
50 messages. Quote that number for the ingestion path, because it includes the
cache line transfer between the producer core and the consumer core.

## Run

1. Build with benchmarks on.

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
   cmake --build build
   ```

2. Run one binary per module.

   ```bash
   ./build/bench/spsc_ring_bench
   ./build/bench/book_ticker_parser_bench
   ./build/bench/tsc_bench
   ```

3. For recordable numbers, pin the clock and the core, and repeat the run.

   ```bash
   sudo cpupower frequency-set --governor performance
   sudo taskset -c 2 ./build/bench/spsc_ring_bench \
       --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
   ```

## Results

Results are hardware specific. Record yours here with the machine that produced
them, so a regression shows up.

The run below is indicative only: it is a WSL2 kernel with no huge pages
reserved, no isolated core and the governor left alone. Treat it as a floor for
relative comparison, not as a number to quote.

WSL2 is a VM, not a bare-metal host, so this gap is not just an unfinished
step: there is no `cpufreq` sysfs and no `cpupower` to lock the clock, and
`isolcpus` is a boot parameter WSL2 does not expose. Reserving huge pages
still works there (`sudo sysctl -w vm.nr_hugepages`), but the clock and core
isolation do not. A number worth quoting needs a real Linux host.

- CPU, kernel, compiler: i9-13900H, WSL2 5.15, GCC 13.3, `-O3 -march=native` + LTO
- Huge pages reserved, core pinned to: none, unpinned
- `BM_Rdtsc`: 4.95 ns
- `BM_HistogramRecord`: 0.169 ns
- `BM_PushPopRoundTrip`: 1.88 ns
- `BM_PushUntilFull`, per item: 1.70 ns (589 M items/s)
- `BM_ParseBookTicker`, per frame: 174 ns
- `BM_SnapshotBookApply`, per snapshot: 25.8 ns
- `BM_TickScaleConvert`: 0.185 ns
- `BM_Microprice`: 0.095 ns
- Consumer `pop`, 99th percentile cycles: not yet measured on a pinned host

Two things stand out. The parse dominates the ingestion path at ~174 ns, an order
of magnitude above the ring hop, so it is where the next latency work belongs.
The book apply is ~26 ns rather than the hundreds of nanoseconds the vendored
book used to cost, because its slab allocator removed the per-order allocation.

## Targets

- 99th percentile ingestion path under 50ns.
- Zero TLB misses in the ring loop, verified by huge pages actually being granted.
- Zero copies between the socket payload and the ring slot beyond the single
  `Message` write.

## Troubleshooting

**Numbers swing between runs.** The governor is scaling the clock or the core is
shared. Set `performance` and `taskset` to an isolated core.

**`BM_PushPopRoundTrip` looks too fast.** The compiler folded the loop. Confirm
`benchmark::DoNotOptimize` still wraps both calls.

**`BM_PushUntilFull` reports the pause cost.** Timing is paused per iteration to
reset the ring. Read the per-item rate, not the wall time.

**Cycle counts do not match nanoseconds.** `rdtsc` counts reference cycles, not
core cycles. Convert with the invariant TSC frequency, not the current clock.
