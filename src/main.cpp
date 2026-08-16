#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch_scheduler.h"
#include "minisrv/runtime/fake_backend.h"
#include "minisrv/runtime/inference_request.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

int main() {
    using namespace minisrv;

    BoundedBlockingQueue<
        std::shared_ptr<InferenceRequest>
    > queue(16);

    FakeInferenceBackend backend;

    BatchScheduler scheduler(
        queue,
        backend,
        4,
        std::chrono::milliseconds(100)
    );

    scheduler.start();

    std::vector<
        std::future<InferenceResult>
    > futures;

    for (int i = 0; i < 4; ++i) {
        auto request =
            std::make_shared<InferenceRequest>();

        request->id = i + 1;

        request->input = {
            static_cast<float>(i + 1),
            static_cast<float>(i + 2),
            static_cast<float>(i + 3)
        };

        futures.emplace_back(
            request->promise.get_future()
        );

        queue.push(std::move(request));
    }

    for (auto& future : futures) {
        auto result = future.get();

        std::cout << "Result:";

        for (float value : result.output) {
            std::cout << ' ' << value;
        }

        std::cout << '\n';
    }

    scheduler.stop();

    return 0;
}
