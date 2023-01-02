#pragma once

#include <iostream>

#include <array>
#include <concepts>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map> // For hashes
#include <vector>

// Maximum load factor
#define ALPHA_MAX 0.75
#define GROWTH_FACTOR 2
#define SHRINK_FACTOR 2

/**
 * Hashable concept as found at https://en.cppreference.com/w/cpp/language/constraints
 */ 
template<typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};


/**
 * A hashtable storing key-value pairs supporting concurrent operations.
 * Hash collisions are resolved by chaining via linked lists for each bucket in the table.
 */ 
template <typename K, typename V> requires Hashable<K> && std::equality_comparable<K> && std::copy_constructible<V>
class HashTable {
    /**
     * Proxy class to enable correct assignments via the subscript operator
     */
    struct Proxy {
        private:
            HashTable<K, V>& table;
            const K* key;
            V* element;

        public:
            // Constructor
            Proxy(HashTable<K, V>& table, const K* key, V* element) : table(table), key(key), element(element) { }

            // Assignment via subscript in class HashTable
            void operator=(V rhs) {
                // TODO Somehow lock bucket?

                //++table._size;
                auto entry = table.get(*key);
                if(entry) {
                    // TODO: Test this thoroughly... perhaps values should be stored "by reference", not "by value"
                    *element = rhs;
                    //table.remove(*key);
                    //table.insert(*key, rhs);

                } else {
                    //table.insert(*key, *element);
                    table.insert(*key, rhs);
                }

                // TODO: Make static?
                //resize();
            }

            friend std::ostream& operator<<(std::ostream& os, const Proxy& prox) {
                os << *prox.element;
                return os;
            }

            // Return the wrapped value
            operator V() const { return *element; };
            operator V() { return *element; };

        };
   
    public:
        /**
         * The HashTable's default constructor.
         * Initializes a HashTable with default space for 1024 elements
         * which is also resizable.
         */
        HashTable() : _size(0), _capacity(1024), _resizable(true) {
            _storage = std::make_unique<Node[]>(1024);
            for(size_t i = 0; i < 1024; ++i) {
                _storage[i] = Node();
            }
        }

        /**
         * Constructor.
         * Initializes a HashTable with space for the given amount of elements.
         *
         * @param cap number of elements the HashTable should have space for after initialization
         */
        HashTable(size_t cap, bool resizable = false) : _size(0), _capacity(cap), _resizable(resizable) {
            _storage = std::make_unique<Node[]>(cap);
            for(size_t i = 0; i < cap; ++i) {
                _storage[i] = Node();
            }
        }

        /**
         * Inserts `value` into the HashTable given the `key`.
         * If the entry exists already, insert() returns false and does
         * not overwrite the existing entry.
         * To overwrite a value, use the subscript operator or remove the
         * existing one first.
         *
         * @param key the entry's key
         * @param value the value which is to be inserted into the HashTable
         * @return True if successful, false otherwise
         */
        bool insert(K key, V value) {
            if(get(key))
                return false;

            std::unique_lock lock(_mutex);

            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];

            bucket.l.push_back(std::make_pair(key, value));

            ++_size;
            resize();

            return true;
        }

        /**
         * Tries to fetch the value associated with the given `key`.
         *
         * @param key the key of the entry which should be retrieved
         * @return An optional which contains a value if the key existed.
         */
        std::optional<V> get(const K& key) const {
            size_t hash_val = hash(key);

            std::shared_lock lock(_mutex);

            auto& bucket = _storage[hash_val];

            auto result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            if(result != bucket.l.end()) {
                return std::make_optional((*result).second);
            } else {
                return std::nullopt;
                //return std::optional<V>{};
            }
        }

        /**
         * Tries to remove and return the value associated with the given `key`.
         *
         * @param key the key of the entry which should be removed from the HashTable
         * @return An optional which contains a value if the key existed.
         */
        std::optional<V> remove(const K& key) {
            size_t hash_val = hash(key);

            std::unique_lock lock(_mutex);

            auto& bucket = _storage[hash_val];

            auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            if(result != bucket.l.end()) {
                //std::cout << "found key " << key << std::endl;
                --_size;
                auto ret = std::make_optional(std::move((*result).second));
                bucket.l.erase(result);
                resize(true);
                return ret;
            } else {
                //std::cout << "did not find key " << key << std::endl;
                return std::nullopt;
                //std::cout << "remove: does not contain " << key << ", result is " << (*result).first << std::endl;
                //return std::optional<V>{};
            }
            return std::make_optional((*result).second);
        }

        /**
         * Returns a vector containing all keys present in the HashTable.
         */
        std::vector<K> getKeys() const {
            std::vector<K> vec = std::vector<K>();
            //std::cout << _size << std::endl;
            //std::cout << _capacity << std::endl;

            std::shared_lock lock(_mutex);

            for(size_t i = 0; i < _capacity; ++i) {
                //std::cout << "Bucket " << i << " size: " << _storage[i].l.size() << std::endl;
                if(!_storage[i].l.empty()) {
                    for(auto& elem : _storage[i].l) {
                        vec.push_back(elem.first);
                    }
                }
            }
            return vec;
        }
        
        /**
         * Returns a vector containing all values present in the HashTable.
         */
        std::vector<V> getValues() const {
            std::vector<V> vec = std::vector<V>();

            std::shared_lock lock(_mutex);

            for(size_t i = 0; i < _capacity; ++i) {
                if(!_storage[i].l.empty()) {
                    for(auto& elem : _storage[i].l) {
                        vec.push_back(elem.second);
                    }
                }
            }
            return vec;
        }

        /**
         * Returns the current size/number of elements of the HashTable.
         */
        size_t size() const {
            //std::shared_lock lock(_mutex);
            return _size;
        }

        /**
         * Returns the current capacity of the HashTable.
         */
        size_t capacity() const {
            //std::shared_lock lock(_mutex);
            return _capacity;
        }

        /**
         * Returns the current load factor of the HashTable.
         * The load factor is calculated as n / k, n being the number of entries occupied
         * in the HashTable, k being the number of buckets.
         */
        double load_factor() const {
            //std::shared_lock lock(_mutex);
            if(_capacity == 0) return 0.0;

            return static_cast<double>(_size) / static_cast<double>(_capacity);
        }

        /**
         * Returns a reference to the value the provided key is mapped to.
         * If an assignment happens, the assignment is proxied to the assignment operator
         * of the Proxy struct.
         * Note that no resizing of the HashTable happens when using the subscript operator
         * in combination with an assignment.
         */
        Proxy operator[](const K key) {
            std::unique_lock lock(_mutex);
            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];

            auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            return Proxy(*this, &key, &((*result).second));
        }
        //V operator[](const K key) {
        //    size_t hash_val = hash(key);
        //    auto& bucket = _storage[hash_val];

        //    auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
        //            [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

        //    return V(Proxy(*this, &((*result).second)));
        //}
        //Proxy operator[](const K key) const {
        //    size_t hash_val = hash(key);
        //    auto& bucket = _storage[hash_val];

        //    auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
        //            [key](const std::pair<K, V>& elem) { return elem.first == key; } );

        //    return Proxy(*this, &((*result).second));
        //}
        V& operator[](const K key) const {
            std::shared_lock lock(_mutex);
            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];

            auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            return V(Proxy(*this, &key, &((*result).second)));
        }
        
        /**
         * Returns a reference to the value the provided key is mapped to.
         */ 
        //V& operator[](const K key) const {
        //    std::cout << "operator[] const called" << std::endl;
        //    size_t hash_val = hash(key);
        //    auto& bucket = _storage[hash_val];

        //    //auto& result = std::find(std::begin(bucket), std::end(bucket), key);
        //    auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
        //            [key](const std::pair<K, V>& elem) { return elem.first == key; } );

        //    return &((*result).second);
        //}

        //// Inserts / overwrites the entry at the given key
        //V& operator[](const K key) {
        //    std::cout << "operator[] called" << std::endl;
        //    size_t hash_val = hash(key);
        //    auto& bucket = _storage[hash_val];

        //    //auto& result = std::find(std::begin(bucket), std::end(bucket), key);
        //    auto result = std::find_if(bucket.l.begin(), bucket.l.end(),
        //            [key](const std::pair<K, V>& elem) { return elem.first == key; } );

        //    if(result == bucket.l.end()) {
        //        ++_size;
        //    }
        //    return (*result).second;
        //}

        void print_table() const {
            // TODO
            auto keys = getKeys();
            auto values = getValues();

            //for(auto&& [k, v] : zip(keys, values)) {
            //    std::cout << k << " -> " << v << std::endl;
            //}
            auto ki = keys.begin();
            auto vi = values.begin();
            for(; ki < keys.end(); ++ki, ++vi) {
                std::cout << *ki << " -> " << *vi << std::endl;
            }
        }

    private:
        /**
        * The internal list's node/bucket type
        */
        struct Node {
            // TODO
            int lock = 0;

            std::list<std::pair<K, V>> l = std::list<std::pair<K, V>>();
        };
        //using Node = std::list<std::pair<K, V>>;
        
        size_t _size;
        size_t _capacity;
        const bool _resizable;
        //std::array<std::list<T>, _size> _storage;
        //std::list<V>** _storage;
        std::unique_ptr<Node[]> _storage;
        //std::unordered_map<std::string, int> _storage;
        mutable std::shared_mutex _mutex;
        mutable std::mutex _gmutex;

        size_t hash(const K key) const {
            std::hash<K> hash_fn;
            size_t hash_val = hash_fn(key) % _capacity;

            return hash_val;
        }

        
        /**
         * Searches for an element with the given key and returns a pair consisting
         * of a reference to the bucket and an iterator to the element.
         * TODO
         */
        //std::pair<Node&, std::iterator> find(const K& key) {
        //std::pair<Node&, std::iterator<std::input_iterator_tag, std::pair<K, V>>> find(const K& key) {
        //std::pair<Node&, typename std::list<std::pair<K, V>>::iterator> find(const K& key) {
        //std::pair<Node&, std::__list_iterator<std::pair<K, V>, void*>> find(const K& key) {
        //    size_t hash_val = hash(key);
        //    auto& bucket = _storage[hash_val];

        //    auto result = std::find_if(bucket.l.begin(), bucket.l.end(),
        //            [key](const std::pair<K, V>& elem) { return elem.first == key; } );

        //    return std::make_pair(bucket, result);
        //}

        // Collects all (key, value) pairs into one list, emptying all buckets
        std::list<std::pair<K, V>> collectPairs() {
            std::list<std::pair<K, V>> l = std::list<std::pair<K, V>>();

            for(size_t i = 0; i < _capacity; ++i) {
                    l.splice(l.begin(), _storage[i].l);
            }

            //for(auto& elem : l) {
            //    std::cout << elem.first << " : " << elem.second << std::endl;
            //}

            return l;
        }

        void resize(bool shrink=false) {
            // TODO: Lock all buckets
            std::lock_guard<std::mutex> lock(_gmutex);

            auto lf = load_factor();

            if(!shrink && lf >= ALPHA_MAX) {
                // Grow the HashTable
                std::cout << "lf " << load_factor() << " >= " << ALPHA_MAX << ", growing..." << std::endl;

                auto old_elems = collectPairs();

                // Create new storage, rehash all old entrys
                _storage.release();
                _storage = std::make_unique<Node[]>(_capacity * GROWTH_FACTOR);
                _capacity = _capacity * GROWTH_FACTOR;
                _size = 0;

                //_mutex.unlock();
                for(size_t i = 0; i < _capacity; ++i) {
                    _storage[i] = Node();
                }
                for(auto& elem : old_elems) {
                    //insert(elem.first, std::move(elem.second)); 
                    insert_unsafe(elem.first, std::move(elem.second));
                }
                //_mutex.lock();

            } else if(shrink && lf <= 0.25) { //(ALPHA_MAX/4)) {
                // Shrink the HashTable
                std::cout << "lf " << load_factor() << " <= " << /*(ALPHA_MAX/4)*/ 0.25 << ", shrinking..." << std::endl;
                
                auto old_elems = collectPairs();
                // Create new storage, rehash all old entrys
                _storage.release();
                _storage = std::make_unique<Node[]>(_capacity / SHRINK_FACTOR);
                _capacity = _capacity / SHRINK_FACTOR;
                _size = 0;
                for(size_t i = 0; i < _capacity; ++i) {
                    _storage[i] = Node();
                }
                for(auto& elem : old_elems) {
                    //insert(elem.first, std::move(elem.second)); 
                    insert_unsafe(elem.first, std::move(elem.second));
                }

            }

            // TODO: Unlock all buckets
        }

        // Used when resizing
        void insert_unsafe(K key, V value) {
            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];

            bucket.l.push_back(std::make_pair(key, value));

            ++_size;
        }

};
