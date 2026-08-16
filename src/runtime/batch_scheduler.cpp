#include "minisrv/runtime/batch_scheduler.h"

#include <stdexcept>
#include <iostream>

namespace minisrv {

BatchScheduler::BatchScheduler(
    RequestQueue& queue,
    std::size_t max_batch_size,
    std::chrono::milliseconds max_wait_time
)
    : queue_(queue),
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

        std::cout
            << "Built batch with "
            << batch.size()
            << " requests\n";
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

    // 第一个请求到达后开始计时
    const auto deadline =
        std::chrono::steady_clock::now()
        + max_wait_time_;

    while (batch.size() < max_batch_size_) {
        auto now = std::chrono::steady_clock::now();

        if (now >= deadline) {
            break;
        }

        // 在剩余时间内等待下一个请求
        auto request = queue_.try_pop();

        if (request) {
            batch.add(std::move(*request));
            continue;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1)
        );
    }

    return batch;
}

} // namespace minisrv
