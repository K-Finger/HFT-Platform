#include "hft/ipc/spsc_ring.hpp"

#include <gtest/gtest.h>

#include <memory>

using hft::Message;
using hft::ipc::SpscRing;

namespace
{

Message make_message(std::uint64_t update_id)
{
    Message msg{};
    msg.bid_price = 100.0 + static_cast<double>(update_id);
    msg.ask_price = 101.0 + static_cast<double>(update_id);
    msg.update_id = update_id;
    return msg;
}

std::unique_ptr<SpscRing> make_ring()
{
    auto ring = std::make_unique<SpscRing>();
    ring->head.store(0);
    ring->tail.store(0);
    return ring;
}

}  // namespace

TEST(SpscRing, PopOnEmptyRingFails)
{
    auto ring = make_ring();
    Message out{};

    EXPECT_FALSE(ring->pop(out));
}

TEST(SpscRing, PushThenPopReturnsSameMessage)
{
    auto ring = make_ring();
    ASSERT_TRUE(ring->push(make_message(7)));

    Message out{};
    ASSERT_TRUE(ring->pop(out));
    EXPECT_EQ(out.update_id, 7u);
    EXPECT_DOUBLE_EQ(out.bid_price, 107.0);
    EXPECT_DOUBLE_EQ(out.ask_price, 108.0);
}

TEST(SpscRing, PreservesFifoOrder)
{
    auto ring = make_ring();
    for (std::uint64_t i = 0; i < 16; i++)
        ASSERT_TRUE(ring->push(make_message(i)));

    Message out{};
    for (std::uint64_t i = 0; i < 16; i++)
    {
        ASSERT_TRUE(ring->pop(out));
        EXPECT_EQ(out.update_id, i);
    }
    EXPECT_FALSE(ring->pop(out));
}

TEST(SpscRing, PushOnFullRingFails)
{
    auto ring = make_ring();
    for (std::uint64_t i = 0; i < SpscRing::kCapacity; i++)
        ASSERT_TRUE(ring->push(make_message(i)));

    EXPECT_FALSE(ring->push(make_message(SpscRing::kCapacity)));
}

TEST(SpscRing, CursorsWrapAroundCapacity)
{
    auto ring = make_ring();
    Message out{};

    for (std::uint64_t i = 0; i < SpscRing::kCapacity * 3; i++)
    {
        ASSERT_TRUE(ring->push(make_message(i)));
        ASSERT_TRUE(ring->pop(out));
        EXPECT_EQ(out.update_id, i);
    }
}
