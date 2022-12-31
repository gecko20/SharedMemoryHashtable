#pragma once

#include "message.h"

/**
 * Polls the server's mailbox for a new message and spawns
 * a new thread which handles the associated request.
 */
void receiveMsg(Mailbox* mailbox);

/**
 * Creates a new Message/response for a client.
 * The Message will be written to the same slot the original
 * request was stored in.
 *
 * @param idx the slot's index in the underlying CircularBuffer
 * @param msg the response in the format of a struct Message
 */
void respond(Mailbox* mailbox, int idx, Message msg);

