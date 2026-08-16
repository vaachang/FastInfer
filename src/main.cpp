#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch_scheduler.h"
#include "minisrv/runtime/inference_request.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

int main() {
    using namespace minisrv;

    BoundedBlockingQueue<
        std::shared_ptr<InferenceRequest>
    > queue(16);

    BatchScheduler scheduler(
        queue,
        4,
        std::chrono::milliseconds(100)
    );

    scheduler.start();

    for (int i = 0; i < 2; ++i) {
        auto request =
            std::make_shared<InferenceRequest>();

        request->id = i;

        queue.push(std::move(request));
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(200)
    );

    scheduler.stop();

    return 0;
}
