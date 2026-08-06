#pragma once

#include <pthread.h>
#include <sched.h>

#include <string>
#include <system_error>

namespace hft::sys
{

/* Locks the calling thread to one core so the scheduler cannot migrate it and
   throw away its warm L1 and L2 working set.
   Throws std::system_error when the core is unavailable to this process. */
inline void pin_to_core(int core_id)
{
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(core_id, &cpu_set);

    const int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
    if (rc != 0)
        throw std::system_error(rc, std::generic_category(),
                                "pthread_setaffinity_np failed for core " + std::to_string(core_id));
}

/* Moves the calling thread onto SCHED_FIFO so the kernel cannot preempt it for
   ordinary work. Requires CAP_SYS_NICE.
   Throws std::system_error when the priority cannot be applied, rather than
   running unpinned and reporting meaningless latencies. */
inline void request_realtime_priority(int priority)
{
    sched_param params{};
    params.sched_priority = priority;

    const int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
    if (rc != 0)
        throw std::system_error(rc, std::generic_category(),
                                "pthread_setschedparam(SCHED_FIFO, " + std::to_string(priority) +
                                    ") failed, CAP_SYS_NICE is required");
}

}  // namespace hft::sys
