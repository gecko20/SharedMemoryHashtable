#include <cstdio>
#include <cstdlib>

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
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    if(pthread_mutex_init(&_mutex, &attr) == -1) {
        std::perror("CountingSemaphore::pthread_mutex_init()");
        std::exit(-1);
    }

    if(sem_init(&_sem, 1, value) == -1) {
        std::perror("CountingSemaphore::sem_init()");
        std::exit(-1);
    }
}

CountingSemaphore::~CountingSemaphore() {
    if(pthread_mutex_destroy(&_mutex) == -1) {
        std::perror("CountingSemaphore::pthread_mutex_destroy()");
        std::exit(-1);
    }
    if(sem_destroy(&_sem) == -1) {
        std::perror("CountingSemaphore::sem_destroy()");
        std::exit(-1);
    }
}

void CountingSemaphore::wait() {
    if(sem_wait(&_sem) == -1) {
        std::perror("CountingSemaphore::sem_wait()");
        std::exit(-1);
    }
}

void CountingSemaphore::post() {
    if(sem_post(&_sem) == -1) {
        std::perror("CountingSemaphore::sem_post()");
        std::exit(-1);
    }
}

