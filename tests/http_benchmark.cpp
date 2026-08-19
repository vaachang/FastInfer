#include <httplib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    std::size_t requests = 0;
    std::size_t completed = 0;
    std::size_t errors = 0;

    double elapsed = 0.0;
    double throughput = 0.0;

    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

int main(int argc, char* argv[]) {

    constexpr int default_clients = 8;
    constexpr int default_duration = 5;

    int num_clients = default_clients;
    int duration_seconds = default_duration;

    if (argc >= 2) {
        num_clients = std::stoi(argv[1]);
    }

    if (argc >= 3) {
        duration_seconds = std::stoi(argv[2]);
    }

    if (num_clients <= 0 || duration_seconds <= 0) {
        std::cerr
            << "Usage: "
            << argv[0]
            << " [clients] [duration_seconds]\n";

        return 1;
    }

    std::atomic<bool> running{true};

    std::atomic<std::size_t> request_count{0};
    std::atomic<std::size_t> completed_count{0};
    std::atomic<std::size_t> error_count{0};

    std::mutex latency_mutex;
    std::vector<double> latencies;

    std::cout
        << "========== HTTP Benchmark ==========\n";

    std::cout
        << "Clients: "
        << num_clients
        << '\n';

    std::cout
        << "Duration: "
        << duration_seconds
        << " s\n";

    std::cout << '\n';

    std::vector<std::thread> clients;

    const auto start = Clock::now();

    for (int i = 0; i < num_clients; ++i) {

        clients.emplace_back([&]() {

            httplib::Client client(
                "http://127.0.0.1:8080"
            );

            client.set_connection_timeout(
                2,
                0
            );

            client.set_read_timeout(
                5,
                0
            );

            client.set_write_timeout(
                2,
                0
            );

            while (
                running.load(
                    std::memory_order_relaxed
                )
            ) {

                const auto request_start =
                    Clock::now();

                auto response =
                    client.Post(
                        "/infer",
                        R"({"input":[1.0,2.0,3.0]})",
                        "application/json"
                    );

                const auto request_end =
                    Clock::now();

                const double latency_ms =
                    std::chrono::duration<double, std::milli>(
                        request_end - request_start
                    ).count();

                request_count.fetch_add(
                    1,
                    std::memory_order_relaxed
                );

                if (
                    response &&
                    response->status == 200
                ) {

                    completed_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                    {
                        std::lock_guard<std::mutex> lock(
                            latency_mutex
                        );

                        latencies.push_back(
                            latency_ms
                        );
                    }

                } else {

                    error_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            }
        });
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(
            duration_seconds
        )
    );

    running.store(
        false,
        std::memory_order_relaxed
    );

    for (auto& client : clients) {
        client.join();
    }

    const auto end = Clock::now();

    const double elapsed =
        std::chrono::duration<double>(
            end - start
        ).count();

    const auto requests =
        request_count.load(
            std::memory_order_relaxed
        );

    const auto completed =
        completed_count.load(
            std::memory_order_relaxed
        );

    const auto errors =
        error_count.load(
            std::memory_order_relaxed
        );

    const double throughput =
        elapsed > 0.0
            ? static_cast<double>(completed) / elapsed
            : 0.0;

    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;

    if (!latencies.empty()) {

        std::sort(
            latencies.begin(),
            latencies.end()
        );

        auto percentile =
            [&](double p) -> double {

                const std::size_t index =
                    static_cast<std::size_t>(
                        p * (latencies.size() - 1)
                    );

                return latencies[index];
            };

        p50 = percentile(0.50);
        p95 = percentile(0.95);
        p99 = percentile(0.99);
    }

    std::cout
        << "Requests: "
        << requests
        << '\n';

    std::cout
        << "Completed: "
        << completed
        << '\n';

    std::cout
        << "Errors: "
        << errors
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
        << "========== HTTP Latency ==========\n";

    std::cout
        << "P50: "
        << p50
        << " ms\n";

    std::cout
        << "P95: "
        << p95
        << " ms\n";

    std::cout
        << "P99: "
        << p99
        << " ms\n";

    std::cout
        << "===================================\n";

    return 0;
}
