#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <array>
#include <atomic>
#include <queue>

#include "circular_buffer.h"

// The maximum allowed lengths of keys and values
#define MAX_LENGTH_KEY 128
#define MAX_LENGTH_VAL 1024

const size_t slots = 3;

/**
 * A struct representing a single message which can be written by
 * a client and read by the server.
 * It defines a mode / request type the client sends to the server
 * followed by the payload's data_length and the payload itself
 * as raw bytes.
 *
 * Interpretation of the payload's data type is handled by the server.
 */
typedef struct Message {
    enum mode_t {
        DEFAULT, // empty
        //REGISTER_CLIENT,
        //DEREGISTER_CLIENT,
        GET,
        INSERT,
        READ_BUCKET,
        DELETE,
        RESPONSE
    } mode;
    //std::atomic<bool> read;
    //size_t key_length;
    //size_t data_length;
    //uint8_t key[MAX_LENGTH_KEY];
    //uint8_t data[MAX_LENGTH_VAL];
    std::array<uint8_t, MAX_LENGTH_KEY> key;
    std::array<uint8_t, MAX_LENGTH_VAL> data;


    //Message() = default;
    Message() : mode(Message::DEFAULT),
                key({}),
                data({}) {
        //std::atomic_init(&read, false);
    }

    constexpr Message(Message& other) : mode(other.mode),
                                        //read(other.read.load()),
                                        key(other.key),
                                        data(other.data) { }

    constexpr Message(Message&& other) : mode(std::move(other.mode)),
                                         //read(other.read.load(std::memory_order_seq_cst)),
                                         key(std::move(other.key)),
                                         data(std::move(other.data)) { }

    constexpr Message& operator=(const Message& other) {
        mode = other.mode;
        //read = other.read.load(std::memory_order_acquire);
        //read = other.read.load(std::memory_order_seq_cst);
        //key  = other.key;
        //data = other.data;
        std::copy(other.key.begin(), other.key.end(), key.begin());
        std::copy(other.data.begin(), other.data.end(), data.begin());
        return *this;
    }
    constexpr Message& operator=(Message&& other) {
        mode = std::move(other.mode);
        //read = other.read.load(std::memory_order_seq_cst);
        key  = std::move(other.key);
        data = std::move(other.data);
        return *this;
    }

//    constexpr Message(Message& other) : mode(other.mode),
//                                        read(other.read.load()),
//                                        //key_length(other.key_length),
//                                        //data_length(other.data_length),
//                                        key(other.key),
//                                        data(other.data) { }
//
//    constexpr Message(const Message& other) : mode(other.mode),
//                                              read(other.read.load(std::memory_order_seq_cst)),
//                                              key(other.key),
//                                              data(other.data) { }
//
//    Message& operator=(const Message& other) {
//        mode = other.mode;
//        //read = other.read.load(std::memory_order_acquire);
//        read = other.read.load(std::memory_order_seq_cst);
//        //key  = other.key;
//        //data = other.data;
//        std::copy(other.key.begin(), other.key.end(), key.begin());
//        std::copy(other.data.begin(), other.data.end(), data.begin());
//        return *this;
//    }
} Message;


typedef struct Mailbox {
    //Mailbox(Mailbox&&) = default;
    //Mailbox& operator=(Mailbox&&) = default;

    //std::array<Message, 64> msgs{};
    //std::queue<Message>

    Mailbox(size_t slots) : msgs(CircularBuffer<Message>{slots}) {};

    //Mailbox& operator=(const Mailbox& other) {
    //    msgs = other.msgs;
    //    return *this;
    //}

    //Mailbox& operator=(Mailbox&& other) {
    //    msgs = std::move(other.msgs);
    //    return *this;
    //}

    //CircularBuffer<Message> msgs{64};
    CircularBuffer<Message> msgs;
} Mailbox;

typedef struct MMap {
    MMap(size_t msg_slots) : mailbox(Mailbox(msg_slots)) {};
    //MMap& operator=(const MMap& other) {
    //    mailbox = other.mailbox;

    //    return *this;
    //}
    //MMap& operator=(MMap&& other) {
    //    mailbox = std::move(other.mailbox);

    //    return *this;
    //}

    Mailbox mailbox;
    //uint8_t memory[1024];
} MMap;

