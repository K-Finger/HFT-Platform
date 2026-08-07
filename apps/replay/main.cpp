/* Replays a capture file through the live parser into the shared ring at full
   speed, and reports the cycle cost of parse plus push per record.

   Fixed input makes the number reproducible: the same file replays to the same
   work every run, so a change in the histogram is a change in the code. */

#include "hft/capture/capture_reader.hpp"
#include "hft/feed/book_ticker_parser.hpp"
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

constexpr int kReplayCore = 2;
constexpr int kRealtimePriority = 80;

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s <capture.hftc>\n", argv[0]);
        return 2;
    }

    try
    {
        hft::sys::pin_to_core(kReplayCore);
        hft::sys::request_realtime_priority(kRealtimePriority);

        const hft::capture::CaptureReader reader(argv[1]);
        hft::ipc::SpscRing* ring = hft::ipc::create_shared_ring();

        hft::time::LatencyHistogram histogram;
        std::uint64_t dropped = 0;

        hft::Message msg{};

        const std::uint64_t started_ns = hft::time::now_ns();
        for (const hft::capture::CaptureRecord& record : reader)
        {
            const std::uint64_t start = hft::time::rdtsc();

            hft::feed::parse_book_ticker(record.payload, msg);
            msg.timestamp = record.timestamp_ns;  // keep the original receive time
            if (!ring->push(msg))
                dropped++;

            histogram.record(hft::time::rdtsc() - start);
        }
        const std::uint64_t elapsed_ns = hft::time::now_ns() - started_ns;

        const double elapsed_s = static_cast<double>(elapsed_ns) / 1e9;
        std::printf("replayed %llu records from %s in %.3f s, %.0f records/s, %llu dropped\n",
                    static_cast<unsigned long long>(reader.record_count()), argv[1], elapsed_s,
                    static_cast<double>(reader.record_count()) / elapsed_s,
                    static_cast<unsigned long long>(dropped));

        std::printf("\n--- latency histogram (parse + push cycles) ---\n");
        histogram.print();
    }
    catch (const std::exception& error)
    {
        std::cerr << "replay aborted: " << error.what() << '\n';
        return 1;
    }
}
