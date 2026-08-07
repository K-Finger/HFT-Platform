/* Producer process: pinned thread reading the exchange websocket and publishing
   into the shared memory ring. The only writer of the ring. */

#include "hft/feed/book_ticker_parser.hpp"
#include "hft/feed/websocket_feed.hpp"
#include "hft/ipc/shared_memory.hpp"
#include "hft/sys/affinity.hpp"
#include "hft/time/clock.hpp"

#include <cstdio>
#include <exception>
#include <iostream>

namespace
{

constexpr int kIngestionCore = 2;
constexpr int kRealtimePriority = 80;
constexpr auto kHost = "stream.binance.us";
constexpr auto kPort = "9443";
constexpr auto kTarget = "/ws/btcusdt@bookTicker";

}  // namespace

int main()
{
    try
    {
        hft::sys::pin_to_core(kIngestionCore);
        hft::sys::request_realtime_priority(kRealtimePriority);

        hft::ipc::SpscRing* ring = hft::ipc::create_shared_ring();

        hft::feed::WebSocketFeed feed(kHost, kPort, kTarget);
        feed.connect();
        std::printf("connected to %s%s, streaming into %s\n", kHost, kTarget, hft::ipc::kRingName);

        hft::Message msg{};

        while (true)
        {
            const std::string& frame = feed.read();
            const std::uint64_t received_ns = hft::time::now_ns();  // stamp arrival, not parse cost

            hft::feed::parse_book_ticker(frame.c_str(), msg);
            msg.timestamp = received_ns;

            if (!ring->push(msg))
                std::fprintf(stderr, "ring full, dropped update_id=%llu\n",
                             static_cast<unsigned long long>(msg.update_id));
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "ingestion aborted: " << error.what() << '\n';
        return 1;
    }
}
