#pragma once

#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/inference_request.h"

#include <cstdint>
#include <memory>
#include <string>

namespace minisrv {

class HttpServer {
public:
    using RequestQueue =
        BoundedBlockingQueue<
            std::shared_ptr<InferenceRequest>
        >;

    HttpServer(
        RequestQueue& queue,
        const std::string& host,
        int port
    );

    void start();

private:
    RequestQueue& queue_;
    std::string host_;
    int port_;
};

} // namespace minisrv
