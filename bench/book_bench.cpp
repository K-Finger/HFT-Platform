#include "hft/book/snapshot_book.hpp"
#include "hft/book/tick_scale.hpp"
#include "hft/book/top_of_book.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>

namespace
{

hft::Message snapshot(std::uint64_t update_id)
{
    // Walk the price so every apply is a real replace at a new level.
    const double drift = static_cast<double>(update_id % 64) * 0.01;

    hft::Message msg{};
    msg.bid_price = 63501.10 + drift;
    msg.bid_qty   = 1.2;
    msg.ask_price = 63502.45 + drift;
    msg.ask_qty   = 3.4;
    msg.timestamp = update_id;
    msg.update_id = update_id;
    return msg;
}

/* Cost of one snapshot: two cancels, two inserts, and the integer conversion.
   The pinned book allocates per order, so expect this to dominate the ring hop. */
void BM_SnapshotBookApply(benchmark::State& state)
{
    hft::book::SnapshotBook book(hft::book::kUsdtPairScale);
    std::uint64_t           update_id = 1;

    for (auto _ : state)
    {
        book.apply(snapshot(update_id++));
        benchmark::DoNotOptimize(book.top());
    }
}
BENCHMARK(BM_SnapshotBookApply);

/* Conversion alone, to separate it from the book work above. */
void BM_TickScaleConvert(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(hft::book::kUsdtPairScale.price_ticks(63501.10));
        benchmark::DoNotOptimize(hft::book::kUsdtPairScale.quantity_units(1.2));
    }
}
BENCHMARK(BM_TickScaleConvert);

void BM_Microprice(benchmark::State& state)
{
    const hft::book::TopOfBook top{6'350'110, 6'350'245, 120'000'000, 340'000'000};

    for (auto _ : state)
        benchmark::DoNotOptimize(top.microprice_ticks());
}
BENCHMARK(BM_Microprice);

}  // namespace
