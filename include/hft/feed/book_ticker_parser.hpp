#pragma once

#include "hft/core/types.hpp"

namespace hft::feed
{

/* Parses one Binance bookTicker frame. Locates each field by key scan and
   converts it in place: no JSON document, no allocation, no copy of the payload.
   Leaves Message::timestamp at zero, the caller stamps receive time.
   Throws std::runtime_error when a field is missing, rather than publishing a
   zeroed price. */
Message parse_book_ticker(const char* json);

}  // namespace hft::feed
