#include "hft/book/snapshot_book.hpp"
#include "hft/book/tick_scale.hpp"
#include "hft/book/top_of_book.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

using hft::book::kUsdtPairScale;
using hft::book::SnapshotBook;
using hft::book::TickScale;
using hft::book::TopOfBook;

namespace
{

hft::Message snapshot(std::uint64_t update_id, double bid, double bid_qty, double ask,
                      double ask_qty)
{
    hft::Message msg{};
    msg.bid_price = bid;
    msg.bid_qty = bid_qty;
    msg.ask_price = ask;
    msg.ask_qty = ask_qty;
    msg.timestamp = 1'000 * update_id;
    msg.update_id = update_id;
    return msg;
}

}  // namespace

TEST(TickScale, ConvertsPricesToTicks)
{
    EXPECT_EQ(kUsdtPairScale.price_ticks(63501.10), 6'350'110);
    EXPECT_EQ(kUsdtPairScale.price_ticks(0.0), 0);
}

TEST(TickScale, RoundsRatherThanTruncates)
{
    EXPECT_EQ(kUsdtPairScale.price_ticks(1.006), 101);
    EXPECT_EQ(kUsdtPairScale.price_ticks(1.004), 100);
}

TEST(TickScale, ConvertsQuantitiesToUnits)
{
    EXPECT_EQ(kUsdtPairScale.quantity_units(1.2), 120'000'000u);
    EXPECT_EQ(kUsdtPairScale.quantity_units(0.0), 0u);
}

TEST(TickScale, RoundTripsThroughTicks)
{
    EXPECT_DOUBLE_EQ(kUsdtPairScale.price_from_ticks(kUsdtPairScale.price_ticks(63501.10)),
                     63501.10);
}

TEST(TickScale, RejectsNonFiniteAndNegativeInput)
{
    EXPECT_THROW((void)kUsdtPairScale.price_ticks(std::nan("")), std::domain_error);
    EXPECT_THROW((void)kUsdtPairScale.price_ticks(-1.0), std::domain_error);
    EXPECT_THROW((void)kUsdtPairScale.quantity_units(std::nan("")), std::domain_error);
    EXPECT_THROW((void)kUsdtPairScale.quantity_units(-0.5), std::domain_error);
}

TEST(TopOfBook, ReportsSpreadAndMid)
{
    const TopOfBook top{10'000, 10'002, 1, 1};

    EXPECT_EQ(top.spread_ticks(), 2);
    EXPECT_DOUBLE_EQ(top.mid_ticks(), 10'001.0);
    EXPECT_FALSE(top.crossed());
}

TEST(TopOfBook, FlagsACrossedQuote)
{
    EXPECT_TRUE((TopOfBook{10'002, 10'000, 1, 1}.crossed()));
    EXPECT_TRUE((TopOfBook{10'000, 10'000, 1, 1}.crossed()));
}

TEST(TopOfBook, MicropriceLeansAwayFromTheHeavierSide)
{
    const TopOfBook heavy_ask{10'000, 10'002, 1, 3};
    const TopOfBook heavy_bid{10'000, 10'002, 3, 1};

    EXPECT_DOUBLE_EQ(heavy_ask.microprice_ticks(), 10'000.5);
    EXPECT_DOUBLE_EQ(heavy_bid.microprice_ticks(), 10'001.5);
}

TEST(TopOfBook, MicropriceNeedsSizeSomewhere)
{
    EXPECT_THROW((void)(TopOfBook{10'000, 10'002, 0, 0}.microprice_ticks()), std::domain_error);
}

TEST(SnapshotBook, RestsBothSidesOfTheFirstSnapshot)
{
    SnapshotBook book(kUsdtPairScale);
    book.apply(snapshot(1, 100.00, 1.0, 100.02, 2.0));

    EXPECT_EQ(book.applied_count(), 1u);
    EXPECT_EQ(book.book().size(), 2u);
    EXPECT_EQ(book.top().bid_ticks, 10'000);
    EXPECT_EQ(book.top().ask_ticks, 10'002);
    EXPECT_EQ(book.top().bid_units, 100'000'000u);
    EXPECT_EQ(book.top().ask_units, 200'000'000u);
}

TEST(SnapshotBook, ReplacesQuotesRatherThanStackingThem)
{
    SnapshotBook book(kUsdtPairScale);
    for (std::uint64_t i = 1; i <= 32; i++)
        book.apply(snapshot(i, 100.00 + static_cast<double>(i), 1.0,
                            100.02 + static_cast<double>(i), 1.0));

    EXPECT_EQ(book.applied_count(), 32u);
    EXPECT_EQ(book.book().size(), 2u);
    EXPECT_EQ(book.top().bid_ticks, kUsdtPairScale.price_ticks(132.00));
}

TEST(SnapshotBook, IgnoresDuplicateAndReorderedUpdates)
{
    SnapshotBook book(kUsdtPairScale);
    book.apply(snapshot(7, 100.00, 1.0, 100.02, 1.0));
    book.apply(snapshot(7, 200.00, 1.0, 200.02, 1.0));
    book.apply(snapshot(6, 300.00, 1.0, 300.02, 1.0));

    EXPECT_EQ(book.applied_count(), 1u);
    EXPECT_EQ(book.stale_count(), 2u);
    EXPECT_EQ(book.top().bid_ticks, 10'000);
}

TEST(SnapshotBook, LeavesAZeroSizedSideEmpty)
{
    SnapshotBook book(kUsdtPairScale);
    book.apply(snapshot(1, 100.00, 1.0, 100.02, 0.0));

    EXPECT_EQ(book.book().size(), 1u);
    EXPECT_EQ(book.top().ask_units, 0u);
}

TEST(SnapshotBook, MatchesAndCountsACrossedSnapshot)
{
    SnapshotBook book(kUsdtPairScale);
    book.apply(snapshot(1, 100.02, 1.0, 100.00, 1.0));

    EXPECT_EQ(book.crossed_count(), 1u);
    EXPECT_GT(book.trade_count(), 0u);
    EXPECT_EQ(book.book().size(), 0u);
}

TEST(SnapshotBook, RejectsAGarbagePrice)
{
    SnapshotBook book(kUsdtPairScale);

    EXPECT_THROW(book.apply(snapshot(1, -1.0, 1.0, 100.02, 1.0)), std::domain_error);
    EXPECT_EQ(book.applied_count(), 0u);
}
