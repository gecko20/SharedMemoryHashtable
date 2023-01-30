#pragma once

#include <iostream>

#include <array>
#include <atomic>
#include <concepts>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map> // For hashes
#include <vector>

// Maximum load factor
#define ALPHA_MAX 0.75
#define ALPHA_MIN 0.10
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
 * Synchronization of concurrent operations is done via reader/writer locks, i.e. std::shared_mutex.
 */ 
template <typename K, typename V> requires Hashable<K> && std::equality_comparable<K> && std::copy_constructible<V>
class HashTable {
    /**
     * Proxy class to enable correct assignments via the subscript operator
     */
    struct Proxy {
        private:
            HashTable<K, V>& table;
            const K key;
            const V element;

        public:
            // Constructor
            Proxy(HashTable<K, V>& table, const K key, const V element) : table(table), key(key), element(element) { }

            // Assignment via subscript in class HashTable
            void operator=(V rhs) {
                auto entry = table.get(key);
                if(entry) {
                    table.remove(key);
                    table.insert(key, rhs);
                } else {
                    table.insert(key, rhs);
                }
            }

            friend std::ostream& operator<<(std::ostream& os, const Proxy& prox) {
                os << *prox.element;
                return os;
            }

            // Return the wrapped value
            operator V() const { return element; };
            operator V() { return element; };
        };
   
    public:
        /**
         * The HashTable's default constructor.
         * Initializes a HashTable with default space for 4 elements
         * which is also resizable.
         */
        HashTable() : _size(0),
                      _capacity(4),
                      _resizable(true),
                      _storage(std::make_unique<Node[]>(4)),
                      _mutex() { }

        /**
         * Constructor.
         * Initializes a HashTable with space for the given amount of elements.
         *
         * @param cap number of elements the HashTable should have space for after initialization
         * @param resizable decides whether the HashTable should dynamically resize itself or keep a static amount of buckets
         */
        HashTable(size_t cap, bool resizable = false) : _size(0),
                                                        _capacity(cap),
                                                        _resizable(resizable),
                                                        _mutex() {
            if(cap < 4) {
                _storage = std::make_unique<Node[]>(4);
            } else {
                _storage = std::make_unique<Node[]>(cap);
            }
        }

        /**
         * Inserts `value` into the HashTable given the `key`.
         * If the entry exists already, insert() returns false and does
         * not overwrite the existing entry.
         * Likewise, insert() returns false if the HashTable is set to not
         * be resizable and reached its maximum capacity.
         * To overwrite a value, use the subscript operator or remove the
         * existing one first.
         *
         * @param key the entry's key
         * @param value the value which is to be inserted into the HashTable
         * @return True if successful, false otherwise
         */
        bool insert(K key, V value) {
            // TODO: Add if constexpr and some template argument controlling
            // whether we want to allow additional elements to be added to
            // the HashTable even if its "capacity" limit has already been
            // reached
            //if(_size == _capacity)
            //    return false;
            if(get(key))
                return false;
            
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            if(_resizable) {
                while(needsResize(1)) {
                    glock.unlock();
                    resize(1);
                    glock.lock();
                }
            }


            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];
            // Get this bucket's lock
            std::unique_lock lock(bucket._lock);

            bucket.l.push_back(std::make_pair(key, value));

            ++_size; 

            return true;
        }

        /**
         * Tries to fetch the value associated with the given `key`.
         *
         * @param key the key of the entry which should be retrieved
         * @return An optional which contains a value if the key existed.
         */
        std::optional<V> get(const K& key) const {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];
            // Get this bucket's lock
            std::shared_lock lock(bucket._lock);

            if(bucket.l.size() == 0)
                return std::nullopt;

            auto result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            if(result != bucket.l.end()) {
                return std::make_optional((*result).second);
            } else {
                return std::nullopt;
            }
        }

        /**
         * Tries to remove and return the value associated with the given `key`.
         *
         * @param key the key of the entry which should be removed from the HashTable
         * @return An optional which contains a value if the key existed.
         */
        std::optional<V> remove(const K& key) {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            if(_resizable) {
                while(needsResize(-1)) {
                    glock.unlock();
                    resize(-1);
                    glock.lock();
                }
            }

            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];
            // Get this bucket's lock
            std::unique_lock lock(bucket._lock);

            auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            if(result != bucket.l.end()) {
                --_size;
                auto ret = std::make_optional((*result).second);
                bucket.l.erase(result);

                return ret;
            } else {
                return std::nullopt;
            }
            return std::make_optional((*result).second);
        }

        /**
         * Returns a vector of key/value pairs containing the content of the specified bucket.
         *
         * @param i The bucket's index / hash value
         * @returns A vector of key/value pairs containing the content of the specified bucket
         */
        std::vector<std::pair<K, V>> getBucket(size_t i) const {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            std::vector<std::pair<K, V>> vec{};

            auto& bucket = _storage[i];
            // Get the bucket's lock
            std::shared_lock lock(bucket._lock);

            for(auto& elem : bucket.l) {
                vec.push_back(elem);
            } 

            return vec;
        }

        /**
         * Returns a vector containing all keys present in the HashTable.
         *
         * @returns an std::vector<K> containing the keys in all buckets
         */
        std::vector<K> getKeys() const {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            std::vector<K> vec = std::vector<K>();

            // Get the global lock
            std::shared_lock lock(_mutex);

            for(size_t i = 0; i < _capacity; ++i) {
                //std::cout << "Bucket " << i << " size: " << _storage[i].l.size() << std::endl;
                std::shared_lock llock(_storage[i]._lock);
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
         *
         * @returns an std::vector<V> containing the values in all buckets
         */
        std::vector<V> getValues() const {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            std::vector<V> vec = std::vector<V>();

            // Get the global lock
            std::shared_lock lock(_mutex);

            for(size_t i = 0; i < _capacity; ++i) {
                std::shared_lock llock(_storage[i]._lock);
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
         *
         * @returns the current amount of key/value pairs in the HashTable as size_t
         */
        size_t size() const {
            //std::shared_lock lock(_mutex);
            return _size;
        }

        /**
         * Returns the current capacity of the HashTable.
         *
         * @returns the current amount of buckets in the HashTable as size_t
         */
        size_t capacity() const {
            //std::shared_lock lock(_mutex);
            return _capacity;
        }

        /**
         * Returns whether the HashTable is set to be resizable.
         *
         * @returns whether the HashTable is set to be resizable as bool
         */
        constexpr bool isResizable() const {
            return _resizable;
        }

        /**
         * Checks whether the HashTable needs to be resized.
         *
         * @param delta a difference to the current `_size`
         * @returns 1 if the HashTable needs to expanded, 2 if it needs to be shrinked, 0 otherwise
         */
        int needsResize(int delta) const {
            // Avoid underflow
            if(_size == 0)
                return 0;
            // Avoid ending up in an endless circle of shrinking and growing
            // in some circumstances
            if(_capacity <= 4)
                return 1;

            auto lf = load_factor(delta);

            if(lf >= ALPHA_MAX)
                return 1;
            if(lf > 0.0 && lf <= ALPHA_MIN)
                return 2;

            return 0;
        }

        /**
         * Returns the current load factor of the HashTable.
         * The load factor is calculated as n / k, n being the number of entries occupied
         * in the HashTable, k being the number of buckets.
         *
         * @param delta a number added or subtracted from _size prior to calculation
         * @returns the current Load Factor of the HashTable as double
         */
        double load_factor(int delta = 0) const {
            //std::shared_lock lock(_mutex);
            if(_capacity == 0) return 0.0;

            return static_cast<double>(_size + static_cast<size_t>(delta)) / static_cast<double>(_capacity);
        }

        /**
         * Returns a reference to the value the provided key is mapped to.
         * If an assignment happens, the assignment is proxied to the assignment operator
         * of the Proxy struct.
         */
        Proxy operator[](const K key) {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];
            // Get this bucket's lock
            std::unique_lock lock(bucket._lock);

            auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            return Proxy{*this, key, ((*result).second)};
        }

        V& operator[](const K key) const {
            // Get the table's global lock in read mode
            std::shared_lock glock(_mutex);

            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];
            // Get this bucket's lock
            //std::shared_lock lock(bucket._lock);
            std::unique_lock lock(bucket._lock);

            auto const result = std::find_if(bucket.l.begin(), bucket.l.end(),
                    [&key](const std::pair<K, V>& elem) { return elem.first == key; } );

            return V{Proxy{*this, key, ((*result).second)}};
        }

        /**
         * Prints out the current key/value pairs in all buckets to stdout
         */

        void print_table() const {
            // TODO
            auto keys   = getKeys();
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
            // Each bucket manages a RW-lock
            mutable std::shared_mutex _lock;

            std::list<std::pair<K, V>> l = std::list<std::pair<K, V>>();
        };

        // _size must be atomic since many threads may write to it
        // at the same time
        std::atomic<size_t> _size;
        std::atomic<size_t> _capacity;
        const bool _resizable;
        std::unique_ptr<Node[]> _storage;
        // The table's global mutex / RW-lock
        mutable std::shared_mutex _mutex;

        size_t hash(const K key) const {
            std::hash<K> hash_fn;
            size_t hash_val = hash_fn(key) % _capacity;

            return hash_val;
        }

        // Collects all (key, value) pairs into one list, emptying all buckets
        std::list<std::pair<K, V>> collectPairs() {
            std::list<std::pair<K, V>> l = std::list<std::pair<K, V>>();

            for(size_t i = 0; i < _capacity; ++i) {
                l.splice(l.begin(), _storage[i].l);
            }

            return l;
        }

        /**
         * Resizes the HashTable by growing or shrinking.
         */
        inline void resize(int delta = 0) {
            // Acquire the HashTable's global lock in write mode
            std::unique_lock glock(_mutex);

            // Check whether resizing is still necessary or already happened
            switch(needsResize(delta)) {
                case 1: {
                    // Grow the HashTable
                    //std::cout << "lf " << load_factor(1) << " >= " << ALPHA_MAX << ", growing..." << std::endl;

                    auto old_elems = collectPairs();

                    // Create new storage, rehash all old entrys
                    auto oldPtr = _storage.release();
                    delete[] oldPtr;
                    _storage = std::make_unique<Node[]>(_capacity.load() * GROWTH_FACTOR);
                    _capacity = _capacity * GROWTH_FACTOR;
                    _size = 0;

                    for(auto& elem : old_elems) {
                        //insert(elem.first, std::move(elem.second)); 
                        insert_unsafe(elem.first, std::move(elem.second));
                    }
                    break;
                    }
                case 2: {
                    // Shrink the HashTable
                    //std::cout << "lf " << load_factor(-1) << " <= " << /*(ALPHA_MAX/4)*/ 0.10 << ", shrinking..." << std::endl;
                    
                    auto old_elems = collectPairs();
                    // Create new storage, rehash all old entrys
                    auto oldPtr = _storage.release();
                    delete[] oldPtr;
                    _storage = std::make_unique<Node[]>((_capacity.load() + (SHRINK_FACTOR - 1)) / SHRINK_FACTOR);
                    _capacity = (_capacity + (SHRINK_FACTOR - 1)) / SHRINK_FACTOR;
                    _size = 0;

                    for(auto& elem : old_elems) {
                        insert_unsafe(elem.first, std::move(elem.second));
                    }
                    break;
                    }
                default:
                    // Resizing isn't necessary anymore
                    break;
            }

            glock.unlock();
            return;
        }

        // Used when resizing.
        // Does not acquire the global lock nor the bucket's lock.
        void insert_unsafe(K key, V value) {
            size_t hash_val = hash(key);
            auto& bucket = _storage[hash_val];

            bucket.l.push_back(std::make_pair(key, value));

            ++_size;
        }
};
