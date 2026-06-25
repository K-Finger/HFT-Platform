#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include "feed/shm_ring.hpp"

RingBuffer *open_shared_memory(bool create)
{
    int flags = create ? O_CREAT | O_RDWR : O_RDWR;

    // Open file.
    // If it doesn't exist then create it where anyone can read/write to it
    int fd = shm_open("/hft_ring", flags, 0666);
    if (fd == -1)
    {
        exit(1);
    }

    if (create)
        ftruncate(fd, sizeof(RingBuffer));

    void *ptr = mmap(nullptr, sizeof(RingBuffer), PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE, fd, 0);

    if (ptr == MAP_FAILED)
    {
        ptr = mmap(nullptr, sizeof(RingBuffer), PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_POPULATE, fd, 0);
    }

    close(fd);
    return static_cast<RingBuffer *>(ptr);
}