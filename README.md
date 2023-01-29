## Description
This project implements a hashtable in C++20 which handles hash collisions by chaining and is supporting concurrent access (both read and write operations) with R/W-locks.
The hashtable supports all kinds of data types as keys as long as they are `hashable` (see the concept in `hashtable.h`) and `equality_comparable` and all kinds of datatypes as values as long as they are `copy_constructible`.

The project also provides two example applications:

* A server which manages a (statically sized) hashtable in its own process space and which is opening up a shared memory segment for IPC with clients via a circular buffer
* A client which connects to the same shared memory segment opened up by the server. It reads input from `stdin`, sends requests to the server and prints out the responses.

Both example applications handle key/value pairs of C-style strings. The server is able to handle multiple clients at once.

## Compiling
The project can be compiled using the provided `Makefile`. It successfully compiles both with recent versions of `g++` or `clang++`.

## Running
The project can run both on Linux as well as on macOS. However, due to certain inconsistencies in the implementation of POSIX functionality in the Darwin kernel, it currently crashes and/or deadlocks when run on macOS with multiple clients at once. There seems to be a bug in `pthread_cond_wait()`.

On Linux systems, the programs run just fine.

`make run` spawns a server as well as a client which enqueues requests to the server such as "INSERT key value", "DELETE key", "GET key" or "READ_BUCKET idx".

`run_many.sh` can be run after firing up a server in a terminal (which takes one integer argument deciding how many buckets the hashtable has) and spawns a couple of clients spamming the server with thousands of requests. After they are done, the hashtable should, again, be empty.

