#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <array>
#include <atomic>
#include <queue>
#include <iostream>

#include "circular_buffer.h"

// The maximum allowed lengths of keys and values
#define MAX_LENGTH_KEY 128
#define MAX_LENGTH_VAL 1024

//constexpr const size_t slots = 10;
constexpr const size_t slots = 2;

/**
 * Simple spinlock protecting reading accesses on single Messages
 */
class Spinlock {
    public:
        void lock() {
            while (!try_lock()) {
                // TODO: sleep
            }
        }

        inline bool try_lock() {
            return !_flag.test_and_set();
        }

        void unlock() {
            _flag.clear();
        }
    private:
        //std::atomic_flag _flag{ ATOMIC_FLAG_INIT };
        std::atomic_flag _flag{false};
};

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
        DELETE,
        RESPONSE,
        EXIT // Signals the reading thread to exit and is pushed by the server when a SIGINT occurs
    }; //mode;
    //std::atomic<mode_t> mode;
    mode_t mode;
    //std::atomic<bool> read;
    //size_t key_length;
    //size_t data_length;
    //uint8_t key[MAX_LENGTH_KEY];
    //uint8_t data[MAX_LENGTH_VAL];
    //bool success;
    std::atomic<bool> success;
    std::atomic<bool> ready;
    Spinlock rlock;
    std::array<uint8_t, MAX_LENGTH_KEY> key;
    std::array<uint8_t, MAX_LENGTH_VAL> data;


    //Message() = default;
    Message() : mode(Message::DEFAULT),
                success(false),
                ready(false),
                key({}),
                data({}) {
        //std::atomic_init(&read, false);
    }

    Message(Message& other) : mode(other.mode),
                                        //mode(other.mode.load()),
                                        //read(other.read.load()),
                                        //success(false),
                                        success(other.success.load()),
                                        ready(other.ready.load()),
                                        key(other.key),
                                        data(other.data) { }

    Message(Message&& other) : mode(std::move(other.mode)),
                                         //mode(other.mode.load()),
                                         //read(other.read.load(std::memory_order_seq_cst)),
                                         //success(false),
                                         success(other.success.load()),
                                         ready(other.ready.load()),
                                         key(std::move(other.key)),
                                         data(std::move(other.data)) { }

    Message& operator=(const Message& other) {
        mode = other.mode;
        //mode.store(other.mode.load());
        //read = other.read.load(std::memory_order_acquire);
        //read = other.read.load(std::memory_order_seq_cst);
        //key  = other.key;
        //data = other.data;
        //success = other.success;
        success.store(other.success.load());
        ready.store(other.ready.load());
        std::copy(other.key.begin(), other.key.end(), key.begin());
        std::copy(other.data.begin(), other.data.end(), data.begin());
        return *this;
    }
    Message& operator=(Message&& other) {
        mode = std::move(other.mode);
        //mode.store(other.mode.load());
        //read = other.read.load(std::memory_order_seq_cst);
        //success = std::move(other.success);
        success.store(other.success.load());
        ready.store(other.ready.load());
        key  = std::move(other.key);
        data = std::move(other.data);
        return *this;
    }
} Message;

template <size_t slots = 10>
//typedef struct Mailbox {
struct Mailbox {
    Mailbox() : msgs(CircularBuffer<Message, slots>{}) {};

    //Mailbox& operator=(const Mailbox& other) {
    //    msgs = other.msgs;
    //    return *this;
    //}

    //Mailbox& operator=(Mailbox&& other) {
    //    msgs = std::move(other.msgs);
    //    return *this;
    //}

    CircularBuffer<Message, slots> msgs;
}; // Mailbox;

//typedef struct MMap {
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

// TODO
void printMessage(const Message& msg) {
    switch(msg.mode) {
        case Message::GET:

            break;
        case Message::INSERT:

            break;
        case Message::DELETE:

            break;
        case Message::READ_BUCKET:

            break;
        default:
            break;
    }
}

