#include "hft/capture/capture_writer.hpp"

#include "hft/capture/capture_file.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <system_error>

namespace hft::capture
{
namespace
{

void write_all(int fd, const std::byte* data, std::size_t size, const std::string& path)
{
    std::size_t written = 0;
    while (written < size)
    {
        const ssize_t rc = ::write(fd, data + written, size - written);
        if (rc == -1)
        {
            if (errno == EINTR)
                continue;  // interrupted before any byte moved, reissue
            throw std::system_error(errno, std::generic_category(), "write failed for " + path);
        }
        written += static_cast<std::size_t>(rc);
    }
}

}  // namespace

CaptureWriter::CaptureWriter(const std::string& path)
    : path_(path), fd_(open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)),
      buffer_(kBufferBytes)
{
    if (fd_ == -1)
        throw std::system_error(errno, std::generic_category(),
                                "open(O_WRONLY | O_CREAT | O_TRUNC) failed for " + path_);

    FileHeader header{};
    header.magic          = kMagic;
    header.format_version = kFormatVersion;
    write_all(fd_, reinterpret_cast<const std::byte*>(&header), sizeof(header), path_);
}

CaptureWriter::~CaptureWriter()
{
    try
    {
        close();
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "capture writer failed to close %s: %s\n", path_.c_str(),
                     error.what());
    }
}

void CaptureWriter::append(std::uint64_t timestamp_ns, std::string_view payload)
{
    const std::size_t stride = record_stride(static_cast<std::uint32_t>(payload.size()));
    if (stride > buffer_.size())
        throw std::length_error("capture payload of " + std::to_string(payload.size()) +
                                " bytes exceeds the " + std::to_string(buffer_.size()) +
                                " byte write buffer");

    if (buffered_ + stride > buffer_.size())
        flush();

    RecordHeader header{};
    header.timestamp_ns = timestamp_ns;
    header.payload_size = static_cast<std::uint32_t>(payload.size());

    std::byte* slot = buffer_.data() + buffered_;
    std::memcpy(slot, &header, sizeof(header));
    std::memcpy(slot + sizeof(header), payload.data(), payload.size());
    std::memset(slot + sizeof(header) + payload.size(), 0,
                stride - sizeof(header) - payload.size());

    buffered_ += stride;
    record_count_++;
}

void CaptureWriter::flush()
{
    if (buffered_ == 0)
        return;

    write_all(fd_, buffer_.data(), buffered_, path_);
    buffered_ = 0;
}

void CaptureWriter::close()
{
    if (fd_ == -1)
        return;

    flush();

    const int fd = fd_;
    fd_          = -1;
    if (::close(fd) == -1)
        throw std::system_error(errno, std::generic_category(), "close failed for " + path_);
}

}  // namespace hft::capture
