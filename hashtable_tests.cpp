#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <unordered_map> // for hashing std::string
#include <string>
#include "doctest.h"
#include "hashtable.h"
#include "circular_buffer.h"

//int factorial(int number) { return number <= 1 ? number : factorial(number - 1) * number; }
//
//TEST_CASE("testing the factorial") {
//    CHECK(factorial(1) == 1);
//    CHECK(factorial(2) == 2);
//    CHECK(factorial(3) == 6);
//    CHECK(factorial(10) == 3628800);
//}

TEST_CASE("adding new elements to HashTable") {
    HashTable<std::string, int> table{5};

    REQUIRE(table.size() == 0);
    REQUIRE(table.capacity() >= 5);

    SUBCASE("adding one element to empty HashTable") {
        table.insert("3", 1337);

        REQUIRE(table.size() == 1);
        REQUIRE(table.capacity() >= 5);

        auto elem = table.get("3");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 1337);
    }
    SUBCASE("adding/changing elements in HashTable via subscript") {
        table["3"] = 1336;
        REQUIRE(table.size() == 1);
        REQUIRE(table.capacity() >= 5);
        auto elem = table.get("3");
        REQUIRE(elem.has_value() == true); // why does has_value() return false in this case??
        CHECK(*elem == 1336);

        table["4"] = 4;
        REQUIRE(table.size() == 2);
        REQUIRE(table.capacity() >= 5);
        elem = table.get("4");
        REQUIRE(elem.has_value() == true);
        CHECK(*elem == 4);

        table["hallowelt"] = 1234567;
        REQUIRE(table.size() == 3);
        REQUIRE(table.capacity() >= 5);
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
    HashTable<int, int> table{10};
    
    for(size_t i = 0; i < 3; ++i) {
        if(i != 2)
            table.insert(static_cast<int>(i), static_cast<int>(i));
    }

    REQUIRE(table.size() == 2);
    REQUIRE(table.capacity() == 10);
    
    SUBCASE("remove missing key.") {
        auto elem = table.remove(5);
        REQUIRE(elem.has_value() == false);
        REQUIRE(table.size() == 2);
        REQUIRE(table.capacity() == 10);
    }

    SUBCASE("remove existing key, shrinking.") {
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

        REQUIRE(table.capacity() == (10 / SHRINK_FACTOR));
    }
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
