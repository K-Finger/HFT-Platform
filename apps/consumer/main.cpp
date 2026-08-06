/* Consumer process: pinned thread draining the ring and reporting the cycle cost
   of each pop, which is the latency a strategy would pay to see an update. */

#include "hft/ipc/shared_memory.hpp"
#include "hft/sys/affinity.hpp"
#include "hft/time/latency_histogram.hpp"
#include "hft/time/tsc.hpp"

#include <cstdio>
#include <exception>
#include <iostream>

namespace
{

constexpr int           kConsumerCore     = 3;
constexpr int           kRealtimePriority = 80;
constexpr std::uint64_t kReportInterval   = 50;

}  // namespace

int main()
{
    try
    {
        hft::sys::pin_to_core(kConsumerCore);
        hft::sys::request_realtime_priority(kRealtimePriority);

        hft::ipc::SpscRing* ring = hft::ipc::open_shared_ring();

        hft::time::LatencyHistogram histogram;
        hft::Message                msg{};

        while (true)
        {
            const std::uint64_t start = hft::time::rdtsc();
            if (!ring->pop(msg))
            {
                __builtin_ia32_pause();  // spin without starving the sibling hyperthread
                continue;
            }
            histogram.record(hft::time::rdtsc() - start);

            const double mid = (msg.bid_price + msg.ask_price) / 2.0;
            std::printf("mid=%.2f bid=%.2f ask=%.2f\n", mid, msg.bid_price, msg.ask_price);

            if (histogram.total % kReportInterval == 0)
            {
                std::printf("\n--- latency histogram (pop cycles) ---\n");
                histogram.print();
                std::printf("--------------------------------------\n\n");
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "consumer aborted: " << error.what() << std::endl;
        return 1;
    }
}
