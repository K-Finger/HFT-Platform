#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace hft::book
{

/* Converts the exchange's decimal prices and sizes into the integers the book
   works in. Integers are exact, compare and hash without epsilon, and are what a
   hardware risk gate or an FPGA path can consume; doubles are neither.

   Conversion happens once, at the boundary, and never again downstream. */
struct TickScale
{
    std::int64_t  price_ticks_per_unit;      // 100 turns 63501.10 into 6350110 cents
    std::uint64_t quantity_units_per_unit;   // 100000000 turns 1.2 BTC into satoshi

    /* Throws std::domain_error on a non-finite or negative price. */
    std::int64_t price_ticks(double price) const
    {
        if (!std::isfinite(price) || price < 0.0)
            throw std::domain_error("price " + std::to_string(price) +
                                    " is not a finite non-negative number");

        return std::llround(price * static_cast<double>(price_ticks_per_unit));
    }

    /* Throws std::domain_error on a non-finite or negative quantity. */
    std::uint64_t quantity_units(double quantity) const
    {
        if (!std::isfinite(quantity) || quantity < 0.0)
            throw std::domain_error("quantity " + std::to_string(quantity) +
                                    " is not a finite non-negative number");

        return static_cast<std::uint64_t>(
            std::llround(quantity * static_cast<double>(quantity_units_per_unit)));
    }

    double price_from_ticks(std::int64_t ticks) const
    {
        return static_cast<double>(ticks) / static_cast<double>(price_ticks_per_unit);
    }
};

/* Binance USDT pairs quote to the cent and size to eight decimals. */
constexpr TickScale kUsdtPairScale{100, 100'000'000};

}  // namespace hft::book
