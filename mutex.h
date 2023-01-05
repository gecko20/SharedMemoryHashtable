#pragma once

#include <pthread.h>
#ifdef __APPLE__
// macOS does not support POSIX semaphores, but those from System V
//#include <sys/sem.h>
//But since those are... special we use our own Semaphore class
//#include <condition_variable>
#else
#include <semaphore.h>
#endif

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

        void wait(); // acquire
        void post(); // release
        
        bool try_post();
    private:
        pthread_mutex_t _mutex;
#ifdef __APPLE__
        //std::condition_variable _cond;
        pthread_cond_t _cond;
        size_t _count;
#else
        sem_t           _sem;
#endif
};

