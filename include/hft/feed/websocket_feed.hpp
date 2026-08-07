#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <string>

namespace hft::feed
{

/* Blocking TLS websocket client for one exchange stream. Not thread-safe: one
   instance is driven by one pinned thread. Failures surface as
   boost::system::system_error. */
class WebSocketFeed
{
  public:
    WebSocketFeed(std::string host, std::string port, std::string target);

    /* Resolves the host, completes the TLS handshake, upgrades to websocket. */
    void connect();

    /* Blocks until the next frame arrives. The payload is owned by this feed and
       stays valid until the next read. */
    const std::string& read();

  private:
    std::string host_;
    std::string port_;
    std::string target_;

    boost::asio::io_context io_context_;
    boost::asio::ssl::context ssl_context_;

    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> websocket_;
    boost::beast::flat_buffer buffer_;
    std::string payload_;
};

}  // namespace hft::feed
