/* Records raw exchange frames to a capture file for later replay.

   Runs as its own process with its own connection, unpinned and without
   real-time priority: disk writes must never land in the ingestion thread. */

#include "hft/capture/capture_writer.hpp"
#include "hft/feed/websocket_feed.hpp"
#include "hft/time/clock.hpp"

#include <csignal>
#include <cstdio>
#include <exception>
#include <iostream>

namespace
{

constexpr auto kHost   = "stream.binance.us";
constexpr auto kPort   = "9443";
constexpr auto kTarget = "/ws/btcusdt@bookTicker";

volatile std::sig_atomic_t g_stop = 0;

void request_stop(int)
{
    g_stop = 1;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s <capture.hftc>\n", argv[0]);
        return 2;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    try
    {
        hft::capture::CaptureWriter writer(argv[1]);

        hft::feed::WebSocketFeed feed(kHost, kPort, kTarget);
        feed.connect();
        std::printf("recording %s%s to %s, stop with ctrl-c\n", kHost, kTarget, argv[1]);

        while (g_stop == 0)
        {
            const std::string& frame = feed.read();
            writer.append(hft::time::now_ns(), frame);
        }

        writer.close();
        std::printf("recorded %llu frames to %s\n",
                    static_cast<unsigned long long>(writer.record_count()), argv[1]);
    }
    catch (const std::exception& error)
    {
        std::cerr << "recorder aborted: " << error.what() << std::endl;
        return 1;
    }
}
