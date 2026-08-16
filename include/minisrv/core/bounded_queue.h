#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>
#include <stdexcept>
#include <chrono>

namespace minisrv {

template <typename T>
class BoundedBlockingQueue {
public:
    explicit BoundedBlockingQueue(std::size_t capacity)
        : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument(
                "queue capacity must be greater than zero"
            );
        }
    }

    bool push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);

        not_full_.wait(lock, [this] {
            return closed_ || queue_.size() < capacity_;
        });

        if (closed_) {
            return false;
        }

        queue_.push(std::move(value));

        lock.unlock();
        not_empty_.notify_one();

        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        not_empty_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();

        lock.unlock();
        not_full_.notify_one();

        return value;
    }

    std::optional<T> wait_for(
        std::chrono::milliseconds timeout
    ) {
        std::unique_lock<std::mutex> lock(mutex_);

        bool ready = not_empty_.wait_for(
            lock,
            timeout,
            [this] {
                return closed_ || !queue_.empty();
            }
        );

        if (!ready || queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();

        lock.unlock();
        not_full_.notify_one();

        return value;
    }

    bool try_push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_ || queue_.size() >= capacity_) {
            return false;
        }

        queue_.push(std::move(value));

        not_empty_.notify_one();

        return true;
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();

        not_full_.notify_one();

        return value;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }

        not_empty_.notify_all();
        not_full_.notify_all();
    }

    bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;

    const std::size_t capacity_;

    mutable std::mutex mutex_;

    std::condition_variable not_empty_;
    std::condition_variable not_full_;

    bool closed_{false};
};

} // namespace minisrv
