#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <deque>

#include "aare/ClusterFinder.hpp"
#include "aare/NDArray.hpp"
#include "aare/logger.hpp"

template <typename T>
class BlockingQueue {
    std::mutex mtx;
    std::condition_variable cv_push, cv_pop;
    std::deque<T> queue;
    size_t max_size;
    bool closed = false;

public:
    BlockingQueue(size_t capacity) : max_size(capacity) {}

    void push(T item) {
        std::unique_lock lock(mtx);
        cv_push.wait(lock, [this] { return queue.size() < max_size; });
        queue.push_back(std::move(item));
        cv_pop.notify_one();
    }

    T pop() {
        std::unique_lock lock(mtx);
        cv_pop.wait(lock, [this] { return !queue.empty(); });
        T item = std::move(queue.front());
        queue.pop_front();
        cv_push.notify_one();
        return item;
    }

    void close() {
        std::lock_guard lock(mtx);
        closed = true;
        cv_pop.notify_all();
        cv_push.notify_all();
    }

    bool empty() {
        std::lock_guard lock(mtx);
        return queue.empty();
    }


    void write(T item) {push(item);}
    bool isEmpty() {return empty();}
    T frontPtr() {return pop();}
    T popFront() {return pop();}
};