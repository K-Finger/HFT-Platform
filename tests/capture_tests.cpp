#include "hft/capture/capture_file.hpp"
#include "hft/capture/capture_reader.hpp"
#include "hft/capture/capture_writer.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using hft::capture::CaptureReader;
using hft::capture::CaptureRecord;
using hft::capture::CaptureWriter;

namespace
{

class Capture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const char* test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        path = (std::filesystem::temp_directory_path() /
                ("hft_capture_" + std::string(test_name) + ".hftc"))
                   .string();
        std::filesystem::remove(path);
    }

    void TearDown() override { std::filesystem::remove(path); }

    /* Records point into the reader's mapping, so collecting them is only valid
       while that reader is alive. Callers keep one in scope and pass it in. */
    static std::vector<CaptureRecord> collect(const CaptureReader& reader)
    {
        return {reader.begin(), reader.end()};
    }

    void truncate_by(std::size_t bytes) const
    {
        const auto size = std::filesystem::file_size(path);
        std::filesystem::resize_file(path, size - bytes);
    }

    std::string path;
};

}  // namespace

TEST_F(Capture, EmptyFileHoldsNoRecords)
{
    {
        CaptureWriter writer(path);
        writer.close();
    }

    const CaptureReader reader(path);
    EXPECT_EQ(reader.record_count(), 0u);
    EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(Capture, RoundTripsPayloadsAndTimestamps)
{
    {
        CaptureWriter writer(path);
        writer.append(1'000, R"({"u":1,"b":"10.5"})");
        writer.append(2'000, R"({"u":2,"b":"11.5"})");
        writer.append(3'500, "x");
        writer.close();
        EXPECT_EQ(writer.record_count(), 3u);
    }

    const CaptureReader reader(path);
    const std::vector<CaptureRecord> records = collect(reader);
    ASSERT_EQ(records.size(), 3u);

    EXPECT_EQ(records[0].timestamp_ns, 1'000u);
    EXPECT_EQ(std::string(records[0].payload), R"({"u":1,"b":"10.5"})");
    EXPECT_EQ(records[0].payload_size, 18u);

    EXPECT_EQ(records[1].timestamp_ns, 2'000u);
    EXPECT_EQ(std::string(records[1].payload), R"({"u":2,"b":"11.5"})");

    EXPECT_EQ(records[2].timestamp_ns, 3'500u);
    EXPECT_EQ(std::string(records[2].payload), "x");
    EXPECT_EQ(records[2].payload_size, 1u);
}

TEST_F(Capture, PayloadsAreNulTerminatedInTheMapping)
{
    {
        CaptureWriter writer(path);
        writer.append(1, "abc");
        writer.close();
    }

    const CaptureReader reader(path);
    const std::vector<CaptureRecord> records = collect(reader);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].payload[records[0].payload_size], '\0');
}

TEST_F(Capture, RecordHeadersStayEightByteAligned)
{
    {
        CaptureWriter writer(path);
        for (int i = 0; i < 64; i++)
            writer.append(static_cast<std::uint64_t>(i),
                          std::string(static_cast<std::size_t>(i) + 1, 'a'));
        writer.close();
    }

    const CaptureReader reader(path);
    EXPECT_EQ(reader.record_count(), 64u);

    std::uint64_t expected = 0;
    for (const CaptureRecord& record : reader)
    {
        const auto header_address =
            reinterpret_cast<std::uintptr_t>(record.payload) - sizeof(hft::capture::RecordHeader);

        EXPECT_EQ(record.timestamp_ns, expected);
        EXPECT_EQ(header_address % hft::capture::kRecordAlignment, 0u);
        expected++;
    }
}

TEST_F(Capture, FlushesAcrossBufferBoundaries)
{
    const std::string payload(4'096, 'z');
    const std::size_t count = (CaptureWriter::kBufferBytes / payload.size()) + 8;

    {
        CaptureWriter writer(path);
        for (std::size_t i = 0; i < count; i++)
            writer.append(i, payload);
        writer.close();
    }

    const CaptureReader reader(path);
    ASSERT_EQ(reader.record_count(), count);
    for (const CaptureRecord& record : reader)
        EXPECT_EQ(record.payload_size, payload.size());
}

TEST_F(Capture, ReaderRejectsAMissingFile)
{
    EXPECT_THROW(CaptureReader(path + ".absent"), std::system_error);
}

TEST_F(Capture, ReaderRejectsForeignFiles)
{
    {
        std::ofstream out(path, std::ios::binary);
        out << std::string(128, 'q');
    }

    EXPECT_THROW(CaptureReader{path}, std::runtime_error);
}

TEST_F(Capture, ReaderRejectsAFileSmallerThanTheHeader)
{
    {
        CaptureWriter writer(path);
        writer.close();
    }
    truncate_by(8);

    EXPECT_THROW(CaptureReader{path}, std::runtime_error);
}

TEST_F(Capture, ReaderRejectsATruncatedRecord)
{
    {
        CaptureWriter writer(path);
        writer.append(1, R"({"u":1,"b":"10.5"})");
        writer.close();
    }
    truncate_by(4);

    EXPECT_THROW(CaptureReader{path}, std::runtime_error);
}

TEST_F(Capture, WriterRejectsAPayloadLargerThanItsBuffer)
{
    CaptureWriter writer(path);
    const std::string oversized(CaptureWriter::kBufferBytes, 'a');

    EXPECT_THROW(writer.append(1, oversized), std::length_error);
}
