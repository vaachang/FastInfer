#include "minisrv/server/http_server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <iostream>
#include <stdexcept>

namespace minisrv {

using json = nlohmann::json;

namespace {

std::atomic<RequestId> next_request_id{1};

} // namespace

HttpServer::~HttpServer() {
    stop();
}

HttpServer::HttpServer(
    RequestQueue& queue,
    const std::string& host,
    int port
)
    : queue_(queue),
      host_(host),
      port_(port)
{
}

void HttpServer::start() {
    server_thread_ =
        std::thread(&HttpServer::run, this);
}

void HttpServer::stop() {

    server_.stop();

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void HttpServer::run() {

    httplib::Server server_;

    // --------------------------------------------------
    // GET /health
    // --------------------------------------------------

    server_.Get(
        "/health",
        [](const httplib::Request&,
           httplib::Response& response) {

            response.set_content(
                R"({"status":"ok"})",
                "application/json"
            );
        }
    );

    // --------------------------------------------------
    // POST /infer
    // --------------------------------------------------

    server_.Post(
        "/infer",
        [this](
            const httplib::Request& request,
            httplib::Response& response
        ) {

            try {

                // --------------------------------------
                // 1. Parse JSON
                // --------------------------------------

                const auto body =
                    json::parse(request.body);

                if (!body.contains("input") ||
                    !body["input"].is_array()) {

                    response.status = 400;

                    response.set_content(
                        R"({"error":"input must be an array"})",
                        "application/json"
                    );

                    return;
                }

                // --------------------------------------
                // 2. Create inference request
                // --------------------------------------

                auto inference_request =
                    std::make_shared<InferenceRequest>();

                inference_request->id =
                    next_request_id.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                for (const auto& value : body["input"]) {

                    if (!value.is_number()) {

                        response.status = 400;

                        response.set_content(
                            R"({"error":"input must contain numbers"})",
                            "application/json"
                        );

                        return;
                    }

                    inference_request->input.push_back(
                        value.get<float>()
                    );
                }

                // --------------------------------------
                // 3. Record submit time
                // --------------------------------------

                inference_request->submit_time =
                    std::chrono::steady_clock::now();

                // --------------------------------------
                // 4. Get future
                // --------------------------------------

                auto future =
                    inference_request->promise.get_future();

                // --------------------------------------
                // 5. Submit request to queue
                // --------------------------------------

                if (!queue_.push(inference_request)) {

                    response.status = 503;

                    response.set_content(
                        R"({"error":"server overloaded"})",
                        "application/json"
                    );

                    return;
                }

                // --------------------------------------
                // 6. Wait for inference result
                // --------------------------------------

                auto result = future.get();

                // --------------------------------------
                // 7. Construct JSON response
                // --------------------------------------

                json output;

                output["request_id"] =
                    inference_request->id;

                output["output"] =
                    result.output;

                response.set_content(
                    output.dump(),
                    "application/json"
                );

            } catch (
                const json::exception& e
            ) {

                response.status = 400;

                json error;

                error["error"] =
                    "invalid json";

                error["message"] =
                    e.what();

                response.set_content(
                    error.dump(),
                    "application/json"
                );

            } catch (
                const std::exception& e
            ) {

                response.status = 500;

                json error;

                error["error"] =
                    "internal server error";

                error["message"] =
                    e.what();

                response.set_content(
                    error.dump(),
                    "application/json"
                );
            }
        }
    );

    std::cout
        << "HTTP server listening on "
        << host_
        << ":"
        << port_
        << '\n';

    server_.listen(
        host_.c_str(),
        port_
    );
}

} // namespace minisrv
