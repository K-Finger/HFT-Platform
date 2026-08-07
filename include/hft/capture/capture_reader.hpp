#pragma once

#include "hft/capture/capture_file.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>

namespace hft::capture
{

/* One record, pointing straight into the mapping. The payload is NUL terminated,
   so it feeds a parser with no copy. Valid while the reader is alive. */
struct CaptureRecord
{
    std::uint64_t timestamp_ns;
    const char* payload;
    std::uint32_t payload_size;
};

/* Read-only view over a capture file.

   The constructor maps the file and walks it once to validate every header,
   stride and terminator, so the replay loop touches no bounds checks and no
   error paths. Iteration is a header load and a pointer add.

   Regular files cannot use MAP_HUGETLB, so the mapping asks for MAP_POPULATE and
   pays its page faults at startup instead of inside the loop. */
class CaptureReader
{
  public:
    class Iterator
    {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = CaptureRecord;
        using difference_type = std::ptrdiff_t;

        /* operator* builds a record rather than exposing one, so dereferencing
           yields a value. Both members are still required for
           std::iterator_traits to recognise the type at all. */
        using reference = CaptureRecord;
        using pointer = void;

        explicit Iterator(const std::byte* cursor) : cursor_(cursor) {}

        CaptureRecord operator*() const
        {
            const auto* header = reinterpret_cast<const RecordHeader*>(cursor_);
            return CaptureRecord{header->timestamp_ns,
                                 reinterpret_cast<const char*>(cursor_ + sizeof(RecordHeader)),
                                 header->payload_size};
        }

        Iterator& operator++()
        {
            const auto* header = reinterpret_cast<const RecordHeader*>(cursor_);
            cursor_ += record_stride(header->payload_size);
            return *this;
        }

        bool operator==(const Iterator& other) const { return cursor_ == other.cursor_; }
        bool operator!=(const Iterator& other) const { return cursor_ != other.cursor_; }

      private:
        const std::byte* cursor_;
    };

    /* Throws std::system_error when the file cannot be opened or mapped, and
       std::runtime_error when the magic, version or record layout is wrong. */
    explicit CaptureReader(std::string path);
    ~CaptureReader();

    CaptureReader(const CaptureReader&) = delete;
    CaptureReader& operator=(const CaptureReader&) = delete;

    [[nodiscard]] Iterator begin() const { return Iterator(base_ + sizeof(FileHeader)); }
    [[nodiscard]] Iterator end() const { return Iterator(base_ + size_bytes_); }

    [[nodiscard]] std::uint64_t record_count() const { return record_count_; }
    [[nodiscard]] std::size_t size_bytes() const { return size_bytes_; }

  private:
    void validate();

    std::string path_;
    const std::byte* base_ = nullptr;
    std::size_t size_bytes_ = 0;
    std::uint64_t record_count_ = 0;
};

}  // namespace hft::capture
