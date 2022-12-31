#include <algorithm>
#include <atomic>
#include <iostream>
#include <sstream>
#include <csignal>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "client.h"
#include "message.h"
//#include "hashtable.h"

/**
 * An example client storing C-style strings in a HashTable managed by
 * the example server using C-style strings as keys.
 */

bool running = true;
void signal_handler([[maybe_unused]] int signal) {
    running = false;
}

Message sendMsg(Mailbox* mailbox, const enum Message::mode_t mode, const char* key, const char* value) {
    //Message test_msg{Message::GET, strlen(test_data) + 1, {0}};
    //// Poor memory
    //memcpy(test_msg.data, test_data, strlen(test_data) + 1);

    Message msg{};

    // Prepare the message
    switch(mode) {
        case Message::GET:
            msg.mode = Message::GET;
            //msg.key_length = strlen(key); // + 1;
            //memcpy(msg.key, key, msg.key_length);
            memcpy(msg.key.data(), key, strlen(key));
            break;
        case Message::INSERT:
            msg.mode = Message::INSERT;
            //msg.key_length = strlen(key); // + 1;
            //memcpy(msg.key, key, msg.key_length);
            memcpy(msg.key.data(), key, strlen(key));
            //msg.data_length = strlen(value); // + 1;
            //memcpy(msg.data, value, msg.data_length);
            memcpy(msg.data.data(), value, strlen(value));
            break;
        case Message::READ_BUCKET:
            msg.mode = Message::READ_BUCKET;
            // Key is interpreted as the bucket's index in this case
            //msg.key_length = strlen(key); // + 1;
            memcpy(msg.key.data(), key, strlen(key));
            break;
        case Message::DELETE:
            msg.mode = Message::DELETE;
            //msg.key_length = strlen(key); // + 1;
            //memcpy(msg.key, key, msg.key_length);
            memcpy(msg.key.data(), key, strlen(key));
            break;
        default:
            throw std::invalid_argument("mode must be either GET, INSERT, READ_BUCKET or DELETE");            
            break;
    }
    // Send the message
    // TODO: Lock, check for return value of -1
    auto idx = static_cast<size_t>(mailbox->msgs.push_back(std::move(msg)));

    // Wait for a response
    Message ret{};
    while(mailbox->msgs[idx].mode != Message::RESPONSE) {
        std::cout << "Waiting..." << std::endl;
    }
    ret = mailbox->msgs[idx];
    //mailbox->msgs[idx].read.store(true);
    //mailbox->msgs[idx].read.notify_all();
    mailbox->msgs.getPtr(idx)->read.store(true);
    mailbox->msgs.getPtr(idx)->read.notify_all();

    return ret;
}

int main(int argc, char** argv) {
    std::cout << "Hello from the client!" << std::endl;
    
    // The name associated with the shared memory object
    const char* name = "/shm_ipc";
    //int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    int shm_fd = shm_open(name, O_RDWR, 0666);
    
    if(shm_fd == -1) {
        perror("shm_open() failed");
        return EXIT_FAILURE;
    }
    
    const size_t slots = 10; //64; // Number of message slots
    const size_t page_size = static_cast<size_t>(getpagesize());
    const size_t mmap_size = sizeof(MMap) + sizeof(Message) * slots; // TODO: Check if + sizeof(Mailbox) is missing
    const size_t num_pages = (mmap_size / page_size) + 1; // + 1 page just to make sure we have enough memory

    // Truncate / Extend the shared memory object to the needed size in multiples of PAGE_SIZE
    //if(ftruncate(shm_fd, static_cast<off_t>(num_pages * page_size)) != 0) {
    //    perror("ftruncate() failed");
    //    shm_unlink(name);
    //    return EXIT_FAILURE;
    //}

    void* shared_mem_ptr = mmap(NULL,
            sizeof(MMap) + sizeof(Message) * slots,
            PROT_READ | PROT_WRITE,
            MAP_SHARED, //| MAP_HASSEMAPHORE,
            shm_fd,
            0);
    if(shared_mem_ptr == MAP_FAILED) {
        perror("mmap() failed");
        //shm_unlink(name);
        return EXIT_FAILURE;
    }

    // Cast the shared memory pointer to struct MMap*
    MMap* shared_mem = reinterpret_cast<MMap*>(shared_mem_ptr);
    Mailbox* mailbox_ptr = &(shared_mem->mailbox);


    std::signal(SIGINT, signal_handler);
    // Main loop
    do {
        std::vector<std::string> input{};
        std::string raw_input;
        std::getline(std::cin, raw_input);
        std::stringstream ss(raw_input);

        std::string param;
        while(ss >> param) {
            input.push_back(param);
        }
        if(input.size() == 0)
            break;
            //continue;

        // Convert the first parameter / command to lowercase
        std::transform(input[0].begin(), input[0].end(), input[0].begin(),
                [](char c){ return std::tolower(c); });


        // Send the Message
        //sendMsg(mailbox_ptr, Message::INSERT, "client", "ichbins");
        try {
            if(input[0] == "insert") {
                if(input.size() != 3 || input[1].length() > MAX_LENGTH_KEY || input[2].length() > MAX_LENGTH_VAL) {
                    throw std::invalid_argument("INSERT expects exactly 2 arguments (key and value) with a maximum length of "
                            + std::to_string(MAX_LENGTH_KEY) + " and " + std::to_string(MAX_LENGTH_VAL) + " respectively");
                }
                sendMsg(mailbox_ptr, Message::INSERT, input[1].c_str(), input[2].c_str());
            } else if(input[0] == "get") {
                if(input.size() < 2 || input[1].length() > MAX_LENGTH_KEY) {
                    throw std::invalid_argument("GET expects 1 argument (the key) with a maximum length of "
                            + std::to_string(MAX_LENGTH_KEY));
                }
                sendMsg(mailbox_ptr, Message::GET, input[1].c_str());
            } else if(input[0] == "read_bucket") {
                if(input.size() < 2 || input[1].length() > MAX_LENGTH_KEY) {
                    throw std::invalid_argument("READ_BUCKET expects 1 argument (the bucket's number)");
                }
                // TODO
                std::cout << "READ_BUCKET case" << std::endl;

            } else if(input[0] == "delete") {
                if(input.size() < 2 || input[1].length() > MAX_LENGTH_KEY) {
                    throw std::invalid_argument("DELETE expects 1 argument (the key) with a maximum length of "
                            + std::to_string(MAX_LENGTH_KEY));
                }
                sendMsg(mailbox_ptr, Message::DELETE, input[1].c_str());

            } else {
                throw std::invalid_argument("the first argument must be either GET, INSERT, READ_BUCKET or DELETE");            
            }
        } catch(std::invalid_argument e) {
            std::cerr << e.what() << std::endl;
            continue;
        }
        // Wait for the server's response

    } while(running);

    // Tidy up
    //shm_unlink(name);
    //munmap(shared_mem_ptr, sizeof(MMap) + sizeof(Message) * slots);
    close(shm_fd);

    return 0;
}
