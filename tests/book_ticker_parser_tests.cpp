#include "hft/feed/book_ticker_parser.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using hft::feed::parse_book_ticker;

namespace
{

constexpr auto kFrame = R"({"u":400900217,"s":"BTCUSDT","b":"63501.10","B":"1.20",)"
                        R"("a":"63502.45","A":"3.40"})";

}  // namespace

TEST(BookTickerParser, ExtractsEveryField)
{
    const hft::Message msg = parse_book_ticker(kFrame);

    EXPECT_EQ(msg.update_id, 400900217u);
    EXPECT_DOUBLE_EQ(msg.bid_price, 63501.10);
    EXPECT_DOUBLE_EQ(msg.bid_qty, 1.20);
    EXPECT_DOUBLE_EQ(msg.ask_price, 63502.45);
    EXPECT_DOUBLE_EQ(msg.ask_qty, 3.40);
}

TEST(BookTickerParser, LeavesTimestampToTheCaller)
{
    EXPECT_EQ(parse_book_ticker(kFrame).timestamp, 0u);
}

TEST(BookTickerParser, ThrowsOnMissingField)
{
    constexpr auto truncated = R"({"u":1,"s":"BTCUSDT","b":"63501.10"})";

    EXPECT_THROW(parse_book_ticker(truncated), std::runtime_error);
}
