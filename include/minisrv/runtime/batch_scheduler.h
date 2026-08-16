#pragma once

#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch.h"
#include "minisrv/runtime/inference_backend.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <atomic>

namespace minisrv {

class BatchScheduler {
public:
    using RequestPtr = std::shared_ptr<InferenceRequest>;
    using RequestQueue = BoundedBlockingQueue<RequestPtr>;

    BatchScheduler(
        RequestQueue& queue,
        InferenceBackend& backend,
        std::size_t max_batch_size,
        std::chrono::milliseconds max_wait_time
    );

    ~BatchScheduler();

    BatchScheduler(const BatchScheduler&) = delete;
    BatchScheduler& operator=(const BatchScheduler&) = delete;

    void start();

    void stop();

private:
    void run();

    Batch build_batch();

private:
    RequestQueue& queue_;

    InferenceBackend& backend_;

    const std::size_t max_batch_size_;
    const std::chrono::milliseconds max_wait_time_;

    std::thread scheduler_thread_;

    std::atomic<bool> running_{false};
};

} // namespace minisrv
