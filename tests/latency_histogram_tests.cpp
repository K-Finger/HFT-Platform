#include "hft/time/latency_histogram.hpp"

#include <gtest/gtest.h>

using hft::time::LatencyHistogram;

TEST(LatencyHistogram, StartsEmpty)
{
    const LatencyHistogram histogram;

    EXPECT_EQ(histogram.total, 0u);
    EXPECT_EQ(histogram.counts[0], 0u);
}

TEST(LatencyHistogram, BucketsByPowerOfTwo)
{
    LatencyHistogram histogram;
    histogram.record(8);
    histogram.record(15);
    histogram.record(16);

    EXPECT_EQ(histogram.total, 3u);
    EXPECT_EQ(histogram.counts[3], 2u);
    EXPECT_EQ(histogram.counts[4], 1u);
}

TEST(LatencyHistogram, ZeroCyclesLandInFirstBucket)
{
    LatencyHistogram histogram;
    histogram.record(0);

    EXPECT_EQ(histogram.total, 1u);
    EXPECT_EQ(histogram.counts[0], 1u);
}
