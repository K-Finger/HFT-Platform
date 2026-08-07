#include "hft/capture/capture_reader.hpp"

#include "hft/sys/scoped_descriptor.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hft::capture
{
namespace
{

std::size_t file_size(int fd, const std::string& path)
{
    struct stat info
    {
    };
    if (fstat(fd, &info) == -1)
        throw std::system_error(errno, std::generic_category(), "fstat failed for " + path);

    return static_cast<std::size_t>(info.st_size);
}

std::string hex(std::uint32_t value)
{
    static constexpr char kDigits[] = "0123456789abcdef";

    std::string out = "0x";
    for (int shift = 28; shift >= 0; shift -= 4)
        out += kDigits[(value >> shift) & 0xf];

    return out;
}

}  // namespace

CaptureReader::CaptureReader(const std::string& path) : path_(path)
{
    const int fd = open(path_.c_str(), O_RDONLY);
    if (fd == -1)
        throw std::system_error(errno, std::generic_category(),
                                "open(O_RDONLY) failed for " + path_);

    const sys::ScopedDescriptor descriptor(fd);
    size_bytes_ = file_size(descriptor.get(), path_);
    if (size_bytes_ < sizeof(FileHeader))
        throw std::runtime_error(path_ + " is " + std::to_string(size_bytes_) +
                                 " bytes, too small to hold a capture header of " +
                                 std::to_string(sizeof(FileHeader)));

    void* mapping =
        mmap(nullptr, size_bytes_, PROT_READ, MAP_PRIVATE | MAP_POPULATE, descriptor.get(), 0);
    if (mapping == MAP_FAILED)
        throw std::system_error(errno, std::generic_category(), "mmap failed for " + path_);

    base_ = static_cast<const std::byte*>(mapping);

    try
    {
        validate();
    }
    catch (...)
    {
        munmap(mapping, size_bytes_);  // the destructor never runs for a failed constructor
        throw;
    }
}

CaptureReader::~CaptureReader()
{
    munmap(const_cast<std::byte*>(base_), size_bytes_);
}

void CaptureReader::validate()
{
    const auto* header = reinterpret_cast<const FileHeader*>(base_);
    if (header->magic != kMagic)
        throw std::runtime_error(path_ + " is not a capture file: magic " + hex(header->magic) +
                                 ", expected " + hex(kMagic));

    if (header->format_version != kFormatVersion)
        throw std::runtime_error(path_ + " uses capture format version " +
                                 std::to_string(header->format_version) + ", this build reads " +
                                 std::to_string(kFormatVersion));

    const std::byte* cursor = base_ + sizeof(FileHeader);
    const std::byte* limit = base_ + size_bytes_;

    while (cursor != limit)
    {
        const std::size_t offset = static_cast<std::size_t>(cursor - base_);
        const std::size_t remaining = static_cast<std::size_t>(limit - cursor);

        if (remaining < sizeof(RecordHeader))
            throw std::runtime_error(path_ + " truncated: record header at offset " +
                                     std::to_string(offset) + " needs " +
                                     std::to_string(sizeof(RecordHeader)) + " bytes, " +
                                     std::to_string(remaining) + " remain");

        const auto* record = reinterpret_cast<const RecordHeader*>(cursor);
        const std::size_t stride = record_stride(record->payload_size);
        if (remaining < stride)
            throw std::runtime_error(path_ + " truncated: record at offset " +
                                     std::to_string(offset) + " needs " + std::to_string(stride) +
                                     " bytes, " + std::to_string(remaining) + " remain");

        if (*(cursor + sizeof(RecordHeader) + record->payload_size) != std::byte{0})
            throw std::runtime_error(path_ + " record at offset " + std::to_string(offset) +
                                     " is not NUL terminated");

        cursor += stride;
        record_count_++;
    }
}

}  // namespace hft::capture
