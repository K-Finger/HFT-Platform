#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include "shm_ring.hpp"

RingBuffer* open_shared_memory(bool create) 
{
    int flags = create ? O_CREAT | O_RDWR : O_RDWR;

    // Open file. 
    // If it doesn't exist then create it where anyone can read/write to it
    int fd = shm_open("/hft_ring", flags, 0666);
    if (fd == -1)
    {
        exit(1);
    }

    if (create) ftruncate(fd, sizeof(RingBuffer));

    // Get a pointer to a shared memory mapping where..
    // Kernal picks the location
    // It can be read and written to
    // Mapping is shared
    // Using file descriptor at 0 offset
    void* ptr = mmap(
        nullptr,
        sizeof(RingBuffer), 
        PROT_READ | PROT_WRITE, 
        MAP_SHARED, 
        fd, 
        0
    );

    close(fd);
    return static_cast<RingBuffer*>(ptr);
}