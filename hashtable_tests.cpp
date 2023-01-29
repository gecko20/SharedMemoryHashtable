#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <unordered_map> // for hashing std::string
#include <string>
#include <thread>
#include "doctest.h"
#include "hashtable.h"
#include "circular_buffer.h"

#include <optional>


TEST_CASE("adding new elements to the HashTable") {
    HashTable<std::string, int> table{5, true};

    REQUIRE(table.size() == 0);
    REQUIRE(table.capacity() >= 5);

    SUBCASE("adding one element to the empty HashTable") {
        table.insert("3", 1337);

        REQUIRE(table.size() == 1);

        auto elem = table.get("3");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 1337);
    }
    SUBCASE("adding/changing elements in the HashTable via subscript") {
        table["3"] = 1336;
        REQUIRE(table.size() == 1);
        auto elem = table.get("3");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 1336);

        table["4"] = 4;
        REQUIRE(table.size() == 2);
        elem = table.get("4");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 4);

        table["hallowelt"] = 1234567;
        REQUIRE(table.size() == 3);
        elem = table.get("hallowelt");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 1234567);
    }
    SUBCASE("adding many elements such that the HashTable needs to be resized.") {
        for(size_t i = 0; i < 5; ++i) {
            table.insert(std::to_string(i), static_cast<int>(i));
            //table.insert(i, static_cast<int>(i));
        }

        REQUIRE(table.size() == 5);
        if(table.isResizable())
            REQUIRE(table.capacity() > 5);

        auto elem = table.get("2");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 2);
        elem = table.get("4");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 4);
        elem = table.get("10");
        REQUIRE(elem.has_value() == false);
        CHECK(elem.has_value() == false);

        // TODO: Check whether all elements are still present
    }
    SUBCASE("make sure no duplicates in the HashTable are allowed via insert()") {
        table.insert("2", 2);
        auto elem = table.get("2");
        CHECK(*elem == 2);
        table.insert("2", 3);
        elem = table.get("2");
        CHECK(*elem == 2);
    }
}

TEST_CASE("removing elements from the HashTable") {
    HashTable<int, int> table{10, true};
    
    for(size_t i = 0; i < 2; ++i) {
        table.insert(static_cast<int>(i), static_cast<int>(i));
    }

    REQUIRE(table.size() == 2);
    REQUIRE(table.capacity() == 10);
    
    SUBCASE("remove missing key.") {
        auto elem = table.remove(5);
        REQUIRE(elem.has_value() == false);
        REQUIRE(table.size() == 2);
        REQUIRE(table.capacity() == 5);
    }

    SUBCASE("remove existing key.") {
        table.insert(2, 2);

        REQUIRE(table.size() == 3);
        REQUIRE(table.capacity() == 10);

        auto elem = table.remove(2);
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 2);
        REQUIRE(table.size() == 2);
        auto elem2 = table.get(2);
        REQUIRE(elem2.has_value() == false);
        REQUIRE(table.size() == 2);
    }
}

TEST_CASE("growing and shrinking the HashTable") {
    HashTable<int, int> table{10, true};

    REQUIRE(table.size() == 0);
    REQUIRE(table.capacity() == 10);

    for(size_t i = 0; i < 7; ++i) {
        table.insert(static_cast<int>(i), static_cast<int>(i));
    }
    REQUIRE(table.size() == 7);
    REQUIRE(table.capacity() == 10);
    
    table.insert(8, 8);
    REQUIRE(table.size() == 8);
    REQUIRE(table.capacity() == 20);

    for(size_t i = 0; i < 5; ++i) {
        table.remove(static_cast<int>(i));
    }
    REQUIRE(table.size() == 3);
    REQUIRE(table.capacity() == 20);

    table.remove(5);
    REQUIRE(table.size() == 2);
    REQUIRE(table.capacity() == 10);
}

TEST_CASE("stress tests (dynamic)") {
    const size_t slots = 12;
    HashTable<int, int> table{slots * 1000000, true};

    SUBCASE("parallel access with different keys/values for each thread (dynamic)") {
        std::cout << "Running stress tests: Parallel access with different keys/values (dynamic)" << std::endl;
        std::array<std::thread, slots> threads{};

        auto f = [&table](int x) { 
            bool               insRes;
            std::optional<int> remRes;
            std::optional<int> getRes;
            //std::optional<int> getRes;
            //$((1 + 100000*$i)) $(( 100000 + 100000*$i)))
            for(int i = 1 + 1000000 * x; i <= 1000000 + 1000000*x; ++i) {
                getRes = table.get(i);
                REQUIRE(getRes.has_value() == false);
                insRes = table.insert(i, i);
                CHECK(insRes == true);
                insRes = table.insert(i, i);
                CHECK(insRes == false);
                getRes = table.get(i);
                REQUIRE(getRes.has_value() == true);
                CHECK(*getRes == i);
                remRes = table.remove(i);
                getRes = table.get(i);
                REQUIRE(getRes.has_value() == false);

                CHECK(remRes.has_value() == true);
            }
        };
    
        for(size_t i = 0; i < slots; ++i) {
            threads[i] = std::thread{f, i};
        }

        for(auto& t : threads) {
            t.join();
        }

        table.print_table();
        REQUIRE(table.size() == 0);
    }

    SUBCASE("parallel access with same keys/values for each thread (dynamic)") {
        std::cout << "Running stress tests: Parallel access with same keys/values (dynamic)" << std::endl;
        std::array<std::thread, slots> threads{};

        auto f = [&table]() { 
            //bool               insRes;
            std::optional<int> remRes;
            std::optional<int> getRes;
            for(int i = 1; i <= 1000000; ++i) {
                getRes = table.get(i);
                //insRes = table.insert(i, i);
                table.insert(i, i);
                getRes = table[i];
                remRes = table.remove(i);
                getRes = table.get(i);
            }
        };
    
        for(size_t i = 0; i < slots; ++i) {
            threads[i] = std::thread{f};
        }

        for(auto& t : threads) {
            t.join();
        }

        table.print_table();
        REQUIRE(table.size() == 0);

    }
    
    SUBCASE("spamming insertions (dynamic)") {
        std::cout << "Running stress tests: Spamming insertions (dynamic)" << std::endl;
        std::array<std::thread, slots> threads{};

        auto f = [&table]() { 
            //bool               insRes;
            //std::optional<int> remRes;
            //std::optional<int> getRes;
            for(int i = 1; i <= 1000000; ++i) {
                table.insert(i, i);
            }
        };
    
        for(size_t i = 0; i < slots; ++i) {
            threads[i] = std::thread{f};
        }

        for(auto& t : threads) {
            t.join();
        }

        //table.print_table();
        //REQUIRE(table.size() == slots * 1023124);
        // TODO: check size
        // TODO: capacity

    }
}

TEST_CASE("stress tests (static)") {
    const size_t slots = 12;
    HashTable<int, int> table{slots * 1000000, false};

    SUBCASE("parallel access with different keys/values for each thread (static)") {
        std::cout << "Running stress tests: Parallel access with different keys/values (static)" << std::endl;
        std::array<std::thread, slots> threads{};

        auto f = [&table](int x) { 
            bool               insRes;
            std::optional<int> remRes;
            std::optional<int> getRes;
            //std::optional<int> getRes;
            //$((1 + 100000*$i)) $(( 100000 + 100000*$i)))
            for(int i = 1 + 1000000 * x; i <= 1000000 + 1000000*x; ++i) {
                getRes = table.get(i);
                REQUIRE(getRes.has_value() == false);
                insRes = table.insert(i, i);
                CHECK(insRes == true);
                insRes = table.insert(i, i);
                CHECK(insRes == false);
                getRes = table.get(i);
                REQUIRE(getRes.has_value() == true);
                CHECK(*getRes == i);
                remRes = table.remove(i);
                getRes = table.get(i);
                REQUIRE(getRes.has_value() == false);

                CHECK(remRes.has_value() == true);
            }
        };
    
        for(size_t i = 0; i < slots; ++i) {
            threads[i] = std::thread{f, i};
        }

        for(auto& t : threads) {
            t.join();
        }

        table.print_table();
        REQUIRE(table.size() == 0);
    }

    SUBCASE("parallel access with same keys/values for each thread (static)") {
        std::cout << "Running stress tests: Parallel access with same keys/values (static)" << std::endl;
        std::array<std::thread, slots> threads{};

        auto f = [&table]() { 
            //bool               insRes;
            std::optional<int> remRes;
            std::optional<int> getRes;
            for(int i = 1; i <= 1000000; ++i) {
                getRes = table.get(i);
                //insRes = table.insert(i, i);
                table.insert(i, i);
                getRes = table[i];
                remRes = table.remove(i);
                getRes = table.get(i);
            }
        };
    
        for(size_t i = 0; i < slots; ++i) {
            threads[i] = std::thread{f};
        }

        for(auto& t : threads) {
            t.join();
        }

        table.print_table();
        REQUIRE(table.size() == 0);

    }
    
    SUBCASE("spamming insertions (static)") {
        std::cout << "Running stress tests: Spamming insertions (static)" << std::endl;
        std::array<std::thread, slots> threads{};

        auto f = [&table]() { 
            //bool               insRes;
            //std::optional<int> remRes;
            //std::optional<int> getRes;
            for(int i = 1; i <= 1000000; ++i) {
                table.insert(i, i);
            }
        };
    
        for(size_t i = 0; i < slots; ++i) {
            threads[i] = std::thread{f};
        }

        for(auto& t : threads) {
            t.join();
        }

        //table.print_table();
        //REQUIRE(table.size() == slots * 1023124);
        // TODO: check size
        // TODO: capacity

    }
}

TEST_CASE("parallel insertions via the subscript operator (dynamic)") {
    std::cout << "Running stress tests: Parallel access via the subscript operator (dynamic)" << std::endl;
    const size_t slots = 12;
    HashTable<int, int> table{slots * 1000000, true};
    std::array<std::thread, slots> threads{};

    auto f = [&table](int x) {
        //bool               insRes;
        std::optional<int> remRes;
        std::optional<int> getRes;
        
        for(int i = 1 + 1000000 * x; i <= 1000000 + 1000000*x; ++i) {
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == false);
            table[i] = i;
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == true);
            CHECK(*getRes == i);
            table[i] = i + 1;
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == true);
            REQUIRE(*getRes == i + 1);
            remRes = table.remove(i);
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == false);

            CHECK(remRes.has_value() == true);
        }
    };
        
    for(size_t i = 0; i < slots; ++i) {
        threads[i] = std::thread{f, i};
    }

    for(auto& t : threads) {
        t.join();
    }

    table.print_table();
    REQUIRE(table.size() == 0);
}

TEST_CASE("parallel insertions via the subscript operator (static)") {
    std::cout << "Running stress tests: Parallel access via the subscript operator (static)" << std::endl;
    const size_t slots = 12;
    HashTable<int, int> table{slots * 1000000, false};
    std::array<std::thread, slots> threads{};

    auto f = [&table](int x) {
        //bool               insRes;
        std::optional<int> remRes;
        std::optional<int> getRes;
        
        for(int i = 1 + 1000000 * x; i <= 1000000 + 1000000*x; ++i) {
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == false);
            table[i] = i;
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == true);
            CHECK(*getRes == i);
            table[i] = i + 1;
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == true);
            REQUIRE(*getRes == i + 1);
            remRes = table.remove(i);
            getRes = table.get(i);
            REQUIRE(getRes.has_value() == false);

            CHECK(remRes.has_value() == true);
        }
    };
        
    for(size_t i = 0; i < slots; ++i) {
        threads[i] = std::thread{f, i};
    }

    for(auto& t : threads) {
        t.join();
    }

    table.print_table();
    REQUIRE(table.size() == 0);
}

TEST_CASE("Add elements to the CircularBuffer") {
    CircularBuffer<int, 5> cb{};

    REQUIRE(cb.isEmpty() == true);
    REQUIRE(cb.isFull() == false);
    REQUIRE(cb.size() == 0);
    REQUIRE(cb.capacity() == 5);

    
    SUBCASE("Add single element on empty buffer") {
        CHECK(cb.push_back(1) != -1);

        REQUIRE(cb.isEmpty() == false);
        REQUIRE(cb.isFull() == false);
        REQUIRE(cb.size() == 1);
        REQUIRE(cb.capacity() == 5);
    }
    SUBCASE("Add single element on non-empty buffer") {
        CHECK(cb.push_back(1) != -1);
        CHECK(cb.push_back(2) != -1);

        REQUIRE(cb.isEmpty() == false);
        REQUIRE(cb.isFull() == false);
        REQUIRE(cb.size() == 2);
        REQUIRE(cb.capacity() == 5);
    }
    SUBCASE("Add elements until the buffer is full") {
        for(int i = 1; i <= 5; ++i) {
            CHECK(cb.push_back(i) != -1);
        }

        CHECK(cb.push_back(6) == -1);
    
        cb.printBuffer();

        REQUIRE(cb.isEmpty() == false);
        REQUIRE(cb.isFull() == true);
        REQUIRE(cb.size() == 5);
        REQUIRE(cb.capacity() == 5);
    }
}

TEST_CASE("Remove elements from the CircularBuffer") {
    CircularBuffer<int, 5> cb{};

    REQUIRE(cb.isEmpty() == true);
    REQUIRE(cb.isFull() == false);
    REQUIRE(cb.size() == 0);
    REQUIRE(cb.capacity() == 5);

    SUBCASE("Remove from empty buffer") {
        auto elem = cb.pop();

        REQUIRE(elem.has_value() == false);
        REQUIRE(cb.size() == 0);
        REQUIRE(cb.capacity() == 5);
    }
    SUBCASE("Remove from non-empty buffer") {
        cb.push_back(0);
        cb.push_back(0);
        cb.push_back(1);

        auto elem = cb.pop();

        REQUIRE(elem.has_value() == true);
        CHECK((*elem).first == 0);
        REQUIRE(cb.size() == 2);
        REQUIRE(cb.capacity() == 5);
        elem = cb.pop();
        CHECK((*elem).first == 0);
        elem = cb.pop();
        CHECK((*elem).first == 1);
        elem = cb.pop();
        REQUIRE(elem.has_value() == false);
        REQUIRE(cb.size() == 0);
        REQUIRE(cb.capacity() == 5);
    }
}

TEST_CASE("Add and remove elements to/from the CircularBuffer") {
    CircularBuffer<int, 5> cb{};

    REQUIRE(cb.isEmpty() == true);
    REQUIRE(cb.isFull() == false);
    REQUIRE(cb.size() == 0);
    REQUIRE(cb.capacity() == 5);

    cb.push_back(0);
    cb.push_back(1);
    cb.push_back(2);
    cb.pop();
    cb.push_back(3);
    cb.pop();

    auto elem = cb.pop();

    REQUIRE(elem.has_value() == true);
    CHECK((*elem).first == 2);

    cb.printBuffer();
}
