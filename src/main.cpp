#include "minisrv/runtime/fake_backend.h"

#include <iostream>
#include <memory>

int main() {
    using namespace minisrv;

    auto request1 =
        std::make_shared<InferenceRequest>();

    request1->id = 1;
    request1->input = {1.0f, 2.0f, 3.0f};

    auto request2 =
        std::make_shared<InferenceRequest>();

    request2->id = 2;
    request2->input = {4.0f, 5.0f, 6.0f};

    auto future1 = request1->promise.get_future();
    auto future2 = request2->promise.get_future();

    Batch batch;

    batch.add(request1);
    batch.add(request2);

    FakeInferenceBackend backend;

    backend.infer(batch);

    auto result1 = future1.get();
    auto result2 = future2.get();

    std::cout << "Request "
              << request1->id
              << ":";

    for (float value : result1.output) {
        std::cout << ' ' << value;
    }

    std::cout << '\n';

    std::cout << "Request "
              << request2->id
              << ":";

    for (float value : result2.output) {
        std::cout << ' ' << value;
    }

    std::cout << '\n';

    return 0;
}
