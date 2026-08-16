#pragma once

#include "minisrv/core/bounded_queue.h"

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <stdexcept>

namespace minisrv {

class ThreadPool {
public:
    ThreadPool(
        std::size_t num_threads,
        std::size_t queue_capacity
    );

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F>
    auto submit(F&& task)
        -> std::future<std::invoke_result_t<F>>;

private:
    void worker_loop();

private:
    std::vector<std::thread> workers_;

    BoundedBlockingQueue<std::function<void()>> tasks_;
};

template <typename F>
auto ThreadPool::submit(F&& task)
    -> std::future<std::invoke_result_t<F>>
{
    using ReturnType = std::invoke_result_t<F>;

    auto packaged_task =
        std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<F>(task)
        );

    std::future<ReturnType> result =
        packaged_task->get_future();

    std::function<void()> wrapper =
        [packaged_task]() {
            (*packaged_task)();
        };

    if (!tasks_.try_push(std::move(wrapper))) {
        throw std::runtime_error(
            "ThreadPool task queue is full or closed"
        );
    }

    return result;
}

} // namespace minisrv
