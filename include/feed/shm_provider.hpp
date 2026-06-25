#pragma once
#include "feed/shm_ring.hpp"

RingBuffer* open_shared_memory(bool create);
