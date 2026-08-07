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

TEST(LatencyHistogram, PercentileIsZeroWhenEmpty)
{
    const LatencyHistogram histogram;

    EXPECT_EQ(histogram.percentile(0.50), 0u);
}

TEST(LatencyHistogram, PercentileReturnsTheBucketUpperBoundOfTheTargetSample)
{
    LatencyHistogram histogram;
    for (int i = 0; i < 990; i++)
        histogram.record(8);  // bucket [8, 16)
    for (int i = 0; i < 9; i++)
        histogram.record(100);  // bucket [64, 128)
    histogram.record(1000);     // bucket [512, 1024)

    EXPECT_EQ(histogram.percentile(0.50), 16u);
    EXPECT_EQ(histogram.percentile(0.99), 128u);
    EXPECT_EQ(histogram.percentile(0.999), 1024u);
}
