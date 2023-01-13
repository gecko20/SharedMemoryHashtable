#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <array>
#include <atomic>
#include <pthread.h>
#include <queue>
#include <iostream>

#include "circular_buffer.h"
#include "mutex.h"

// The maximum allowed lengths of keys and values
#define MAX_LENGTH_KEY 128
#define MAX_LENGTH_VAL 1024

constexpr const size_t slots = 8;


/**
 * A struct representing a single message which can be written by
 * a client and read by the server.
 * It defines a mode / request type the client sends to the server
 * followed by the payload's data_length and the payload itself
 * as raw bytes.
 *
 * Interpretation of the payload's data type is handled by the server
 * as well as the client.
 */
typedef struct Message {
    enum mode_t {
        DEFAULT, // empty
        GET,
        INSERT,
        READ_BUCKET,
        CLOSE_SHM, // Signals the server to close a shared memory segment opened by READ_BUCKET
        DELETE,
        RESPONSE,
        EXIT // Signals the reading thread to exit and is pushed by the server when a SIGINT occurs
    }; //mode;

    mode_t mode;
    bool success;
    std::atomic_flag ready;
    std::atomic<pid_t> client_id;
    std::array<uint8_t, MAX_LENGTH_KEY> key;
    std::array<uint8_t, MAX_LENGTH_VAL> data;

    Message() : mode(Message::DEFAULT),
                success(false),
                ready(false),
                client_id(0),
                key({}),
                data({}) {
    }

    Message(mode_t m) : mode(m),
                        success(false),
                        ready(false),
                        client_id(0),
                        key({}),
                        data({}) { }

    Message(Message& other) : mode(other.mode),
                                        success(other.success),
                                        ready(other.ready.test()),
                                        client_id(other.client_id.load()),
                                        key(other.key),
                                        data(other.data) { }

    Message(Message&& other) : mode(std::move(other.mode)),
                                         success(other.success),
                                         ready(other.ready.test()),
                                         client_id(other.client_id.load()),
                                         key(std::move(other.key)),
                                         data(std::move(other.data)) { }

    Message& operator=(const Message& other) {
        mode = other.mode;
        success = other.success;
        if(other.ready.test()) {
            ready.test_and_set();
        } else {
            ready.clear();
        }
        client_id = other.client_id.load();
        std::copy(other.key.begin(), other.key.end(), key.begin());
        std::copy(other.data.begin(), other.data.end(), data.begin());
        return *this;
    }
    Message& operator=(Message&& other) {
        mode = std::move(other.mode);
        success = std::move(other.success);
        if(other.ready.test()) {
            ready.test_and_set();
        } else {
            ready.clear();
        }
        client_id = other.client_id.load();
        key  = std::move(other.key);
        data = std::move(other.data);
        return *this;
    }
} Message;

template <size_t slots = 10>
struct Mailbox {
    Mailbox() : msgs(CircularBuffer<Message, slots>{}), responses() {
        for(size_t i = 0; i < slots; ++i) {
            responses[i] = Message(Message::RESPONSE);

            mutexes[i] = PMutex();

            rcvs[i] = PTHREAD_COND_INITIALIZER;
            pthread_condattr_t cond_attr;
            pthread_condattr_init(&cond_attr);
            pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

            if((errno = pthread_cond_init(&rcvs[i], &cond_attr)) != 0) {
                std::perror("CountingSemaphore::pthread_cond_init()");
                std::exit(-1);
            }
        }
    };

    ~Mailbox() {
        for(size_t i = 0; i < slots; ++i) {
            //~(mutexes.data()[i])();

            if((errno = pthread_cond_destroy(&rcvs[i])) != 0) {
                std::perror("CountingSemaphore::pthread_cond_destroy()");
                std::exit(-1);
            }
        }
    };

    // Stores all requests by clients
    CircularBuffer<Message, slots> msgs;

    // Stores all responds by the server
    // Messages here all have mode == Message::RESPONSE
    // and should never be replaced, only modified
    std::array<Message, slots> responses;
    // Used for synchronization of responses
    std::array<PMutex, slots> mutexes;
    std::array<pthread_cond_t, slots> rcvs;
}; // Mailbox;

template <size_t msg_slots = 10>
struct MMap {
    MMap() : mailbox(Mailbox<msg_slots>()) {}
    //MMap& operator=(const MMap& other) {
    //    mailbox = other.mailbox;

    //    return *this;
    //}
    //MMap& operator=(MMap&& other) {
    //    mailbox = std::move(other.mailbox);

    //    return *this;
    //}

    Mailbox<msg_slots> mailbox;
}; // MMap;

