#include "hft/ipc/spsc_ring.hpp"

#include <benchmark/benchmark.h>

namespace
{

hft::Message sample_message()
{
    hft::Message msg{};
    msg.bid_price = 63501.10;
    msg.ask_price = 63502.45;
    msg.update_id = 1;
    return msg;
}

/* Floor of one IPC hop: producer and consumer on the same core, ring hot in L1.
   A cross-core hop pays the cache line transfer on top of this. */
void BM_PushPopRoundTrip(benchmark::State& state)
{
    hft::ipc::SpscRing ring{};
    const hft::Message msg = sample_message();
    hft::Message out{};

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(ring.push(msg));
        benchmark::DoNotOptimize(ring.pop(out));
    }
}
BENCHMARK(BM_PushPopRoundTrip);

void BM_PushUntilFull(benchmark::State& state)
{
    const hft::Message msg = sample_message();

    for (auto _ : state)
    {
        state.PauseTiming();
        hft::ipc::SpscRing ring{};
        state.ResumeTiming();

        for (std::size_t i = 0; i < hft::ipc::SpscRing::kCapacity; i++)
            benchmark::DoNotOptimize(ring.push(msg));
    }
    state.SetItemsProcessed(state.iterations() * hft::ipc::SpscRing::kCapacity);
}
BENCHMARK(BM_PushUntilFull);

}  // namespace
