#include "mutex.h"

PMutex::PMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    //pthread_mutexattr_setrobust(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    if(pthread_mutex_init(&_handle, &attr) == -1) {
        // TODO: Error handling
    }
}

PMutex::~PMutex() {
    pthread_mutex_destroy(&_handle);
}

void PMutex::lock() {
    if(pthread_mutex_lock(&_handle) != 0) {
        // TODO: Error handling
    }
}

void PMutex::unlock() {
    if(pthread_mutex_unlock(&_handle) != 0) {
        // TODO: Error handling
    }
}


CountingSemaphore::CountingSemaphore(unsigned int value) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    if(pthread_mutex_init(&_mutex, &attr) == -1) {
        // TODO: Error handling
    }

    if(sem_init(&_sem, 1, value) == -1) {
        // TODO: Error handling
    }
}

CountingSemaphore::~CountingSemaphore() {
    pthread_mutex_destroy(&_mutex);
    sem_destroy(&_sem);
}

void CountingSemaphore::wait() {
    if(sem_wait(&_sem) == -1) {
        // TODO: Error handling
    }
}

void CountingSemaphore::post() {
    if(sem_post(&_sem) == -1) {
        // TODO: Error handling
    }
}

