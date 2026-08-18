#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch_scheduler.h"
#include "minisrv/runtime/inference_request.h"
#include "minisrv/runtime/onnxruntime_backend.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <string>

using Clock = std::chrono::steady_clock;

struct LatencyRecord {
    double milliseconds;
};

int main(int argc, char* argv[])  {
    using namespace minisrv;

    int num_requests = 1000;
    constexpr int num_clients = 8;
    std::size_t max_batch_size = 8;

    if (argc >= 2) {
        max_batch_size =
            static_cast<std::size_t>(
                std::stoul(argv[1])
            );
    }

    if (argc >= 3) {
        num_requests =
            std::stoi(argv[2]);
    }

    BoundedBlockingQueue<
        std::shared_ptr<InferenceRequest>
    > queue(128);

    ONNXRuntimeBackend backend(
        "models/mul2.onnx"
    );

    BatchScheduler scheduler(
        queue,
        backend,
        max_batch_size,
        std::chrono::milliseconds(5)
    );

    scheduler.start();

    std::vector<std::future<InferenceResult>> futures;
    std::vector<Clock::time_point> submit_times;

    futures.resize(num_requests);
    submit_times.resize(num_requests);

    auto start = Clock::now();

    // --------------------------------------------------
    // 多个 Client 并发提交请求
    // --------------------------------------------------

    std::vector<std::thread> clients;

    for (int t = 0; t < num_clients; ++t) {
        clients.emplace_back([&, t]() {

            const int begin =
                t * num_requests / num_clients;

            const int end =
                (t + 1) * num_requests / num_clients;

            for (int i = begin; i < end; ++i) {
                auto request =
                    std::make_shared<InferenceRequest>();

                request->id = i;

                request->input = {
                    1.0f,
                    2.0f,
                    3.0f
                };

                futures[i] =
                    request->promise.get_future();

                submit_times[i] = Clock::now();

                if (!queue.push(std::move(request))) {
                    std::cerr
                        << "Queue rejected request "
                        << i
                        << '\n';
                }
            }
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    // --------------------------------------------------
    // 等待所有请求完成，并统计延迟
    // --------------------------------------------------

    std::vector<LatencyRecord> latencies;
    latencies.reserve(num_requests);

    for (int i = 0; i < num_requests; ++i) {
        futures[i].get();

        auto finish = Clock::now();

        double latency =
            std::chrono::duration<double, std::milli>(
                finish - submit_times[i]
            ).count();

        latencies.push_back({
            latency
        });
    }

    auto end = Clock::now();

    scheduler.stop();

    const auto total_batches =
        scheduler.total_batches();

    const auto total_requests =
        scheduler.total_requests();

    const auto max_actual_batch =
        scheduler.max_batch_size_seen();

    double average_batch_size =
        total_batches == 0
            ? 0.0
            : static_cast<double>(total_requests)
              / total_batches;

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

    // --------------------------------------------------
    // 排序
    // --------------------------------------------------

    std::sort(
        latencies.begin(),
        latencies.end(),
        [](const LatencyRecord& a,
           const LatencyRecord& b) {
            return a.milliseconds <
                   b.milliseconds;
        }
    );

    auto percentile =
        [&](double p) {

            std::size_t index =
                static_cast<std::size_t>(
                    p * latencies.size()
                );

            if (index >= latencies.size()) {
                index = latencies.size() - 1;
            }

            return latencies[index].milliseconds;
        };

    // --------------------------------------------------
    // 统计吞吐
    // --------------------------------------------------

    double elapsed =
        std::chrono::duration<double>(
            end - start
        ).count();

    double throughput =
        num_requests / elapsed;

    // --------------------------------------------------
    // 输出
    // --------------------------------------------------

    std::cout
        << "========== Benchmark ==========\n";

    std::cout
        << "Requests: "
        << num_requests
        << '\n';

    std::cout
        << "Clients: "
        << num_clients
        << '\n';

    std::cout
        << "Max Batch Size: "
        << max_batch_size
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
        << "P50: "
        << percentile(0.50)
        << " ms\n";

    std::cout
        << "P95: "
        << percentile(0.95)
        << " ms\n";

    std::cout
        << "P99: "
        << percentile(0.99)
        << " ms\n";

    std::cout
        << "================================\n";

    return 0;
}
