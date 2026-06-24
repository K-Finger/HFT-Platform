#ifndef SHM_PROVIDER_HPP
#define SHM_PROVIDER_HPP

#include "shm_ring.hpp"

RingBuffer* open_shared_memory(bool create);

#endif