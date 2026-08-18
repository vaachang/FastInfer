#include "minisrv/runtime/batch_scheduler.h"

#include <stdexcept>
#include <iostream>

namespace minisrv {

    BatchScheduler::BatchScheduler(
        RequestQueue& queue,
        InferenceBackend& backend,
        std::size_t max_batch_size,
        std::chrono::milliseconds max_wait_time
    )
        : queue_(queue),
          backend_(backend),
          max_batch_size_(max_batch_size),
          max_wait_time_(max_wait_time)
{
    if (max_batch_size_ == 0) {
        throw std::invalid_argument(
            "max_batch_size must be greater than zero"
        );
    }

    if (max_wait_time_.count() <= 0) {
        throw std::invalid_argument(
            "max_wait_time must be greater than zero"
        );
    }
}

BatchScheduler::~BatchScheduler() {
    stop();
}

void BatchScheduler::start() {
    if (running_) {
        return;
    }

    running_ = true;

    scheduler_thread_ =
        std::thread(&BatchScheduler::run, this);
}

void BatchScheduler::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    queue_.close();

    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void BatchScheduler::run() {
    while (running_) {
        Batch batch = build_batch();

        if (batch.size() == 0) {
            break;
        }
        const auto current_batch_size =
            batch.size();

        total_batches_.fetch_add(
            1,
            std::memory_order_relaxed
        );

        total_requests_.fetch_add(
            current_batch_size,
            std::memory_order_relaxed
        );

        auto old_max =
            max_batch_size_seen_.load(
                std::memory_order_relaxed
            );

        while (
            old_max < current_batch_size &&
            !max_batch_size_seen_.compare_exchange_weak(
                old_max,
                current_batch_size,
                std::memory_order_relaxed
            )
        ) {
        }
        backend_.infer(batch);
    }
}

Batch BatchScheduler::build_batch() {
    Batch batch;

    // 等待第一个请求
    auto first_request = queue_.pop();

    if (!first_request) {
        return batch;
    }

    batch.add(std::move(*first_request));

    const auto deadline =
        std::chrono::steady_clock::now()
        + max_wait_time_;

    while (batch.size() < max_batch_size_) {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= deadline) {
            break;
        }

        const auto remaining =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(deadline - now);

        auto request = queue_.wait_for(remaining);

        if (!request) {
            break;
        }

        batch.add(std::move(*request));
    }

    return batch;
}

std::size_t BatchScheduler::total_batches() const {
    return total_batches_.load(
        std::memory_order_relaxed
    );
}

std::size_t BatchScheduler::total_requests() const {
    return total_requests_.load(
        std::memory_order_relaxed
    );
}

std::size_t BatchScheduler::max_batch_size_seen() const {
    return max_batch_size_seen_.load(
        std::memory_order_relaxed
    );
}

} // namespace minisrv
