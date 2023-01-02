#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
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

// hex start
// from https://gist.github.com/miguelmota/4fc9b46cf21111af5fa613555c14de92
#include <sstream>
#include <iomanip>

using namespace std::chrono_literals;

volatile bool running = true;
std::atomic<bool> thread_running{true};
//std::condition_variable stop_threads;
std::mutex cout_lock;
void signal_handler([[maybe_unused]] int signal) {
    running = false;
}

// Our HashTable which is managed by the server
std::unique_ptr<HashTable<std::string, std::string>> table;

std::string uint8_to_hex_string(const uint8_t* v, const size_t s) {
    std::stringstream ss;

    ss << std::hex << std::setfill('0');

    for(int i = 0; i < s; ++i) {
        if(static_cast<int>(v[i]) == 0)
            break;
        ss << std::hex << std::setw(2) << static_cast<int>(v[i]);
    }
    return ss.str();
}

// hex end

std::string uint8_to_string(const uint8_t* v, const size_t len) {
    std::stringstream ss;
    for(int i = 0; i < len; ++i) {
        if(static_cast<int>(v[i]) == 0)
            break;
        ss << v[i];
    }
    return ss.str();
}

std::ostream& operator<<(std::ostream& output, const Message& other) {
    output << "Message ";
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

    //output << " KeyLength: " << other.key_length << " Key (Hex): ";
    //output << uint8_to_hex_string(other.key, other.key_length);
    //output << " DataLength: " << other.data_length << " Data (Hex): ";
    //output << uint8_to_hex_string(other.data, other.data_length);
    //output << std::endl;
    output << " KeyLength: " << other.key.size() << " Key (Hex): ";
    output << uint8_to_hex_string(other.key.data(), other.key.size());
    output << " DataLength: " << other.data.size() << " Data (Hex): ";
    output << uint8_to_hex_string(other.data.data(), other.data.size());
    //output << " Read: " << other.read.load();
    output << std::endl;

    return output;
}

//using namespace std::chrono_literals;
void receiveMsg(Mailbox* mailbox) {
    while(thread_running.load(std::memory_order_relaxed)) {
        // Peek whether the current slot still has to be read by a client
        // This is necessary because of the case of a wrap-around in the
        // CircularBuffer which could lead to the response being overwritten.
        // TODO: Prevent a deadlock if the client does not exist anymore
        //const auto& peek_elem = mailbox->msgs.peek();
        //while(elem && elem->first.mode == Message::RESPONSE && elem->first.read.load() == false) {
        //    //sleep(1); // TODO: Better solution
        //                // Futures would be great, but with shared memory and
        //                // multiple processes they could lead to problems
        //    std::this_thread::sleep_for(10ms);
        //    elem = mailbox->msgs.peek();
        //}
        //if(peek_elem && peek_elem->first->mode == Message::RESPONSE && peek_elem->first->read.load() == false)
        //if(peek_elem && peek_elem->first.mode == Message::RESPONSE && peek_elem->first.read.load() == false)
        //    peek_elem->first.read.wait(false);

        auto elem = mailbox->msgs.pop();
        if(elem) {
            auto& msg = elem->first;
            auto idx = elem->second;
            // Set the Message's slot's mode to RESPONSE as to prevent the server
            // to overwrite it prematurely TODO
            mailbox->msgs.at(idx).mode = Message::RESPONSE;

            respond(mailbox, idx, msg);

            {
                std::scoped_lock<std::mutex> lock(cout_lock);
                std::cout << "thread " << std::this_thread::get_id() << " handling slot " << idx << std::endl;
            }
            //std::this_thread::sleep_for(1000ms);
        }
    }
}

void respond(Mailbox* mailbox, int idx, Message msg) {
    //std::cout << "Hello from the worker handling " << idx << ": " << msg.mode << "!" << std::endl;
    // TODO Check for malformed requests
    Message response{};
    response.mode = Message::RESPONSE;
    //response.key_length = msg.key_length;
    //response.key = msg.key;
    //memcpy(response.key, msg.key, response.key_length);
    response.key = msg.key;

    switch(msg.mode) {
        case Message::GET: {
            auto result = table->get(uint8_to_string(msg.key.data(), msg.key.size()));
            if(result) {
                auto& value = *result;
                //response.data_length = value.length(); //+ 1;
                memcpy(response.data.data(), value.c_str(), value.length());

                {
                    std::scoped_lock<std::mutex> lock(cout_lock);
                    std::cout << "GET: returned from HashTable: " << uint8_to_string(response.data.data(), response.data.size()) << std::endl;
                }
            } else {
                // Entry was not found in the HashTable
                //response.data_length = 0;
                {
                    std::scoped_lock<std::mutex> lock(cout_lock);
                    std::cout << "GET: did not find key " << uint8_to_string(response.key.data(), response.key.size()) << " in the HT!" << std::endl;
                }
            }
            break;
            }
        case Message::INSERT: {
            bool result = table->insert(uint8_to_string(msg.key.data(), msg.key.size()), uint8_to_string(msg.data.data(), msg.data.size()));
            memcpy(response.data.data(), &result, sizeof(bool));
            {
                std::scoped_lock<std::mutex> lock(cout_lock);
                //std::cout << "INSERT: returned from HashTable: " << uint8_to_string(response.data.data(), response.data.size()) << std::endl;
                //std::cout << "INSERT: returned from HashTable: " << response.data.data() << std::endl;
            }
            break;
            }
        case Message::READ_BUCKET:
            break;
        case Message::DELETE: {
            auto result = table->remove(uint8_to_string(msg.key.data(), msg.key.size()));
            if(result) {
                auto& value = *result;
                //response.data_length = value.length(); //+ 1;
                memcpy(response.data.data(), value.c_str(), value.length());

                {
                    std::scoped_lock<std::mutex> lock(cout_lock);
                    std::cout << "DELETE: returned from HashTable: " << uint8_to_string(response.data.data(), response.data.size()) << std::endl;
                }
            } else {
                // Entry was not found in the HashTable
                //response.data_length = 0;
                {
                    std::scoped_lock<std::mutex> lock(cout_lock);
                    std::cout << "DELETE: did not find key " << uint8_to_string(response.key.data(), response.key.size()) << " in the HT!" << std::endl;
                }
            }
            }
            break;
        case Message::RESPONSE:
            break;
        default:
            break;
    }
    // Write the response in the request's slot
    // TODO Lock CircularBuffer?
    //mailbox->msgs[idx] = response;
    mailbox->msgs[idx] = std::move(response);
}

int main(int argc, char* argv[]) {
    std::cout << "Hello from the server!" << std::endl;

    //std::copy(argv, argv + argc, std::ostream_iterator<char*>(std::cout, "\n"));

    if(argc != 2) {
        std::cerr << "Wrong number of arguments! Expecting exactly \
            one integer argument (size of the HashTable)." << std::endl;
        return EXIT_FAILURE;
    }

    size_t tableSize{0};
   
    try {
        tableSize = std::stoul(argv[1]);
    } catch(std::exception const& e) {
        std::cerr << e.what() << std::endl;
    }

    // Our HashTable which is managed by the server
    //auto table = std::make_unique<HashTable<std::string, std::string>>(tableSize);
    table = std::make_unique<HashTable<std::string, std::string>>(tableSize);

    table->insert("test", "hallo");
    (*table)["test"] = "test";
    (*table)["test"] = "hallo";

    table->print_table();

    // The name associated with the shared memory object
    const char* name = "/shm_ipc";
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    
    if(shm_fd == -1) {
        perror("shm_open() failed");
        return EXIT_FAILURE;
    }

    //const size_t slots = 3;//10; //64; // Number of message slots, defined in message.h
    const size_t page_size = static_cast<size_t>(getpagesize());
    const size_t mmap_size = sizeof(MMap) + sizeof(Message) * slots; // TODO: Check if + sizeof(Mailbox) is missing
    const size_t num_pages = (mmap_size / page_size) + 1; // + 1 page just to make sure we have enough memory

    // Truncate / Extend the shared memory object to the needed size in multiples of PAGE_SIZE
    if(ftruncate(shm_fd, static_cast<off_t>(num_pages * page_size)) != 0) {
        perror("ftruncate() failed");
        shm_unlink(name);
        return EXIT_FAILURE;
    }

    //MMap* shared_mem = (MMap*)mmap(0, sizeof(MMap), PROT_READ, MAP_SHARED, shm_fd, 0);
    void* shared_mem_ptr = mmap(NULL,
            sizeof(MMap) + sizeof(Message) * slots,
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
    MMap* shared_mem = new(shared_mem_ptr) MMap{slots};

    // Initialize the shared memory / mailbox

    //MMap tmp{slots};
    //*shared_mem = std::move(MMap{slots});
    //*shared_mem = std::move(tmp); // is std::move useful here?
    //*shared_mem = tmp;

    std::cout << "Size of shared memory (in bytes): " << num_pages * page_size << std::endl;
    std::cout << "Size of mailbox (in bytes): " << sizeof(MMap) + sizeof(Message) * slots << std::endl;

    //shared_mem->mailbox.msgs.push_back(3);

    Mailbox* mailbox_ptr = &(shared_mem->mailbox);

    //const char test_data[] = "hallo";
    //Message test_msg{};
    //test_msg.mode = Message::RESPONSE;
    //test_msg.key_length = strlen(test_data) + 1;
    //memcpy(test_msg.key, test_data, test_msg.key_length);
    //test_msg.data_length = test_msg.key_length;
    //memcpy(test_msg.data, test_data, test_msg.data_length);
    //mailbox_ptr->msgs.push_back(test_msg);


    std::signal(SIGINT, signal_handler);
    // Spawn a thread for each slot in the Mailbox
    std::array<std::thread, slots> threads{};

    for(size_t i = 0; i < slots; ++i) {
        threads[i] = std::thread{receiveMsg, mailbox_ptr};
        //std::thread t{respond, mailbox, idx, msg};
        //t.detach();
    }

    // Main loop
    while(running) {
        //receiveMsg(mailbox_ptr);
        //std::cout << "returned from receiveMsg" << std::endl;
        //sleep(5);
        std::this_thread::sleep_for(2s);
        mailbox_ptr->msgs.printBuffer();
        table->print_table();
    }

    // Signal the threads to return
    thread_running.store(false, std::memory_order_relaxed);

    // Wait for all threads to finish their jobs
    for(auto& t : threads) {
        t.join();
    }

    // Destroy the MMap struct
    shared_mem->~MMap();

    shm_unlink(name);
    munmap(shared_mem_ptr, sizeof(MMap) + sizeof(Message) * slots);
    close(shm_fd);

    return 0;
}
