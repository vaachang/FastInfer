#pragma once

#include "minisrv/core/bounded_queue.h"
#include "minisrv/runtime/inference_request.h"

#include <memory>
#include <string>
#include <thread>

#include <httplib.h>

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

    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void start();
    void stop();

private:
    void run();

private:
    RequestQueue& queue_;

    std::string host_;
    int port_;

    httplib::Server server_;
    std::thread server_thread_;
};

} // namespace minisrv
