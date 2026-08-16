#include "minisrv/runtime/batch.h"

#include <iostream>

int main() {
    auto request1 =
        std::make_shared<minisrv::InferenceRequest>();

    request1->id = 1;
    request1->input = {1.0f, 2.0f};

    auto request2 =
        std::make_shared<minisrv::InferenceRequest>();

    request2->id = 2;
    request2->input = {3.0f, 4.0f};

    minisrv::Batch batch;

    batch.add(request1);
    batch.add(request2);

    std::cout
        << "Batch size: "
        << batch.size()
        << '\n';

    for (const auto& request : batch.requests()) {
        std::cout
            << "Request ID: "
            << request->id
            << '\n';
    }

    return 0;
}
