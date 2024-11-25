#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mtx;
    std::condition_variable condvar;

public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(value);
        condvar.notify_one();
    }

    T get() {
        std::unique_lock<std::mutex> lock(mtx);
        condvar.wait(lock, [this] { return !queue.empty(); });
        T value = queue.front();
        queue.pop();
        return value;
    }

    T front() {
        std::unique_lock<std::mutex> lock(mtx);
        condvar.wait(lock, [this] { return !queue.empty(); });
        return queue.front();
    }

    void pop() {
        std::unique_lock<std::mutex> lock(mtx);
        condvar.wait(lock, [this] { return !queue.empty(); });
        queue.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
};

#endif // THREADSAFEQUEUE_H