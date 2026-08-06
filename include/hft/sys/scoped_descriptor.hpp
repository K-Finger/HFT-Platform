#pragma once

#include <unistd.h>

namespace hft::sys
{

/* Closes a file descriptor on every exit path, including a throw. Used where the
   resource that matters outlives the descriptor, such as a mapping. */
class ScopedDescriptor
{
  public:
    explicit ScopedDescriptor(int fd) : fd_(fd) {}
    ~ScopedDescriptor() { close(fd_); }

    ScopedDescriptor(const ScopedDescriptor&)            = delete;
    ScopedDescriptor& operator=(const ScopedDescriptor&) = delete;

    int get() const { return fd_; }

  private:
    int fd_;
};

}  // namespace hft::sys
