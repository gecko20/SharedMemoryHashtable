#pragma once
#include <array>
#include <cstddef>

#include <mutex>
#include <optional>
#include <semaphore>
#include <shared_mutex>
#include <iostream> // Debugging

/**
 * A simple circular buffer / fixed size queue
 * The number of available slots is defined by the template parameter N.
 *
 * TODO: Timeouts with try_acquire_for and exception handling in server/client
 */

template <typename T, size_t N = 10>
class CircularBuffer {
    public: 
        //CircularBuffer<T>(size_t cap) : _buffer(std::make_unique<T[]>(cap)), _capacity(cap) {
        CircularBuffer<T>(size_t cap) : _buffer(std::array<T, N> {}), _capacity(cap), _openSlots(N), _fullSlots(0) {

        }

        //CircularBuffer<T>& operator=(const CircularBuffer<T>& other) {
        //    std::copy(other._buffer.begin(), other._buffer.end(), _buffer.begin());
        //    //_capacity = other.capacity();
        //    _capacity = other._capacity;
        //    _headIdx  = other._headIdx;
        //    _tailIdx  = other._tailIdx;
        //    _size     = other._size;

        //    _openSlots = std::counting_semaphore<N>(other._capacity);
        //    _fullSlots = std::counting_semaphore<N>(0);

        //    return *this;
        //}
        //CircularBuffer<T>& operator=(CircularBuffer<T>&& other) {
        //    _buffer   = std::move(other._buffer);
        //    _capacity = std::move(other._capacity);
        //    _headIdx  = std::move(other._headIdx);
        //    _tailIdx  = std::move(other._tailIdx);
        //    _size     = std::move(other._size);

        //    _openSlots = std::counting_semaphore<N>(other._capacity);
        //    _fullSlots = std::counting_semaphore<N>(0);

        //    return *this;
        //}

        
        /**
         * Returns a reference to the slot at the provided index.
         * //Reading and writing via the subscript operator is not threadsafe per se.
         */
        T& operator[](const size_t idx) {
            std::lock_guard<std::mutex> lock(_mutex);
            //return &(_buffer[idx]);
            return _buffer.at(idx);
        }
        //T* getPtr(const size_t idx) {
        //    return &(_buffer.data()[idx]);
        //}

        /**
         * @returns a pointer to the element at the current head without modifying it as well as its index.
         */
        //std::optional<std::pair<const T*, size_t>> peek() const {
        std::optional<std::pair<T, size_t>> peek() {
            if(isEmpty())
                return std::nullopt;

            //_fullSlots.acquire();
            T elem;
            size_t idx;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                elem = _buffer[_headIdx];
                idx  = _headIdx;
            }
            //_openSlots.release();

            //return std::make_optional<std::pair<T, int>>(std::make_pair(_buffer[_tailIdx], _tailIdx));
            //return std::make_optional<std::pair<T&, size_t>>(std::make_pair(&_buffer[_headIdx], _headIdx));
            //return std::make_optional(std::pair<T*, size_t>{_buffer.at(_headIdx), _headIdx});
            //return std::make_optional(std::pair<const T*, size_t>{&(_buffer.data()[_headIdx]), _headIdx});
            return std::make_optional(std::pair<T, size_t>{elem, idx});
        }

        /**
         * Returns and pops the element at the current head as well as its index.
         *
         * @returns the element at the current head without modifying it as well as its index.
         */
        std::optional<std::pair<T, size_t>> pop() {
            if(isEmpty())
                return std::nullopt;

            _fullSlots.acquire();
            T elem;
            size_t idx;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                //auto elem = std::make_optional<std::pair<T, size_t>>(std::make_pair(_buffer[_headIdx], _headIdx));
                //_buffer[_headIdx] = T(); // Not actually needed, remove for better performance
                elem = _buffer[_headIdx];
                idx  = _headIdx;
                --_size;
                _headIdx = (_headIdx + 1) % _capacity;
            }
            _openSlots.release();

            return std::make_optional<std::pair<T, size_t>>(std::make_pair(elem, idx));
        }

        /**
         * Enqueues a new element.
         *
         * @param elem the new element
         * @returns a positive integer (the now filled slot's index) on success, -1 on failure
         */
        int push_back(T elem) {
            if(isFull())
                return -1;

            _openSlots.acquire();
            int ret = static_cast<int>(_tailIdx);
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _buffer[_tailIdx] = elem;
                ++_size;
                //++_tailIdx; // TODO avoid modulo etc.
                //_tailIdx %= _capacity;
                _tailIdx = (_tailIdx + 1) % _capacity;
            }
            _fullSlots.release();

            return ret;
        }

        /**
         * Returns a reference to the specified slot in the underlying buffer.
         * The slot can be modified but is ignored in other functionality of
         * the CircularBuffer.
         */
        T& at(size_t idx) {
            std::lock_guard<std::mutex> lock(_mutex);
            //std::unique_lock lock(_rwlock);
            return _buffer.at(idx);
        }
        //T* at(size_t idx) {
        //    return &(_buffer[idx]);
        //}


        inline bool isEmpty() const {
            std::lock_guard<std::mutex> lock(_mutex);
            //std::shared_lock lock(_rwlock);
            return _size == 0;
        }

        inline bool isFull() const {
            std::lock_guard<std::mutex> lock(_mutex);
            //std::shared_lock lock(_rwlock);
            return _size == _capacity;
        }

        size_t capacity() const {
            return _capacity;
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(_mutex);
            //std::shared_lock lock(_rwlock);
            return _size;
        }

        //size_t size_in_bytes() const {
        //    return _size * sizeof(T) + sizeof(CircularBuffer<T>);
        //}

        // Debugging
        void printBuffer() const {
            std::cout << "Buffer capacity: " << capacity() << " (" << size() << " used)" \
                << "; Head: " << _headIdx << ", Tail: " << _tailIdx << std::endl;
            for(size_t i = 0; i < _capacity; ++i) {
                std::cout << _buffer[i] << " ";
            }
            std::cout << std::endl;
        }

    private:
        //std::unique_ptr<T[]> _buffer;
        //T[N] _buffer;
        std::array<T, N> _buffer;

        size_t _headIdx{0};
        size_t _tailIdx{0};

        size_t _capacity;
        size_t _size{0};

        mutable std::mutex _mutex;
        std::counting_semaphore<N> _openSlots;
        std::counting_semaphore<N> _fullSlots;
        mutable std::shared_mutex _rwlock;
};
