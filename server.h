#pragma once

#include <cstdint>
#include "message.h"

// Used to convert uint8_t arrays to uint32_t or uint64_t respectively
union uint8_to_uint32 {
    uint32_t uint32;
    uint8_t uint8[4];
};

union uint8_to_uint64 {
    uint64_t uint64;
    uint8_t uint8[8];
};

/**
 * Polls the server's mailbox for a new message and spawns
 * a new thread which handles the associated request.
 */
void receiveMsg(Mailbox<slots>* mailbox);

/**
 * Creates a new Message/response for a client.
 * The Message will be written to the same slot the original
 * request was stored in.
 *
 * @param idx the slot's index in the underlying CircularBuffer
 * @param msg the response in the format of a struct Message
 */
void respond(Mailbox<slots>* mailbox, size_t idx, Message msg);

