#include <cstdio>
#include "feed/shm_ring.hpp"
#include "feed/shm_provider.hpp"
#include "utils/thread_utils.hpp"
#include "utils/tsc.hpp"

int main()
{
    pin_thread_to_core(3);
    set_realtime(80);

    RingBuffer *ring = open_shared_memory(false);
    if (!ring)
        return 1;

    LatencyHistogram hist;
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

            double mid = (msg.bid_price + msg.ask_price) / 2.0;
            printf("mid=%.2f bid=%.2f ask=%.2f\n", mid, msg.bid_price, msg.ask_price);

            if (count % 50 == 0)
            {
                printf("\n--- latency histogram (pop cycles) ---\n");
                hist.print();
                printf("--------------------------------------\n\n");
            }
        }
        else
        {
            __builtin_ia32_pause();
        }
    }
}
