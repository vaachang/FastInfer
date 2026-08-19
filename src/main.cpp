#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/batch_scheduler.h"
#include "minisrv/runtime/inference_request.h"
#include "minisrv/runtime/onnxruntime_backend.h"
#include "minisrv/server/http_server.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

int main() {
    using namespace minisrv;

    BoundedBlockingQueue<
        std::shared_ptr<InferenceRequest>
    > queue(16);

    ONNXRuntimeBackend backend(
        "models/mul2.onnx"
    );

    BatchScheduler scheduler(
        queue,
        backend,
        4,
        std::chrono::milliseconds(100)
    );

    HttpServer server(
        queue,
        "0.0.0.0",
        8080
    );

    scheduler.start();
    server.start();

    std::cout
        << "MiniServe started.\n";

    // 暂时让主线程保持运行
    std::this_thread::sleep_for(
        std::chrono::hours(24)
    );

    server.stop();
    scheduler.stop();

    return 0;
}
