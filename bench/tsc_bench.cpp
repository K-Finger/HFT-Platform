#include "hft/time/latency_histogram.hpp"
#include "hft/time/tsc.hpp"

#include <benchmark/benchmark.h>

namespace
{

/* Overhead of the measurement itself. Subtract it from every other number. */
void BM_Rdtsc(benchmark::State& state)
{
    for (auto _ : state)
        benchmark::DoNotOptimize(hft::time::rdtsc());
}
BENCHMARK(BM_Rdtsc);

void BM_HistogramRecord(benchmark::State& state)
{
    hft::time::LatencyHistogram histogram;
    std::uint64_t               cycles = 1;

    for (auto _ : state)
    {
        histogram.record(cycles++);
        benchmark::DoNotOptimize(histogram.total);
    }
}
BENCHMARK(BM_HistogramRecord);

}  // namespace
