#pragma once

#include "hft/core/types.hpp"

namespace hft::feed
{

/* Parses one Binance bookTicker frame into out. Locates each field by key scan
   and converts it in place: no JSON document, no allocation, no copy of the
   payload. Leaves Message::timestamp at zero, the caller stamps receive time.
   Throws std::runtime_error when a field is missing, rather than publishing a
   zeroed price; out is left partially written in that case.

   Message is over-aligned to a cache line, and the ABI only guarantees the stack
   16-byte aligned. Returning one by value therefore makes the compiler emit an
   aligned vector store into a slot it did not align, which faults depending on
   where the stack happens to sit. Filling a caller-owned object avoids that:
   a named Message gets its alignment honoured. */
void parse_book_ticker(const char* json, Message& out);

}  // namespace hft::feed
