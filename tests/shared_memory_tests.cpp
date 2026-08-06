#include "hft/ipc/shared_memory.hpp"

#include <gtest/gtest.h>

#include <system_error>

using hft::ipc::create_shared_ring;
using hft::ipc::open_shared_ring;
using hft::ipc::SpscRing;
using hft::ipc::unlink_shared_ring;
using hft::ipc::unmap_shared_ring;

namespace
{

class SharedRing : public ::testing::Test
{
  protected:
    void SetUp() override { producer = create_shared_ring(); }

    void TearDown() override
    {
        unmap_shared_ring(producer);
        unlink_shared_ring();
    }

    SpscRing* producer = nullptr;
};

}  // namespace

TEST_F(SharedRing, MessageCrossesBetweenTwoMappings)
{
    SpscRing* consumer = open_shared_ring();

    hft::Message sent{};
    sent.bid_price = 63501.10;
    sent.update_id = 42;
    ASSERT_TRUE(producer->push(sent));

    hft::Message received{};
    ASSERT_TRUE(consumer->pop(received));
    EXPECT_EQ(received.update_id, 42u);
    EXPECT_DOUBLE_EQ(received.bid_price, 63501.10);

    unmap_shared_ring(consumer);
}

TEST(SharedRingWithoutProducer, OpenThrowsWhenObjectIsMissing)
{
    EXPECT_THROW(open_shared_ring(), std::system_error);
}
