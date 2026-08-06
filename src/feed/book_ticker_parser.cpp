#include "hft/feed/book_ticker_parser.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace hft::feed
{
namespace
{

/* Returns the first character of the value following key. */
const char* find_value(const char* json, const char* key)
{
    const char* found = std::strstr(json, key);
    if (found == nullptr)
        throw std::runtime_error("bookTicker frame missing key " + std::string(key) + ": " + json);

    return found + std::strlen(key);
}

}  // namespace

Message parse_book_ticker(const char* json)
{
    Message msg{};
    msg.update_id = std::strtoull(find_value(json, "\"u\":"), nullptr, 10);
    msg.bid_price = std::strtod(find_value(json, "\"b\":\""), nullptr);
    msg.bid_qty   = std::strtod(find_value(json, "\"B\":\""), nullptr);
    msg.ask_price = std::strtod(find_value(json, "\"a\":\""), nullptr);
    msg.ask_qty   = std::strtod(find_value(json, "\"A\":\""), nullptr);

    return msg;
}

}  // namespace hft::feed
