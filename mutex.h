#pragma once

#include "pthread.h"
#include "semaphore.h"

/**
 * Simple wrapper class for pthread_mutexes since
 * C++ std::mutexes do not work between processes
 */
class PMutex {
    public:
        PMutex();
        ~PMutex();

        void lock();
        void unlock();

    private:
        pthread_mutex_t _handle;
};

/**
 * Simple wrapper class for pthread_countingblbla since
 * C++ std::counting_semaphores do have a bug leading to
 * deadlocks in certain circumstances:
 * https://stackoverflow.com/questions/71893279/why-does-stdcounting-semaphoreacquire-suffer-deadlock-in-this-case
 */
class CountingSemaphore {
    public:
        CountingSemaphore(unsigned int value = 0);
        ~CountingSemaphore();

        void wait();
        void post();
    private:
        pthread_mutex_t _mutex;
        sem_t           _sem;
};
