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

