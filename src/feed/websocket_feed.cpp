#include "hft/feed/websocket_feed.hpp"

#include "hft/core/version.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <string>
#include <utility>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace hft::feed
{

WebSocketFeed::WebSocketFeed(std::string host, std::string port, std::string target)
    : host_(std::move(host)), port_(std::move(port)), target_(std::move(target)),
      ssl_context_(ssl::context::tlsv12_client), websocket_(io_context_, ssl_context_)
{
    ssl_context_.set_default_verify_paths();
}

void WebSocketFeed::connect()
{
    tcp::resolver resolver(io_context_);
    auto& tls_stream = websocket_.next_layer();

    beast::get_lowest_layer(tls_stream).connect(resolver.resolve(host_, port_));

    // SNI hostname, without it the exchange cannot route the TLS session
    if (!SSL_set_tlsext_host_name(tls_stream.native_handle(), host_.c_str()))
        throw beast::system_error(
            beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
            "failed to set SNI hostname " + host_);

    tls_stream.handshake(ssl::stream_base::client);

    websocket_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& request)
        {
            request.set(http::field::user_agent,
                        std::string("binance-data-feed/") + kVersionString);
        }));
    websocket_.set_option(websocket::permessage_deflate{});

    websocket_.handshake(host_, target_);
}

const std::string& WebSocketFeed::read()
{
    buffer_.consume(buffer_.size());
    websocket_.read(buffer_);
    payload_ = beast::buffers_to_string(buffer_.data());

    return payload_;
}

}  // namespace hft::feed
