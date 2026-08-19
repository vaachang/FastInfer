#include "minisrv/server/http_server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>

namespace minisrv {

using json = nlohmann::json;

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

    httplib::Server server;

    server.Get(
        "/health",
        [](const httplib::Request&,
           httplib::Response& response) {

            response.set_content(
                R"({"status":"ok"})",
                "application/json"
            );
        }
    );

    std::cout
        << "HTTP server listening on "
        << host_
        << ":"
        << port_
        << '\n';

    server.listen(
        host_.c_str(),
        port_
    );
}

} // namespace minisrv
