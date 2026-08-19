#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch_scheduler.h"
#include "minisrv/runtime/inference_request.h"
#include "minisrv/runtime/onnxruntime_backend.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>

using Clock = std::chrono::steady_clock;

struct RoundResult {
    std::size_t requests = 0;
    std::size_t completed = 0;
    double elapsed = 0.0;
    double throughput = 0.0;

    double e2e_p50 = 0.0;
    double e2e_p95 = 0.0;
    double e2e_p99 = 0.0;

    double queue_p50 = 0.0;
    double queue_p95 = 0.0;
    double queue_p99 = 0.0;

    double inference_p50 = 0.0;
    double inference_p95 = 0.0;
    double inference_p99 = 0.0;

    std::size_t batches = 0;
    double average_batch = 0.0;
    std::size_t max_batch = 0;
};

int main(int argc, char* argv[]) {

    using namespace minisrv;

    // --------------------------------------------------
    // Parameters
    // --------------------------------------------------

    std::size_t max_batch_size = 8;
    int warmup_seconds = 5;
    int duration_seconds = 10;
    int rounds = 3;

    if (argc >= 2) {
        max_batch_size =
            static_cast<std::size_t>(
                std::stoul(argv[1])
            );
    }

    if (argc >= 3) {
        warmup_seconds =
            std::stoi(argv[2]);
    }

    if (argc >= 4) {
        duration_seconds =
            std::stoi(argv[3]);
    }

    if (argc >= 5) {
        rounds =
            std::stoi(argv[4]);
    }

    constexpr int num_clients = 8;

    // --------------------------------------------------
    // Backend
    // --------------------------------------------------

    ONNXRuntimeBackend backend(
        "models/mul2.onnx"
    );

    // --------------------------------------------------
    // Queue
    // --------------------------------------------------

    BoundedBlockingQueue<
        std::shared_ptr<InferenceRequest>
    > queue(128);

    // --------------------------------------------------
    // Scheduler
    // --------------------------------------------------

    BatchScheduler scheduler(
        queue,
        backend,
        max_batch_size,
        std::chrono::milliseconds(5)
    );

    scheduler.start();

    // --------------------------------------------------
    // Warmup
    // --------------------------------------------------

    std::cout
        << "========== Warmup ==========\n";

    std::cout
        << "Warmup: "
        << warmup_seconds
        << " s\n";

    std::atomic<bool> warmup_running{true};

    std::vector<std::thread> warmup_clients;

    for (int t = 0; t < num_clients; ++t) {

        warmup_clients.emplace_back([&]() {

            while (
                warmup_running.load(
                    std::memory_order_relaxed
                )
            ) {

                auto request =
                    std::make_shared<InferenceRequest>();

                request->id = 0;

                request->input = {
                    1.0f,
                    2.0f,
                    3.0f
                };

                auto future =
                    request->promise.get_future();

                if (!queue.push(request)) {
                    break;
                }

                future.get();
            }
        });
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(
            warmup_seconds
        )
    );

    warmup_running.store(
        false,
        std::memory_order_relaxed
    );

    for (auto& client : warmup_clients) {
        client.join();
    }

    std::cout
        << "Warmup finished.\n\n";

    // --------------------------------------------------
    // Benchmark rounds
    // --------------------------------------------------

    std::vector<RoundResult> results;

    for (int round = 0; round < rounds; ++round) {
        scheduler.reset_statistics();

        std::cout
            << "========== Round "
            << (round + 1)
            << " ==========\n";

        std::atomic<bool> running{true};

        std::atomic<std::size_t> request_count{0};

        std::atomic<std::size_t> completed_count{0};

        std::mutex latency_mutex;

        std::vector<double> e2e_latencies;
        std::vector<double> queue_latencies;
        std::vector<double> inference_latencies;

        std::vector<std::thread> clients;

        auto start = Clock::now();

        // --------------------------------------------------
        // Start clients
        // --------------------------------------------------

        for (int t = 0; t < num_clients; ++t) {

            clients.emplace_back([&]() {

                while (
                    running.load(
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

                    request->submit_time = Clock::now();

                    auto future =
                        request->promise.get_future();

                    if (!queue.push(request)) {
                        break;
                    }

                    try {

                        future.get();

                        const auto finish_time = Clock::now();

                        const double e2e_ms =
                            std::chrono::duration<double, std::milli>(
                                finish_time - request->submit_time
                            ).count();

                        const double queue_ms =
                            std::chrono::duration<double, std::milli>(
                                request->batch_start_time -
                                request->submit_time
                            ).count();

                        const double inference_ms =
                            std::chrono::duration<double, std::milli>(
                                request->inference_end_time -
                                request->inference_start_time
                            ).count();

                        {
                            std::lock_guard<std::mutex> lock(
                                latency_mutex
                            );

                            e2e_latencies.push_back(e2e_ms);
                            queue_latencies.push_back(queue_ms);
                            inference_latencies.push_back(inference_ms);
                        }

                        completed_count.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );

                    } catch (
                        const std::future_error& e
                    ) {

                        std::cerr
                            << "future error: "
                            << e.what()
                            << '\n';

                        break;
                    }
                }
            });
        }

        // --------------------------------------------------
        // Measurement period
        // --------------------------------------------------

        std::this_thread::sleep_for(
            std::chrono::seconds(
                duration_seconds
            )
        );

        running.store(
            false,
            std::memory_order_relaxed
        );

        // --------------------------------------------------
        // Wait for all requests
        // --------------------------------------------------

        for (auto& client : clients) {
            client.join();
        }

        auto end = Clock::now();

        // --------------------------------------------------
        // Statistics
        // --------------------------------------------------

        const auto requests =
            request_count.load(
                std::memory_order_relaxed
            );

        const auto completed =
            completed_count.load(
                std::memory_order_relaxed
            );

        const double elapsed =
            std::chrono::duration<double>(
                end - start
            ).count();

        const double throughput =
            elapsed > 0.0
                ? static_cast<double>(
                    completed
                  ) / elapsed
                : 0.0;

        auto percentile = [](
            std::vector<double>& values,
            double p
        ) -> double {

            if (values.empty()) {
                return 0.0;
            }

            std::sort(
                values.begin(),
                values.end()
            );

            const std::size_t index =
                static_cast<std::size_t>(
                    p * (values.size() - 1)
                );

            return values[index];
        };

        const double e2e_p50 =
            percentile(e2e_latencies, 0.50);

        const double e2e_p95 =
            percentile(e2e_latencies, 0.95);

        const double e2e_p99 =
            percentile(e2e_latencies, 0.99);

        const double queue_p50 =
            percentile(queue_latencies, 0.50);

        const double queue_p95 =
            percentile(queue_latencies, 0.95);

        const double queue_p99 =
            percentile(queue_latencies, 0.99);

        const double inference_p50 =
            percentile(inference_latencies, 0.50);

        const double inference_p95 =
            percentile(inference_latencies, 0.95);

        const double inference_p99 =
            percentile(inference_latencies, 0.99);

        const auto batches =
            scheduler.total_batches();

        const auto batch_requests =
            scheduler.total_requests();

        const auto max_batch =
            scheduler.max_batch_size_seen();

        const double average_batch =
            batches > 0
                ? static_cast<double>(
                    batch_requests
                  ) / batches
                : 0.0;

        RoundResult result;

        result.requests = requests;
        result.completed = completed;
        result.elapsed = elapsed;
        result.throughput = throughput;

        result.e2e_p50 = e2e_p50;
        result.e2e_p95 = e2e_p95;
        result.e2e_p99 = e2e_p99;

        result.queue_p50 = queue_p50;
        result.queue_p95 = queue_p95;
        result.queue_p99 = queue_p99;

        result.inference_p50 = inference_p50;
        result.inference_p95 = inference_p95;
        result.inference_p99 = inference_p99;
        result.batches = batches;
        result.average_batch = average_batch;
        result.max_batch = max_batch;

        results.push_back(result);

        std::cout
            << "Requests: "
            << requests
            << '\n';

        std::cout
            << "Completed: "
            << completed
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
            << "========== Latency ==========\n";

        std::cout
            << "E2E P50: "
            << e2e_p50
            << " ms\n";

        std::cout
            << "E2E P95: "
            << e2e_p95
            << " ms\n";

        std::cout
            << "E2E P99: "
            << e2e_p99
            << " ms\n";

        std::cout
            << "Queue Wait P50: "
            << queue_p50
            << " ms\n";

        std::cout
            << "Queue Wait P95: "
            << queue_p95
            << " ms\n";

        std::cout
            << "Queue Wait P99: "
            << queue_p99
            << " ms\n";

        std::cout
            << "Inference P50: "
            << inference_p50
            << " ms\n";

        std::cout
            << "Inference P95: "
            << inference_p95
            << " ms\n";

        std::cout
            << "Inference P99: "
            << inference_p99
            << " ms\n";

        std::cout
            << "Total Batches: "
            << batches
            << '\n';

        std::cout
            << "Average Batch Size: "
            << average_batch
            << '\n';

        std::cout
            << "Max Actual Batch Size: "
            << max_batch
            << '\n';

        std::cout
            << '\n';
    }

    // --------------------------------------------------
    // Final statistics
    // --------------------------------------------------

    double throughput_sum = 0.0;

    for (const auto& result : results) {
        throughput_sum += result.throughput;
    }

    const double average_throughput =
        results.empty()
            ? 0.0
            : throughput_sum / results.size();

    std::cout
        << "========================================\n";

    std::cout
        << "Benchmark Summary\n";

    std::cout
        << "Batch Size: "
        << max_batch_size
        << '\n';

    std::cout
        << "Clients: "
        << num_clients
        << '\n';

    std::cout
        << "Warmup: "
        << warmup_seconds
        << " s\n";

    std::cout
        << "Duration: "
        << duration_seconds
        << " s\n";

    std::cout
        << "Rounds: "
        << rounds
        << '\n';

    std::cout
        << "Average Throughput: "
        << average_throughput
        << " req/s\n";

    std::cout
        << "========================================\n";

    scheduler.stop();

    return 0;
}
