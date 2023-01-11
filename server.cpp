#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <mutex>

#include <atomic>
//#include <condition_variable>
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <thread>
#include "server.h"
#include "hashtable.h"
#include "mutex.h"

#include <sstream>
#include <iomanip>

#if defined(__linux__)
    #define MAP_HASSEMAPHORE 0
#endif

static_assert(std::atomic<pid_t>::is_always_lock_free, "atomic<pid_t> is not always lock free and can't be used in shared memory");

using namespace std::chrono_literals;

volatile bool running = true;
std::mutex cout_lock;
void signal_handler([[maybe_unused]] int signal) {
    running = false;
}

// Our HashTable which is managed by the server
std::unique_ptr<HashTable<std::string, std::string>> table;

// from https://gist.github.com/miguelmota/4fc9b46cf21111af5fa613555c14de92
std::string uint8_to_hex_string(const uint8_t* v, const size_t s) {
    std::stringstream ss;

    ss << std::hex << std::setfill('0');

    for(size_t i = 0; i < s; ++i) {
        if(static_cast<int>(v[i]) == 0)
            break;
        ss << std::hex << std::setw(2) << static_cast<int>(v[i]);
    }
    return ss.str();
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

std::ostream& operator<<(std::ostream& output, const Message& other) {
    //output << "Message ";
    switch(other.mode) {
        case Message::GET:
            output << "(GET)";
            break;
        case Message::INSERT:
            output << "(INSERT)";
            break;
        case Message::READ_BUCKET:
            output << "(READ_BUCKET)";
            break;
        case Message::DELETE:
            output << "(DELETE)";
            break;
        case Message::RESPONSE:
            output << "(RESPONSE)";
            break;
        default:
            output << "(DEFAULT)";
            break;
    }

    output << " KeyLength: " << other.key.size() << " Key (Hex): ";
    output << uint8_to_hex_string(other.key.data(), other.key.size());
    output << " DataLength: " << other.data.size() << " Data (Hex): ";
    output << uint8_to_hex_string(other.data.data(), other.data.size());
    output << " PID: " << other.client_id;
    output << " Success: " << other.success;
    output << " Ready: " << other.ready.test();
    output << std::endl;

    return output;
}

void receiveMsg(Mailbox<slots>* mailbox) {
    while(true) {
        // TODO: Prevent a deadlock if the client does not exist anymore?

        auto elem = mailbox->msgs.pop();
        if(elem) {
            auto& msg = elem->first;
            auto idx = elem->second;

            // Check for exit condition
            if(msg.mode == Message::EXIT)
                break;

            respond(mailbox, idx, msg);

            //{
            //    std::scoped_lock<std::mutex> lock(cout_lock);
            //    std::cout << "thread " << std::this_thread::get_id() << " handling slot " << idx << std::endl;
            //}
        }
    }
}

void respond(Mailbox<slots>* mailbox, size_t idx, Message msg) {
    // TODO Check for malformed requests
    Message response{};
    response.mode = Message::RESPONSE;
    response.client_id.store(msg.client_id.load());
    response.key = msg.key;

    switch(msg.mode) {
        case Message::GET: {
            auto result = table->get(uint8_to_string(msg.key.data(), msg.key.size()));
            if(result) {
                auto& value = *result;
                memcpy(response.data.data(), value.c_str(), strlen(value.c_str()) + 1);
                response.success = true;

                //{
                //    std::scoped_lock<std::mutex> lock(cout_lock);
                //    std::cout << "GET: returned from HashTable: " << uint8_to_string(response.data.data(), response.data.size()) << std::endl;
                //}
            } else {
                // Entry was not found in the HashTable
                response.data[0] = 0;
                response.success = false;
                //{
                //    std::scoped_lock<std::mutex> lock(cout_lock);
                //    std::cout << "GET: did not find key " << uint8_to_string(response.key.data(), response.key.size()) << " in the HT!" << std::endl;
                //}
            }
            break;
            }
        case Message::INSERT: {
            bool result = table->insert(uint8_to_string(msg.key.data(), msg.key.size()), uint8_to_string(msg.data.data(), msg.data.size()));
            if(result) {
                memcpy(response.data.data(), &result, sizeof(bool));
                response.success = true;
            } else {
                response.data[0] = 0;
                response.success = false;
                //{
                //    std::scoped_lock<std::mutex> lock(cout_lock);
                //    //std::cout << "INSERT: returned from HashTable: " << uint8_to_string(response.data.data(), response.data.size()) << std::endl;
                //    //std::cout << "INSERT: returned from HashTable: " << response.data.data() << std::endl;
                //}
            }
            break;
            }
        case Message::READ_BUCKET:
            // TODO
            break;
        case Message::DELETE: {
            auto result = table->remove(uint8_to_string(msg.key.data(), msg.key.size()));
            if(result) {
                auto& value = *result;
                memcpy(response.data.data(), value.c_str(), strlen(value.c_str()) + 1);
                response.success = true;

                //{
                //    std::scoped_lock<std::mutex> lock(cout_lock);
                //    std::cout << "DELETE: returned from HashTable: " << uint8_to_string(response.data.data(), response.data.size()) << std::endl;
                //}
            } else {
                // Entry was not found in the HashTable
                response.data[0] = 0;
                response.success = false;
                //{
                //    std::scoped_lock<std::mutex> lock(cout_lock);
                //    std::cout << "DELETE: did not find key " << uint8_to_string(response.key.data(), response.key.size()) << " in the HT!" << std::endl;
                //}
            }
            }
            break;
        case Message::RESPONSE:
            // Should never happen
            //return;
            break;
        default:
            // Should never happen
            std::cout << "default case!" << std::endl;
            //return;
            break;
    }

    
    // Respond
    mailbox->mutexes[idx].lock();
    // Wait for the response's slot to be ready to be written to
    // which is the case if the ready flag is false
    //
    // ready must be false here, wait for that condition
    // AND: pid must be 0/false here
    while(mailbox->responses[idx].ready.test() || mailbox->responses[idx].client_id != 0) {
        if((errno = pthread_cond_wait(&(mailbox->rcvs[idx]), mailbox->mutexes[idx].getHandle())) != 0) {
            std::perror("server.cpp: pthread_cond_wait()");
            std::exit(-1);
        }
    }
    // Write the response in the request's slot in the responses' array
    mailbox->responses[idx].key  = response.key;
    mailbox->responses[idx].data = response.data;
    mailbox->responses[idx].success = response.success;
    mailbox->responses[idx].ready.test_and_set();
    mailbox->responses[idx].client_id.store(response.client_id.load());
    
    // Notify the client(s)
    //if((errno = pthread_cond_signal(&(mailbox->rcvs[idx]))) != 0) {
    if((errno = pthread_cond_broadcast(&(mailbox->rcvs[idx]))) != 0) {
        std::perror("server.cpp: pthread_cond_signal()");
        std::exit(-1);
    }
    mailbox->mutexes[idx].unlock();
}

int main(int argc, char* argv[]) {
    std::cout << "Hello from the server!" << std::endl;

    //std::copy(argv, argv + argc, std::ostream_iterator<char*>(std::cout, "\n"));

    if(argc != 2) {
        std::cerr << "Wrong number of arguments! Expecting exactly \
            one integer argument (size of the HashTable). \
            If 0 is provided, the HashTable dynamically grows \
            and shrinks (which is currently only working \
            with a single client)." << std::endl;
        return EXIT_FAILURE;
    }

    // TODO: --resizable flag

    size_t tableSize{0};
   
    try {
        tableSize = std::stoul(argv[1]);
    } catch(std::exception const& e) {
        std::cerr << e.what() << std::endl;
    }

    // Our HashTable which is managed by the server
    if(tableSize == 0) {
        table = std::make_unique<HashTable<std::string, std::string>>();
    } else {
        table = std::make_unique<HashTable<std::string, std::string>>(tableSize, false);
    }

    //table->insert("test", "hallo");
    //(*table)["test"] = "test";
    //(*table)["test"] = "hallo";

    //for(size_t i = 100001; i <= 300000; ++i) {
    //    auto tmp = std::to_string(i);
    //    table->insert(tmp, tmp);
    //}

    //table->print_table();

    // The name associated with the shared memory object
    const char* name = "/shm_ipc";
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    
    if(shm_fd == -1) {
        perror("shm_open() failed");
        return EXIT_FAILURE;
    }

    //const size_t slots = 3; // Number of message slots, defined in message.h
    const size_t page_size = static_cast<size_t>(getpagesize());
    const size_t mmap_size = sizeof(MMap<slots>) + sizeof(Message) * slots;
    const size_t num_pages = (mmap_size / page_size) + 1; // + 1 page just to make sure we have enough memory

    // Truncate / Extend the shared memory object to the needed size in multiples of PAGE_SIZE
    if(ftruncate(shm_fd, static_cast<off_t>(num_pages * page_size)) != 0) {
        perror("ftruncate() failed");
        shm_unlink(name);
        return EXIT_FAILURE;
    }

    void* shared_mem_ptr = mmap(NULL,
            sizeof(MMap<slots>) + sizeof(Message) * slots,
            //num_pages * page_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_HASSEMAPHORE,
            shm_fd,
            0);
    if(shared_mem_ptr == MAP_FAILED) {
        perror("mmap() failed");
        shm_unlink(name);
        return EXIT_FAILURE;
    }

    // Initialize and cast the shared memory pointer to struct MMap*
    //MMap* shared_mem = reinterpret_cast<MMap*>(shared_mem_ptr);
    MMap<slots>* shared_mem = new(shared_mem_ptr) MMap<slots>{}; // Placement new
    //MMap* shared_mem = new(shared_mem_ptr) MMap{slots}; // Placement new
    //for(int i = 0; i < slots; ++i) {
    //    std::cout << shared_mem->mailbox.msgs[i] << "\n";
    //}

    // Initialize the shared memory / mailbox

    std::cout << "Size of shared memory (in bytes): " << num_pages * page_size << std::endl;
    std::cout << "Size of mailbox (in bytes): " << sizeof(MMap<slots>) + sizeof(Message) * slots << std::endl;

    Mailbox<slots>* mailbox_ptr = &(shared_mem->mailbox);


    std::signal(SIGINT, signal_handler);
    // Spawn a thread for each slot in the Mailbox
    std::array<std::thread, slots> threads{};

    for(size_t i = 0; i < slots; ++i) {
        threads[i] = std::thread{receiveMsg, mailbox_ptr};
    }

    //std::cout << thread_running.is_lock_free() << std::endl;
    //std::cout << std::atomic<bool>::is_always_lock_free << std::endl;

    // Main loop
    while(running) {
        std::scoped_lock lock(cout_lock);
        std::this_thread::sleep_for(2s);
        std::cout << "CircularBuffer:" << std::endl;
        mailbox_ptr->msgs.printBuffer();
        std::cout << "---------------" << std::endl;
        std::cout << "Responses:" << std::endl;
        for(auto& i : mailbox_ptr->responses) {
            std::cout << i;
        }
        std::cout << "---------------" << std::endl;
        std::cout << "HashTable:" << std::endl;
        std::cout << "Size: " << table->size() << "; Capacity: " << table->capacity() << "; Load Factor: " << table->load_factor() << std::endl;
        std::cout << "---------------" << std::endl;
        
        // Periodically notify waiting threads since they somehow
        // seem to sometimes wait on the same condition variable
        // TODO: Not needed anymore, notifications are being broadcast to
        // all clients and server threads
        //for(size_t i = 0; i < slots; ++i) {
        //    //if((errno = pthread_cond_signal(&(mailbox_ptr->rcvs[i]))) != 0) {
        //    if((errno = pthread_cond_broadcast(&(mailbox_ptr->rcvs[i]))) != 0) {
        //        std::perror("main(): pthread_cond_signal()");
        //        std::exit(-1);
        //    }
        //}
    }
    // Signal the threads to return
    Message exit_msg{};
    exit_msg.mode = Message::EXIT;
    for(size_t i = 0; i < slots; ++i) {
        mailbox_ptr->msgs.push_back(exit_msg); 
    }

    // Wait for all threads to finish their jobs
    for(auto& t : threads) {
        t.join();
    }

    table->print_table();

    // Destroy the MMap struct
    shared_mem->~MMap();

    shm_unlink(name);
    munmap(shared_mem_ptr, sizeof(MMap<slots>) + sizeof(Message) * slots);
    close(shm_fd);

    return 0;
}
