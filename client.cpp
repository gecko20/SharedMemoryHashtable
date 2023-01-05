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

#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

#if defined(__linux__)
    #define MAP_HASSEMAPHORE 0
#endif

/**
 * An example client storing C-style strings in a HashTable managed by
 * the example server using C-style strings as keys.
 */
using namespace std::chrono_literals;

bool running = true;
void signal_handler([[maybe_unused]] int signal) {
    running = false;
}

std::string uint8_to_string(const uint8_t* v, const size_t len) {
    std::stringstream ss;
    for(size_t i = 0; i < len; ++i) {
        if(static_cast<int>(v[i]) == 0)
            break;
        ss << v[i];
    }
    return ss.str();
}

Message sendMsg(Mailbox<slots>* mailbox, const enum Message::mode_t mode, const char* key, const char* value) {
//Message sendMsg(Mailbox* mailbox, const enum Message::mode_t mode, const char* key, const char* value) {
    Message msg{};

    // Prepare the message
    switch(mode) {
        case Message::GET:
            msg.mode = Message::GET;
            memcpy(msg.key.data(), key, strlen(key));
            break;
        case Message::INSERT:
            msg.mode = Message::INSERT;
            memcpy(msg.key.data(), key, strlen(key));
            memcpy(msg.data.data(), value, strlen(value));
            break;
        case Message::READ_BUCKET:
            msg.mode = Message::READ_BUCKET;
            // Key is interpreted as the bucket's index in this case
            memcpy(msg.key.data(), key, strlen(key));
            break;
        case Message::DELETE:
            msg.mode = Message::DELETE;
            memcpy(msg.key.data(), key, strlen(key));
            break;
        case Message::EXIT:
            running = false;
            break;
        default:
            throw std::invalid_argument("mode must be either GET, INSERT, READ_BUCKET or DELETE");            
            break;
    }
    // Send the message
    // TODO: Check for return value of -1
    size_t idx;
    //idx = static_cast<size_t>(mailbox->msgs.push_back(std::move(msg)));
    idx = static_cast<size_t>(mailbox->msgs.push_back(msg)); // TODO: Maybe return a condition variable to check on, too?
    while(idx == static_cast<size_t>(-1)) {
        // push_back failed due to the Message buffer being full, wait and try again
        std::cout << "push_back() failed: Message buffer full" << std::endl;
        
        std::this_thread::sleep_for(10ms);
        idx = static_cast<size_t>(mailbox->msgs.push_back(msg));
    }

    // Wait for a response
    Message ret{};
    //Message::mode_t m{Message::DEFAULT};
    // Wait for the response to be marked as ready
    //mailbox->msgs[idx].ready.wait(false);
    while(mailbox->msgs[idx].mode != Message::RESPONSE) {

    }
    mailbox->msgs[idx].ready.wait(false);
    //while(!mailbox->msgs[idx].ready.load()) {

    //}

    //while(mailbox->msgs[idx].mode != Message::RESPONSE) {
//    while(m != Message::RESPONSE) {
//        //std::cout << "Waiting..." << std::endl;
//        try {
//            m = mailbox->msgs[idx].mode;
//        } catch(std::out_of_range& e) {
//            //std::cerr << e.what() << " caught in CircularBuffer::operator[] with idx = " << idx << std::endl;
//            std::cout << e.what() << " caught in CircularBuffer::operator[] with idx = " << idx << std::endl;
//        }
//    }
    //mailbox->msgs.lock(); 
//    try {
    ret = mailbox->msgs[idx];
//            } catch(std::out_of_range& e) {
//                //std::cerr << e.what() << " caught in CircularBuffer::operator[] with idx = " << idx << std::endl;
//                std::cout << e.what() << " caught in CircularBuffer::operator[] with idx = " << idx << std::endl;
//            }
    //mailbox->msgs[idx].read.store(true);
    //mailbox->msgs[idx].read.notify_all();
    //mailbox->msgs.getPtr(idx)->read.store(true);
    //mailbox->msgs.getPtr(idx)->read.notify_all();

    //mailbox->msgs.unlock();

    // Reset the response's slot
    mailbox->msgs[idx].mode = Message::DEFAULT;
    mailbox->msgs[idx].ready.store(false);
    mailbox->msgs[idx].ready.notify_all();
    return ret;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    std::cout << "Hello from the client!" << std::endl;
    
    // The name associated with the shared memory object
    const char* name = "/shm_ipc";
    //int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    int shm_fd = shm_open(name, O_RDWR, 0666);
    
    if(shm_fd == -1) {
        perror("shm_open() failed");
        return EXIT_FAILURE;
    }
    
    //const size_t slots = 10; // Number of message slots, defined in message.h
    //const size_t page_size = static_cast<size_t>(getpagesize());
    //const size_t mmap_size = sizeof(MMap) + sizeof(Message) * slots;
    //const size_t num_pages = (mmap_size / page_size) + 1; // + 1 page just to make sure we have enough memory

    // Truncate / Extend the shared memory object to the needed size in multiples of PAGE_SIZE
    //if(ftruncate(shm_fd, static_cast<off_t>(num_pages * page_size)) != 0) {
    //    perror("ftruncate() failed");
    //    shm_unlink(name);
    //    return EXIT_FAILURE;
    //}

    void* shared_mem_ptr = mmap(NULL,
            sizeof(MMap<slots>) + sizeof(Message) * slots,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_HASSEMAPHORE,
            shm_fd,
            0);
    if(shared_mem_ptr == MAP_FAILED) {
        perror("mmap() failed");
        //shm_unlink(name);
        return EXIT_FAILURE;
    }

    // Cast the shared memory pointer to struct MMap*
    MMap<slots>* shared_mem = reinterpret_cast<MMap<slots>*>(shared_mem_ptr);
    Mailbox<slots>* mailbox_ptr = &(shared_mem->mailbox);

    std::signal(SIGINT, signal_handler);
    Message response{};
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
        try {
            if(input[0] == "insert") {
                if(input.size() != 3 || input[1].length() > MAX_LENGTH_KEY || input[2].length() > MAX_LENGTH_VAL) {
                    throw std::invalid_argument("INSERT expects exactly 2 arguments (key and value) with a maximum length of "
                            + std::to_string(MAX_LENGTH_KEY) + " and " + std::to_string(MAX_LENGTH_VAL) + " respectively");
                }
                response = sendMsg(mailbox_ptr, Message::INSERT, input[1].c_str(), input[2].c_str());
            } else if(input[0] == "get") {
                if(input.size() < 2 || input[1].length() > MAX_LENGTH_KEY) {
                    throw std::invalid_argument("GET expects 1 argument (the key) with a maximum length of "
                            + std::to_string(MAX_LENGTH_KEY));
                }
                response = sendMsg(mailbox_ptr, Message::GET, input[1].c_str());
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
                response = sendMsg(mailbox_ptr, Message::DELETE, input[1].c_str());

            } else {
                throw std::invalid_argument("the first argument must be either GET, INSERT, READ_BUCKET or DELETE");            
            }
        } catch(std::invalid_argument e) {
            std::cerr << e.what() << std::endl;
            continue;
        }
        // Print out the response
        if(input[0] == "get") {
                std::cout << "GET " << input[1];
                //if(response.success) {
                if(response.success.load()) {
                    std::cout << " -> " << uint8_to_string(response.data.data(), response.data.size()) << " succeeded";
                } else {
                    std::cout << " failed";
                }
                std::cout << std::endl;
        } else if(input[0] == "insert") {
                std::cout << "INSERT " << input[1] << " -> " << input[2];
                //if(response.success) {
                if(response.success.load()) {
                    std::cout << " succeeded";
                } else {
                    std::cout << " failed";
                }
                std::cout << std::endl;
        } else if(input[0] == "delete") {
                std::cout << "DELETE " << input[1];
                //if(response.success) {
                if(response.success.load()) {
                    std::cout << " -> " << uint8_to_string(response.data.data(), response.data.size()) << " succeeded";
                } else {
                    std::cout << " failed";
                }
                std::cout << std::endl;
        } else if(input[0] == "read_bucket") {
                // TODO
        } else {
            // default
        }

    } while(running);

    // Tidy up
    //shm_unlink(name);
    //munmap(shared_mem_ptr, sizeof(MMap) + sizeof(Message) * slots);
    close(shm_fd);

    return 0;
}
