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

Results are hardware specific and are not checked in yet. Record yours here with
the machine that produced them, so a regression shows up:

- CPU, kernel, compiler:
- Huge pages reserved, core pinned to:
- `BM_Rdtsc`:
- `BM_HistogramRecord`:
- `BM_PushPopRoundTrip`:
- `BM_PushUntilFull`, per item:
- `BM_ParseBookTicker`, per frame:
- Consumer `pop`, 99th percentile cycles:

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
