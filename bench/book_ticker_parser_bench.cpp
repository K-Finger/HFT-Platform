#include "hft/feed/book_ticker_parser.hpp"

#include "fixtures/frames.hpp"

#include <benchmark/benchmark.h>

#include <cstring>

namespace
{

/* Per frame parse cost, the only work between the socket read and the ring push. */
void BM_ParseBookTicker(benchmark::State& state)
{
    for (auto _ : state)
        benchmark::DoNotOptimize(hft::feed::parse_book_ticker(hft::bench::kBookTickerFrame));

    state.SetBytesProcessed(state.iterations() *
                            static_cast<std::int64_t>(std::strlen(hft::bench::kBookTickerFrame)));
}
BENCHMARK(BM_ParseBookTicker);

}  // namespace
