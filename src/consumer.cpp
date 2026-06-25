#include "shm_ring.hpp"
#include "shm_provider.hpp"
#include "thread_utils.hpp"
#include "tsc.hpp"
#include "order_book.hpp"

int main()
{
    pin_thread_to_core(3);
    set_realtime(80);

    RingBuffer *ring = open_shared_memory(false);
    if (!ring)
        return 1;

    LatencyHistogram hist;
    OrderBook book;
    uint64_t count = 0;
    Message msg;

    while (true)
    {
        uint64_t t0 = rdtsc();
        if (ring->pop(msg))
        {
            uint64_t t1 = rdtsc();
            hist.record(t1 - t0);
            count++;

            book.update(msg);
            book.print();

            if (count % 50 == 0)
            {
                printf("\n--- latency histogram (pop cycles) ---\n");
                hist.print();
                printf("--------------------------------------\n\n");
            }
        }
        else
        {
            __builtin_ia32_pause(); // Tell CPU we're spinning
        }
    }
}
