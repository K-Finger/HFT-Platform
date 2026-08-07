#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hft::capture
{

/* Append-only writer for capture files. Buffers whole records and flushes in
   page-sized batches, so a high-rate feed costs one syscall per batch instead of
   one per frame, and a killed process never leaves a partial record on disk.

   Not thread-safe and not for the hot path: record in a separate process so disk
   writes never land in a pinned ingestion thread. */
class CaptureWriter
{
  public:
    static constexpr std::size_t kBufferBytes = 1 << 20;

    /* Creates or truncates the file and writes the header.
       Throws std::system_error when the file cannot be opened or written. */
    explicit CaptureWriter(const std::string& path);

    /* Flushes and closes, reporting a failure to stderr because it cannot throw.
       Call close() directly to observe errors. */
    ~CaptureWriter();

    CaptureWriter(const CaptureWriter&) = delete;
    CaptureWriter& operator=(const CaptureWriter&) = delete;

    /* Queues one record. Throws std::length_error when a payload cannot fit the
       buffer, std::system_error when a flush fails. */
    void append(std::uint64_t timestamp_ns, std::string_view payload);

    void flush();

    /* Idempotent. Throws std::system_error when the flush or close fails. */
    void close();

    std::uint64_t record_count() const { return record_count_; }

  private:
    std::string path_;
    int fd_;
    std::vector<std::byte> buffer_;
    std::size_t buffered_ = 0;
    std::uint64_t record_count_ = 0;
};

}  // namespace hft::capture
