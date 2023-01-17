#include <algorithm>
#include <atomic>
#include <iostream>
#include <sstream>
#include <csignal>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "client.h"
#include "message.h"

#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

#if defined(__linux__)
    #define MAP_HASSEMAPHORE 0
#endif

static_assert(std::atomic<pid_t>::is_always_lock_free, "atomic<pid_t> is not always lock free and can't be used in shared memory");

/**
 * An example client storing C-style strings in a HashTable managed by
 * the example server using C-style strings as keys.
 */
using namespace std::chrono_literals;

bool running = true;
pid_t client_id;
std::mutex cout_lock;
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
    Message msg{};
    msg.client_id.store(client_id);

    // Prepare the message
    switch(mode) {
        case Message::GET:
            msg.mode = mode;
            memcpy(msg.key.data(), key, strlen(key) + 1);
            break;
        case Message::INSERT:
            msg.mode = mode;
            memcpy(msg.key.data(), key, strlen(key) + 1);
            memcpy(msg.data.data(), value, strlen(value) + 1);
            break;
        case Message::READ_BUCKET:
            msg.mode = mode;
            // Key is interpreted as the bucket's index in this case
            memcpy(msg.key.data(), key, strlen(key) + 1);
            break;
        case Message::CLOSE_SHM:
            msg.mode = mode;
            memcpy(msg.key.data(), key, strlen(key) + 1);
            break;
        case Message::DELETE:
            msg.mode = mode;
            memcpy(msg.key.data(), key, strlen(key) + 1);
            break;
        case Message::EXIT:
            running = false;
            break;
        default:
            throw std::invalid_argument("mode must be either GET, INSERT, READ_BUCKET or DELETE");            
            break;
    }
    // Send the message
    size_t idx = static_cast<size_t>(-1);
    idx = static_cast<size_t>(mailbox->msgs.push_back(msg));
    while(idx == static_cast<size_t>(-1)) {
        // push_back failed due to the Message buffer being full, wait and try again
        //std::cout << "push_back() failed: Message buffer full" << std::endl;
        
        std::this_thread::sleep_for(15ms);
        idx = static_cast<size_t>(mailbox->msgs.push_back(msg));
    }

    Message ret{};
    // If mode is CLOSE_SHM or EXIT we don't have to wait for a reponse
    if(mode == Message::CLOSE_SHM || mode == Message::EXIT) {
        return ret;
    }

    // Wait for a response
    mailbox->mutexes[idx].lock();
    // ready must be true here, wait for that condition
    // AND: pid must be equal to the client's pid, so it knows
    // that this response is meant for it, not for some other client
    while(!mailbox->responses[idx].ready.test() || mailbox->responses[idx].client_id != client_id) {
        if((errno = pthread_cond_wait(&(mailbox->rcvs[idx]), mailbox->mutexes[idx].getHandle())) != 0) {
            std::perror("client.cpp: pthread_cond_wait()");
            std::exit(-1);
        }
    }
    ret.mode = Message::RESPONSE;
    ret.success = mailbox->responses[idx].success;
    ret.key = mailbox->responses[idx].key;
    ret.data = mailbox->responses[idx].data;

    // Reset ready flag in order to notify a potentially waiting server thread
    // which wants to reuse the slot for another response
    // Also reset the response's PID to 0 so the server knows it is allowed to
    // reuse the slot
    mailbox->responses[idx].ready.clear();
    mailbox->responses[idx].client_id.store(0);
    // Notify the server's thread(s)
    //if((errno = pthread_cond_signal(&(mailbox->rcvs[idx]))) != 0) {
    if((errno = pthread_cond_broadcast(&(mailbox->rcvs[idx]))) != 0) {
        std::perror("client.cpp: pthread_cond_signal()");
        std::exit(-1);
    }
    mailbox->mutexes[idx].unlock();

    return ret;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    //std::ios_base::sync_with_stdio(false);
    std::cout << "Hello from the client!" << std::endl;

    client_id = getpid();
    
    // The name associated with the shared memory object
    const char* name = "/shm_ipc";
    int shm_fd = shm_open(name, O_RDWR, 0666);
    
    if(shm_fd == -1) {
        perror("shm_open() failed");
        return EXIT_FAILURE;
    }
    
    void* shared_mem_ptr = mmap(NULL,
            sizeof(MMap<slots>) + sizeof(Message) * slots,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_HASSEMAPHORE,
            shm_fd,
            0);
    if(shared_mem_ptr == MAP_FAILED) {
        perror("mmap() failed");
        return EXIT_FAILURE;
    }

    // Cast the shared memory pointer to struct MMap*
    MMap<slots>* shared_mem = reinterpret_cast<MMap<slots>*>(shared_mem_ptr);
    Mailbox<slots>* mailbox_ptr = &(shared_mem->mailbox);

    std::signal(SIGINT, signal_handler);

    Message response;
    // Main loop
    do {
        response = Message();
        std::vector<std::string> input{};
        std::string raw_input;
        std::getline(std::cin, raw_input);
        std::stringstream ss(raw_input);

        std::string param;
        while(ss >> param) {
            input.push_back(param);
        }
        // Exit the program if no input was given
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
                response = sendMsg(mailbox_ptr, Message::READ_BUCKET, input[1].c_str());

            } else if(input[0] == "delete") {
                if(input.size() < 2 || input[1].length() > MAX_LENGTH_KEY) {
                    throw std::invalid_argument("DELETE expects 1 argument (the key) with a maximum length of "
                            + std::to_string(MAX_LENGTH_KEY));
                }
                response = sendMsg(mailbox_ptr, Message::DELETE, input[1].c_str());

            } else {
                throw std::invalid_argument("the first argument must be either GET, INSERT, READ_BUCKET or DELETE");            
            }
        } catch(std::invalid_argument& e) {
            std::cerr << e.what() << std::endl;
            continue;
        }
        // Print out the response
        {
        std::scoped_lock lock(cout_lock);
        if(input[0] == "get") {
                std::cout << "GET " << input[1];
                if(response.success) {
                    std::cout << " -> " << uint8_to_string(response.data.data(), response.data.size()) << " succeeded";
                } else {
                    std::cout << " failed";
                }
                std::cout << std::endl;
        } else if(input[0] == "insert") {
                std::cout << "INSERT " << input[1] << " -> " << input[2];
                if(response.success) {
                    std::cout << " succeeded";
                } else {
                    std::cout << " failed";
                }
                std::cout << std::endl;
        } else if(input[0] == "delete") {
                std::cout << "DELETE " << input[1];
                if(response.success) {
                    std::cout << " -> " << uint8_to_string(response.data.data(), response.data.size()) << " succeeded";
                } else {
                    std::cout << " failed";
                }
                std::cout << std::endl;
        } else if(input[0] == "read_bucket") {
            if(response.success) {
                // 1. Establish a new shared memory segment, given the name
                //    provided in response.key with length response.data
                // 2. Copy the vector<std::string, std::string> located there
                //    in the memory of the client
                // 3. Close the memory segment
                // 4. Send the server a message which notifies it to remove
                //    the shared memory segment
                // The name associated with the shared memory object
                std::string shm_name = uint8_to_string(response.key.data(), response.key.size());
                std::cout << shm_name << std::endl;
                std::string shm_length_str = uint8_to_string(response.data.data(), response.data.size());
                size_t shm_length{0};
                try {
                    shm_length = std::stoul(shm_length_str);
                } catch(std::exception const& e) {
                    std::cerr << "client.cpp: read_bucket(): " << e.what() << std::endl;
                    std::exit(-1);
                }
                int shm_fd = shm_open(shm_name.c_str(), O_RDONLY, 0666);
                    
                if(shm_fd == -1) {
                    perror("client.cpp: read_bucket(): shm_open() failed");
                    return EXIT_FAILURE;
                }

                void* shm_ptr = mmap(NULL,
                                     shm_length,
                                     PROT_READ,
                                     MAP_SHARED,
                                     shm_fd,
                                     0);
                if(shm_ptr == MAP_FAILED) {
                    perror("client.cpp: read_bucket(): mmap() failed");
                    std::exit(-1);
                }
                auto shm = reinterpret_cast<std::pair<std::array<uint8_t, MAX_LENGTH_KEY>, std::array<uint8_t, MAX_LENGTH_VAL>>*>(shm_ptr);
                // Copy the vector
                std::vector<std::pair<std::array<uint8_t, MAX_LENGTH_KEY>, std::array<uint8_t, MAX_LENGTH_VAL>>> vec;
                auto ptr = shm;
                // TODO For the love of God, I need to change this
                // If this doesn't work, encode the number of elements
                // in the shared memory segment's name
                // NOTE: Also, this would only work in 64-bit platforms,
                //       so it should be changed to some ptr_t
                while(*((uint64_t*)ptr) != 0) {
                    vec.push_back(*ptr);
                    ++ptr;
                }

                // Now, send another message notifying the server that we are done
                // reading the bucket
                sendMsg(mailbox_ptr, Message::CLOSE_SHM, shm_name.c_str());

                // Finally output the response
                std::cout << "READ_BUCKET " << input[1] << " successful:" << std::endl;
                for(auto& [k, v] : vec) {
                    std::cout << uint8_to_string(k.data(), k.size()) << " -> " \
                              << uint8_to_string(v.data(), v.size()) << std::endl;
                }
            } else {
                // read_bucket() failed
                std::cout << "read_bucket() failed" << std::endl;
                std::cout << uint8_to_string(response.key.data(), response.key.size()) << std::endl;
            }

        } else {
            // default
        }
        }

    } while(running);

    // Tidy up
    //shm_unlink(name);
    //munmap(shared_mem_ptr, sizeof(MMap) + sizeof(Message) * slots);
    close(shm_fd);

    return 0;
}
