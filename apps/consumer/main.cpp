/* Consumer process: pinned thread draining the ring into the order book.

   Two histograms rather than one, because the ring hop and the book update are
   separate costs with separate fixes. */

#include "hft/book/snapshot_book.hpp"
#include "hft/ipc/shared_memory.hpp"
#include "hft/sys/affinity.hpp"
#include "hft/time/clock.hpp"
#include "hft/time/latency_histogram.hpp"
#include "hft/time/tsc.hpp"

#include <cstdio>
#include <exception>
#include <iostream>

namespace
{

constexpr int kConsumerCore = 3;
constexpr int kRealtimePriority = 80;
constexpr std::uint64_t kReportInterval = 50;

void report(const hft::book::SnapshotBook& book, const hft::time::LatencyHistogram& pop_cycles,
            const hft::time::LatencyHistogram& apply_cycles,
            const hft::time::LatencyHistogram& wire_latency_ns)
{
    std::printf("\napplied=%llu stale=%llu crossed=%llu trades=%llu filled_quotes=%llu\n",
                static_cast<unsigned long long>(book.applied_count()),
                static_cast<unsigned long long>(book.stale_count()),
                static_cast<unsigned long long>(book.crossed_count()),
                static_cast<unsigned long long>(book.trade_count()),
                static_cast<unsigned long long>(book.filled_quote_count()));

    std::printf("--- ring pop cycles ---\n");
    pop_cycles.print();
    std::printf("--- book apply cycles ---\n");
    apply_cycles.print();
    std::printf("--- exchange-recv-to-applied latency ---\n");
    wire_latency_ns.print("ns");
    std::printf("\n");
}

}  // namespace

int main()
{
    try
    {
        hft::sys::pin_to_core(kConsumerCore);
        hft::sys::request_realtime_priority(kRealtimePriority);

        hft::ipc::SpscRing* ring = hft::ipc::open_shared_ring();

        hft::book::SnapshotBook book(hft::book::kUsdtPairScale);
        hft::time::LatencyHistogram pop_cycles;
        hft::time::LatencyHistogram apply_cycles;
        // TSC deltas are only valid on one core: ingestion and consumer are pinned
        // to different cores, so wire latency is measured with the wall clock
        // stamped into Message.timestamp at recv instead of rdtsc().
        hft::time::LatencyHistogram wire_latency_ns;
        hft::Message msg{};

        while (true)
        {
            const std::uint64_t pop_start = hft::time::rdtsc();
            if (!ring->pop(msg))
            {
                __builtin_ia32_pause();  // spin without starving the sibling hyperthread
                continue;
            }
            const std::uint64_t popped = hft::time::rdtsc();
            pop_cycles.record(popped - pop_start);

            book.apply(msg);
            apply_cycles.record(hft::time::rdtsc() - popped);
            wire_latency_ns.record(hft::time::now_ns() - msg.timestamp);

            const hft::book::TopOfBook& top = book.top();
            std::printf("bid=%.2f ask=%.2f spread=%lld micro=%.4f\n",
                        hft::book::kUsdtPairScale.price_from_ticks(top.bid_ticks),
                        hft::book::kUsdtPairScale.price_from_ticks(top.ask_ticks),
                        static_cast<long long>(top.spread_ticks()), top.microprice_ticks());

            if (pop_cycles.total % kReportInterval == 0)
                report(book, pop_cycles, apply_cycles, wire_latency_ns);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "consumer aborted: " << error.what() << '\n';
        return 1;
    }
}
