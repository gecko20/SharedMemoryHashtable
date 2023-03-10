#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>

#include <cstdlib>

#include <optional>
#include <semaphore>
#include <shared_mutex>
#include <iostream> // Debugging
#include <stdexcept>

#include "mutex.h"

using namespace std::chrono_literals;

class timeout_exception : public std::exception {
    public:
        const char* what() const noexcept override {
            return "Timeout occurred";
        }
};

/**
 * A simple circular buffer / fixed size queue
 * The number of available slots is defined by the template parameter N.
 *
 * TODO: Timeouts so that locked threads can return if a client dies?
 */

template <typename T, size_t N = 10>
class CircularBuffer {
    public: 
        CircularBuffer() : _buffer(std::array<T, N> {}), _capacity(N), _pmutex(), _openSlots(N), _fullSlots(0) {

        }

        /**
         * Returns a reference to the slot at the provided index.
         * //Reading and writing via the subscript operator is not threadsafe per se.
         */
        T& operator[](const size_t idx) {
            _pmutex.lock();
            T& ret = _buffer[idx];
            _pmutex.unlock();
            return ret;
        }

        /**
         * Updates the given slot by overwriting it with the provided element.
         */
        void update(size_t idx, T elem) {
            if(idx >= _capacity)
                throw std::out_of_range("Update failed");
            _pmutex.lock();
            _buffer[idx] = elem;
            _pmutex.unlock();
        }

        /**
         * @returns an std::optional possibly containing an std::pair<T, size_t> consisting of the content of the current slot at _headIdx as well as the _headIdx itself
         */
        std::optional<std::pair<T, size_t>> peek() {
            if(isEmpty())
                return std::nullopt;

            //_fullSlots.acquire();
            T elem;
            size_t idx;
            {
                //std::lock_guard<std::mutex> lock(_mutex);
                _pmutex.lock();
                elem = _buffer[_headIdx];
                idx  = _headIdx;
                //_mutex.unlock();
                _pmutex.unlock();
            }
            //_openSlots.release();

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

            //_fullSlots.acquire();
            //_nWait_full++;
            _fullSlots.wait();
            //if(!_fullSlots.try_acquire_for(2s))
            //    throw timeout_exception();

            T elem;
            size_t idx;
            {
                //std::lock_guard<std::mutex> lock(_mutex);
                _pmutex.lock();
                //_buffer[_headIdx] = T(); // overwrite slot, not actually needed, remove for better performance
                elem = _buffer[_headIdx];
                idx  = _headIdx;
                --_size;
                if((long long)_size < 0) {
                    std::cout << "pop(): _size < 0: " << _size << std::endl;
                    std::cout << "elem = " << elem << std::endl;
                    std::cout << "idx = " << idx << std::endl;
                    std::exit(-1);
                }

                _headIdx = (_headIdx + 1) % _capacity;
                //_mutex.unlock();
                //unsigned int fullVal = _fullSlots.current_value();
                //unsigned int openVal = _openSlots.current_value();
                //std::cout << "fullVal: " << fullVal << "; openVal: " << openVal << std::endl;

                _pmutex.unlock();
            }
            //_openSlots.release();
            //_nPost_open++;
            _openSlots.post();

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

            //_openSlots.acquire();
            //_nWait_open++;
            _openSlots.wait();
            //_openSlots.try_acquire_for(2s);
            //if(!_fullSlots.try_acquire_for(2s))
            //    throw timeout_exception();

            int ret; //= static_cast<int>(_tailIdx);
            {
                //std::lock_guard<std::mutex> lock(_mutex);
                _pmutex.lock();
                ret = static_cast<int>(_tailIdx);
                _buffer[_tailIdx] = elem;
                ++_size;
                if((long long)_size < 0) {
                    std::cout << "push(): _size < 0!" << std::endl;
                    std::exit(-1);
                }
                //++_tailIdx; // TODO avoid modulo etc.
                //_tailIdx %= _capacity;
                _tailIdx = (_tailIdx + 1) % _capacity;
                //_mutex.unlock();
                //unsigned int fullVal = _fullSlots.current_value();
                //unsigned int openVal = _openSlots.current_value();
                //std::cout << "fullVal: " << fullVal << "; openVal: " << openVal << std::endl;
                _pmutex.unlock();
            }
            //_fullSlots.release();
            //_nPost_full++;
            _fullSlots.post();

            return ret;
        }

        /**
         * Returns a reference to the specified slot in the underlying buffer.
         * The slot can be modified but is ignored in other functionality of
         * the CircularBuffer.
         */
        T& at(const size_t idx) {
            //std::lock_guard<std::mutex> lock(_mutex);
            _pmutex.lock();
            //std::unique_lock lock(_rwlock);
            auto& ret = _buffer.at(idx);
            //_mutex.unlock();
            _pmutex.unlock();

            return ret;
            //return _buffer.at(idx);
        }

        inline bool isEmpty() const {
            //std::lock_guard<std::mutex> lock(_mutex);
            _pmutex.lock();
            bool ret = _size == 0;
            //std::shared_lock lock(_rwlock);
            //return _size == 0;
            //_mutex.unlock();
            _pmutex.unlock();

            return ret;
        }

        inline bool isFull() const {
            //std::lock_guard<std::mutex> lock(_mutex);
            _pmutex.lock();
            bool ret = _size == _capacity;
            //std::shared_lock lock(_rwlock);
            //_mutex.unlock();
            _pmutex.unlock();

            return ret;
            //return _size == _capacity;
        }

        size_t capacity() const {
            _pmutex.lock();
            size_t ret = _capacity;
            _pmutex.unlock();
            return ret;
            //return _capacity;
        }

        size_t size() const {
            //std::lock_guard<std::mutex> lock(_mutex);
            _pmutex.lock();
            //std::shared_lock lock(_rwlock);
            size_t ret = _size;
            //_mutex.unlock();
            _pmutex.unlock();

            return ret;
            //return _size;
        }

        void lock() const {
            _pmutex.lock();
        }
        void unlock() const {
            _pmutex.unlock();
        }

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
        //std::atomic<size_t> _nWait_open{0};
        //std::atomic<size_t> _nPost_open{0};
        //std::atomic<size_t> _nWait_full{0};
        //std::atomic<size_t> _nPost_full{0};
        std::array<T, N> _buffer;

        size_t _headIdx{0};
        size_t _tailIdx{0};

        size_t _capacity;
        size_t _size{0};

        //mutable std::mutex _mutex;
        mutable PMutex _pmutex;
        //std::counting_semaphore<N> _openSlots;
        //std::counting_semaphore<N> _fullSlots;
        CountingSemaphore _openSlots;
        CountingSemaphore _fullSlots;
};
