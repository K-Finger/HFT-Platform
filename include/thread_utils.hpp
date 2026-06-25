#ifndef THREAD_UTILS_HPP
#define THREAD_UTILS_HPP

#include <pthread.h>
#include <sched.h>
#include <cstdio>

inline void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset; // CPU(s) bitmask representation
    CPU_ZERO(&cpuset); // clear all bits
    CPU_SET(core_id, &cpuset); // set bit for core_id
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

inline void set_realtime(int priority) {
    struct sched_param sp;
    sp.sched_priority = priority;
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (ret != 0) perror("perror_setschedparam"); // continue with normal scheduling
}

#endif