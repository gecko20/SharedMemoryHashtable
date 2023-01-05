#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include "mutex.h"

PMutex::PMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    //pthread_mutexattr_setrobust(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    if(pthread_mutex_init(&_handle, &attr) == -1) {
        std::perror("PMutex::pthread_mutex_init()");
        std::exit(-1);
    }
}

PMutex::~PMutex() {
    if(pthread_mutex_destroy(&_handle) == -1) {
        std::perror("PMutex::pthread_mutex_destroy()");
        std::exit(-1);
    }
}

void PMutex::lock() {
    if(pthread_mutex_lock(&_handle) != 0) {
        std::perror("PMutex::pthread_mutex_lock()");
        std::exit(-1);
    }
}

void PMutex::unlock() {
    if(pthread_mutex_unlock(&_handle) != 0) {
        std::perror("PMutex::pthread_mutex_unlock()");
        std::exit(-1);
    }
}


CountingSemaphore::CountingSemaphore(unsigned int value) {
#ifdef __APPLE__
    _count = value;
#endif
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    if(pthread_mutex_init(&_mutex, &attr) == -1) {
        std::perror("CountingSemaphore::pthread_mutex_init()");
        std::exit(-1);
    }
#ifdef __APPLE__
    //_sem = dispatch_semaphore_create(value);
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

    if(pthread_cond_init(&_cond, &cond_attr) != 0) {
        std::perror("CountingSemaphore::pthread_cond_init()");
        std::exit(-1);
    }
#else
    if(sem_init(&_sem, 1, value) == -1) {
        std::perror("CountingSemaphore::sem_init()");
        std::exit(-1);
    }
#endif
}

CountingSemaphore::~CountingSemaphore() {
    if(pthread_mutex_destroy(&_mutex) != 0) {
        std::perror("CountingSemaphore::pthread_mutex_destroy()");
        std::exit(-1);
    }
#ifdef __APPLE__
    if(pthread_cond_destroy(&_cond) != 0) {
        std::perror("CountingSemaphore::pthread_cond_destroy()");
        std::exit(-1);
    }
#else
    if(sem_destroy(&_sem) == -1) {
        std::perror("CountingSemaphore::sem_destroy()");
        std::exit(-1);
    }
#endif
}

void CountingSemaphore::wait() {
#ifdef __APPLE__
    //dispatch_semaphore_wait(_sem, DISPATCH_TIME_FOREVER);
    // Lock Mutex
    if(pthread_mutex_lock(&_mutex) != 0) {
        std::perror("CountingSemaphore::wait(): pthread_mutex_lock()");
        std::exit(-1);
    }
    // Wait for _count > 0
    while(!_count) {
        if(pthread_cond_wait(&_cond, &_mutex) != 0) {
            std::perror("CountingSemaphore::wait(): pthread_cond_wait()");
            std::exit(-1);
        }
    
    }
    --_count;

    // Notify a waiting thread
    if(_count > 0) {
        if(pthread_cond_signal(&_cond) != 0) {
            std::perror("CountingSemaphore::post(): pthread_cond_signal()");
            std::exit(-1);
        }
    }
    // Unlock Mutex
    if(pthread_mutex_unlock(&_mutex) != 0) {
        std::perror("CountingSemaphore::wait(): pthread_mutex_unlock()");
        std::exit(-1);
    }
    
#else
    if(sem_wait(&_sem) == -1) {
        std::perror("CountingSemaphore::sem_wait()");
        std::exit(-1);
    }
#endif
}

void CountingSemaphore::post() {
#ifdef __APPLE__
    //dispatch_semaphore_signal(_sem);
    // Lock Mutex
    if(pthread_mutex_lock(&_mutex) != 0) {
        std::perror("CountingSemaphore::post(): pthread_mutex_lock()");
        std::exit(-1);
    }

    ++_count;

    // Notify a waiting thread
    if(_count > 0) {
        if(pthread_cond_signal(&_cond) != 0) {
            std::perror("CountingSemaphore::post(): pthread_cond_signal()");
            std::exit(-1);
        }
    }
    // Unlock Mutex
    if(pthread_mutex_unlock(&_mutex) != 0) {
        std::perror("CountingSemaphore::post(): pthread_mutex_unlock()");
        std::exit(-1);
    }
#else
    if(sem_post(&_sem) == -1) {
        std::perror("CountingSemaphore::sem_post()");
        std::exit(-1);
    }
#endif
}

