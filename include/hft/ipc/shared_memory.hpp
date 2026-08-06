#pragma once

#include "hft/ipc/spsc_ring.hpp"

namespace hft::ipc
{

constexpr const char* kRingName = "/hft_ring";

/* Creates, sizes and maps the ring. Call from the producer only.
   Throws std::system_error when the object cannot be created or mapped. */
SpscRing* create_shared_ring();

/* Maps a ring the producer already created. Call from every consumer.
   Throws std::system_error when the object does not exist yet. */
SpscRing* open_shared_ring();

void unmap_shared_ring(SpscRing* ring);

/* Removes the object so the next run starts on a clean ring. Live mappings stay
   valid until they are unmapped. */
void unlink_shared_ring();

}  // namespace hft::ipc
