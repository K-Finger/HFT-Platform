#pragma once

#include <cstddef>
#include <cstdint>

namespace hft::capture
{

/* "HFTC", so reading an unrelated file as a capture fails immediately. */
constexpr std::uint32_t kMagic         = 0x43544648;
constexpr std::uint32_t kFormatVersion = 1;

/* Sized so the first record header starts on its own cache line. */
struct alignas(64) FileHeader
{
    std::uint32_t magic;
    std::uint32_t format_version;
    std::uint64_t reserved[7];
};

/* The file carries no record count and no index. Readers walk to EOF, so a
   recorder killed mid-session leaves a file that is valid up to its last whole
   record and needs no finalisation step. */
struct RecordHeader
{
    std::uint64_t timestamp_ns;
    std::uint32_t payload_size;  // payload bytes, excluding the NUL terminator
    std::uint32_t reserved;
};

static_assert(sizeof(FileHeader) == 64);
static_assert(sizeof(RecordHeader) == 16);

constexpr std::size_t kRecordAlignment = 8;

/* Bytes one record occupies: header, payload, a NUL terminator, then padding
   that keeps the next header 8-byte aligned so readers can pointer-cast it.
   The terminator lets parsers take a plain const char* straight out of the
   mapping with no copy and no bounded scan. */
constexpr std::size_t record_stride(std::uint32_t payload_size)
{
    const std::size_t unpadded = sizeof(RecordHeader) + payload_size + 1;
    return (unpadded + kRecordAlignment - 1) & ~(kRecordAlignment - 1);
}

}  // namespace hft::capture
