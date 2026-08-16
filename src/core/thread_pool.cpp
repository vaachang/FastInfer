#include "minisrv/core/thread_pool.h"

namespace minisrv {

ThreadPool::ThreadPool(
    std::size_t num_threads,
    std::size_t queue_capacity
)
    : tasks_(queue_capacity)
{
    if (num_threads == 0) {
        throw std::invalid_argument(
            "ThreadPool requires at least one worker"
        );
    }

    workers_.reserve(num_threads);

    for (std::size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(
            &ThreadPool::worker_loop,
            this
        );
    }
}

ThreadPool::~ThreadPool() {
    tasks_.close();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker_loop() {
    while (true) {
        auto task = tasks_.pop();

        if (!task) {
            return;
        }

        (*task)();
    }
}

} // namespace minisrv
