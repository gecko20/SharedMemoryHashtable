#pragma once
#include <array>
#include <cstddef>

#include <mutex> // TODO 
#include <optional>
#include <iostream> // Debugging

/**
 * A simple circular buffer / fixed size queue
 * The number of available slots is defined by the template parameter N.
 */

template <typename T, size_t N = 10>
class CircularBuffer {
    public: 
        //CircularBuffer<T>(size_t cap) : _buffer(std::make_unique<T[]>(cap)), _capacity(cap) {
        CircularBuffer<T>(size_t cap) : _buffer(std::array<T, N> {}), _capacity(cap) {

        }

        CircularBuffer<T>& operator=(const CircularBuffer<T>& other) {
            //_buffer = std::make_unique<T[]>(other.capacity());
            //_buffer = other._buffer;
            std::copy(other._buffer.begin(), other._buffer.end(), _buffer.begin());
            _capacity = other.capacity();
            return *this;
        }
        CircularBuffer<T>& operator=(CircularBuffer<T>&& other) {
            _buffer = std::move(other._buffer);
            _capacity = std::move(other._capacity);
            return *this;
        }
        
        /**
         * Returns a reference to the slot at the provided index.
         * Note that changes to the data at the slot could lead to
         * undefined behaviour.
         */
        T& operator[](const size_t idx) {
            //return &(_buffer[idx]);
            return _buffer.at(idx);
        }
        T* getPtr(const size_t idx) {
            return &(_buffer.data()[idx]);
        }

        /**
         * @returns a pointer to the element at the current head without modifying it as well as its index.
         */
        std::optional<std::pair<const T*, size_t>> peek() const {
            if(isEmpty())
                return std::nullopt;

            //return std::make_optional<std::pair<T, int>>(std::make_pair(_buffer[_tailIdx], _tailIdx));
            //return std::make_optional<std::pair<T&, size_t>>(std::make_pair(&_buffer[_headIdx], _headIdx));
            //return std::make_optional(std::pair<T*, size_t>{_buffer.at(_headIdx), _headIdx});
            return std::make_optional(std::pair<const T*, size_t>{&(_buffer.data()[_headIdx]), _headIdx});
        }

        /**
         * Returns and pops the element at the current head as well as its index.
         *
         * @returns the element at the current head without modifying it as well as its index.
         */
        std::optional<std::pair<T, size_t>> pop() {
            if(isEmpty())
                return std::nullopt;
            // TODO Lock

            auto elem = std::make_optional<std::pair<T, size_t>>(std::make_pair(_buffer[_headIdx], _headIdx));
            //_buffer[_headIdx] = T(); // Not actually needed, remove for better performance
            --_size;
            //++_headIdx; // TODO avoid modulo etc
            //_headIdx %= _capacity;
            _headIdx = (_headIdx + 1) % _capacity;

            return elem;
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
            
            // TODO Lock
            int ret = static_cast<int>(_tailIdx);
            _buffer[_tailIdx] = elem;
            ++_size;
            //++_tailIdx; // TODO avoid modulo etc.
            //_tailIdx %= _capacity;
            _tailIdx = (_tailIdx + 1) % _capacity;

            return ret;
        }

        /**
         * Returns a reference to the specified slot in the underlying buffer.
         * The slot can be modified but is ignored in other functionality of
         * the CircularBuffer.
         */
        T& at(size_t idx) {
            return _buffer.at(idx);
        }
        //T* at(size_t idx) {
        //    return &(_buffer[idx]);
        //}


        inline bool isEmpty() const {
            return _size == 0;
        }

        inline bool isFull() const {
            return _size == _capacity;
        }

        size_t capacity() const {
            return _capacity;
        }

        size_t size() const {
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
        std::mutex _mutex; // TODO
        //std::unique_ptr<T[]> _buffer;
        //T[N] _buffer;
        std::array<T, N> _buffer;

        size_t _headIdx{0};
        size_t _tailIdx{0};

        size_t _capacity;
        size_t _size{0};
};
