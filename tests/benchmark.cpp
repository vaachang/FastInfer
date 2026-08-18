#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch_scheduler.h"
#include "minisrv/runtime/inference_request.h"
#include "minisrv/runtime/onnxruntime_backend.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

int main(int argc, char* argv[]) {
    using namespace minisrv;

    // ----------------------------------------
    // 参数
    // ----------------------------------------

    std::size_t max_batch_size = 8;
    int duration_seconds = 10;

    if (argc >= 2) {
        max_batch_size =
            static_cast<std::size_t>(
                std::stoul(argv[1])
            );
    }

    if (argc >= 3) {
        duration_seconds =
            std::stoi(argv[2]);
    }

    constexpr int num_clients = 8;

    // ----------------------------------------
    // Queue
    // ----------------------------------------

    BoundedBlockingQueue<
        std::shared_ptr<InferenceRequest>
    > queue(128);

    // ----------------------------------------
    // Backend
    // ----------------------------------------

    ONNXRuntimeBackend backend(
        "models/mul2.onnx"
    );

    // ----------------------------------------
    // Scheduler
    // ----------------------------------------

    BatchScheduler scheduler(
        queue,
        backend,
        max_batch_size,
        std::chrono::milliseconds(5)
    );

    scheduler.start();

    // ----------------------------------------
    // Benchmark state
    // ----------------------------------------

    std::atomic<bool> generating{true};

    std::atomic<std::size_t> request_count{0};

    std::atomic<std::size_t> completed_count{0};

    std::vector<std::thread> clients;

    auto benchmark_start = Clock::now();

    // ----------------------------------------
    // Client threads
    // ----------------------------------------

    for (int t = 0; t < num_clients; ++t) {

        clients.emplace_back([&]() {

            while (
                generating.load(
                    std::memory_order_relaxed
                )
            ) {

                auto request =
                    std::make_shared<InferenceRequest>();

                request->id =
                    request_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                request->input = {
                    1.0f,
                    2.0f,
                    3.0f
                };

                request->submit_time =
                    Clock::now();

                auto future =
                    request->promise.get_future();

                // --------------------------------
                // 提交请求
                // --------------------------------

                if (!queue.push(request)) {
                    break;
                }

                // --------------------------------
                // 等待推理完成
                // --------------------------------

                try {
                    future.get();

                    completed_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                } catch (const std::future_error& e) {

                    std::cerr
                        << "future error: "
                        << e.what()
                        << '\n';

                    break;
                }
            }
        });
    }

    // ----------------------------------------
    // 持续运行
    // ----------------------------------------

    std::this_thread::sleep_for(
        std::chrono::seconds(
            duration_seconds
        )
    );

    // ----------------------------------------
    // 停止产生新请求
    // ----------------------------------------

    generating.store(
        false,
        std::memory_order_relaxed
    );

    // ----------------------------------------
    // 等待所有 Client
    //
    // 注意：
    // 此时 Scheduler 仍然运行
    // ----------------------------------------

    for (auto& client : clients) {
        client.join();
    }

    // ----------------------------------------
    // 所有 Client 都已经结束
    //
    // 此时理论上没有未完成的 future
    // ----------------------------------------

    auto benchmark_end = Clock::now();

    // ----------------------------------------
    // 停止 Scheduler
    // ----------------------------------------

    scheduler.stop();

    // ----------------------------------------
    // 统计
    // ----------------------------------------

    const auto total_requests =
        request_count.load(
            std::memory_order_relaxed
        );

    const auto completed_requests =
        completed_count.load(
            std::memory_order_relaxed
        );

    const double elapsed =
        std::chrono::duration<double>(
            benchmark_end - benchmark_start
        ).count();

    const double throughput =
        elapsed > 0.0
            ? static_cast<double>(
                completed_requests
              ) / elapsed
            : 0.0;

    const auto total_batches =
        scheduler.total_batches();

    const auto batched_requests =
        scheduler.total_requests();

    const auto max_actual_batch =
        scheduler.max_batch_size_seen();

    const double average_batch_size =
        total_batches > 0
            ? static_cast<double>(
                batched_requests
              ) / total_batches
            : 0.0;

    // ----------------------------------------
    // 输出
    // ----------------------------------------

    std::cout
        << "========== Benchmark ==========\n";

    std::cout
        << "Clients: "
        << num_clients
        << '\n';

    std::cout
        << "Duration: "
        << duration_seconds
        << " s\n";

    std::cout
        << "Max Batch Size: "
        << max_batch_size
        << '\n';

    std::cout
        << "Total Requests: "
        << total_requests
        << '\n';

    std::cout
        << "Completed Requests: "
        << completed_requests
        << '\n';

    std::cout
        << "Elapsed: "
        << elapsed
        << " s\n";

    std::cout
        << "Throughput: "
        << throughput
        << " req/s\n";

    std::cout
        << "Total Batches: "
        << total_batches
        << '\n';

    std::cout
        << "Average Batch Size: "
        << average_batch_size
        << '\n';

    std::cout
        << "Max Actual Batch Size: "
        << max_actual_batch
        << '\n';

    std::cout
        << "================================\n";

    return 0;
}
