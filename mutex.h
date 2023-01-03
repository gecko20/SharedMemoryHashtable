#pragma once

#include "pthread.h"

/**
 * Simple wrapper class for pthread_mutexes since
 * C++ does not really have interprocess synchronisation.
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

