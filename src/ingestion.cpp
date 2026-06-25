#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "shm_provider.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

static uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1'000'000'000ULL + ts.tv_nsec;
}

static Message parse_bookticker(const char* raw) {
    Message msg{};
    const char* p;

    p = strstr(raw, "\"u\":"); if (p) msg.update_id = strtoull(p + 4, nullptr, 10);
    p = strstr(raw, "\"b\":\""); if (p) msg.bid_price = strtod(p + 5, nullptr);
    p = strstr(raw, "\"B\":\""); if (p) msg.bid_qty   = strtod(p + 5, nullptr);
    p = strstr(raw, "\"a\":\""); if (p) msg.ask_price = strtod(p + 5, nullptr);
    p = strstr(raw, "\"A\":\""); if (p) msg.ask_qty   = strtod(p + 5, nullptr);
    msg.timestamp = now_ns();

    return msg;
}

int main()
{
    try
    {
        RingBuffer* ring = open_shared_memory(true);
        if (!ring) return 1;

        /* Setup I/O engine and TLS configuration */
        net::io_context ioc;
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_default_verify_paths(); 

        /* Host definition used for DNS and SNI */
        std::string const host = "stream.binance.us";

        /* Build socket and resolver */
        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx); // Socket wrapped in TLS
        
        // FIX 1: Use port 443 for individual "/ws/" streams
        auto endpoints = resolver.resolve(host, "9443");
        beast::get_lowest_layer(stream).connect(endpoints);

        /* Set SNI Hostname for TLS routing */
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
                "Failed to set SNI Hostname"
            );
        }

        /* SSL Handshake */
        stream.handshake(ssl::stream_base::client);

        /* Websocket Upgrade */
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws(std::move(stream));
        
        // Add browser user agent decoration
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
            }
        ));

        ws.set_option(websocket::permessage_deflate{});

        ws.handshake(host, "/ws/btcusdt@bookTicker");

        std::cout << "Connected! Streaming data...\n";

        beast::flat_buffer buf;
        while (true) {
            ws.read(buf);
            std::string raw = beast::buffers_to_string(buf.data());
            buf.consume(buf.size());

            Message msg = parse_bookticker(raw.c_str());
            ring->push(msg);

            std::cout << "bid=" << msg.bid_price << " ask=" << msg.ask_price
                      << " spread=" << (msg.ask_price - msg.bid_price)
                      << " ts=" << msg.timestamp << "\n";
        }
    }
    catch (std::exception const& e) {
        std::cerr << "Execution Aborted. Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
