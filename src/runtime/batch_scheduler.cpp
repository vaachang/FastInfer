#include "minisrv/runtime/batch_scheduler.h"

#include <stdexcept>

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
    if (!running_) {
        return;
    }

    running_ = false;

    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void BatchScheduler::run() {
    // TODO
}

Batch BatchScheduler::build_batch() {
    // TODO
    return Batch{};
}

} // namespace minisrv
