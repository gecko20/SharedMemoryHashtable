#pragma once

#include "message.h"

/**
 * Sends a new Message to the server using a shared memory object.
 * The Message will be written to a free slot in a CircularBuffer
 * and be polled and handled by the server.
 * The client has to wait for a response inside sendMsg().
 *
 * @param mailbox a pointer to the shared mailbox
 * @param msg the request's type (either GET, INSERT, READ_BUCKET or DELETE)
 * @param key the key for getting a value from the HashTable or writing to the HashTable
 * @param value the C-style string which should be written to the HashTable. May be NULL or ignored when getting a value.
 * @returns a new Message containing the server's response
 */
Message sendMsg(Mailbox<slots>* mailbox, const enum Message::mode_t mode, const char* key, const char* value = NULL);
//Message sendMsg(Mailbox* mailbox, const enum Message::mode_t mode, const char* key, const char* value = NULL);

