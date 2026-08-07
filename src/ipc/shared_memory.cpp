#include "hft/ipc/shared_memory.hpp"

#include "hft/sys/scoped_descriptor.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>

/* glibc's sys/mman.h declares MAP_HUGETLB but leaves the size selector to the
   Linux headers, which are not present on every toolchain. The encoding is the
   log2 of the page size shifted into the flag word. */
#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

namespace hft::ipc
{
namespace
{

std::system_error shm_error(int error_number, const std::string& what)
{
    return {error_number, std::generic_category(), what + " for shared memory object " + kRingName};
}

/* Maps onto 2MB huge pages so the whole ring costs one TLB entry and the tight
   loop never pays a page walk. MAP_POPULATE faults the pages in at startup
   instead of during the first trade. Huge pages must be reserved up front
   (vm.nr_hugepages), so fall back to 4KB pages when none are available. */
SpscRing* map_ring(int fd)
{
    void* mapping = mmap(nullptr, sizeof(SpscRing), PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE, fd, 0);
    if (mapping != MAP_FAILED)
        return static_cast<SpscRing*>(mapping);

    mapping =
        mmap(nullptr, sizeof(SpscRing), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (mapping == MAP_FAILED)
        throw shm_error(errno, "mmap failed");

    return static_cast<SpscRing*>(mapping);
}

}  // namespace

SpscRing* create_shared_ring()
{
    const int fd = shm_open(kRingName, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
        throw shm_error(errno, "shm_open(O_CREAT | O_RDWR) failed");

    const sys::ScopedDescriptor descriptor(fd);
    if (ftruncate(descriptor.get(), sizeof(SpscRing)) == -1)
        throw shm_error(errno, "ftruncate failed");

    return map_ring(descriptor.get());
}

SpscRing* open_shared_ring()
{
    const int fd = shm_open(kRingName, O_RDWR, 0666);
    if (fd == -1)
        throw shm_error(errno, "shm_open(O_RDWR) failed, the producer must create the ring first");

    const sys::ScopedDescriptor descriptor(fd);
    return map_ring(descriptor.get());
}

void unmap_shared_ring(SpscRing* ring)
{
    if (munmap(ring, sizeof(SpscRing)) == -1)
        throw shm_error(errno, "munmap failed");
}

void unlink_shared_ring()
{
    if (shm_unlink(kRingName) == -1)
        throw shm_error(errno, "shm_unlink failed");
}

}  // namespace hft::ipc
