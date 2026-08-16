#pragma once

#include <cstdint>
#include <future>
#include <vector>

namespace minisrv {

using RequestId = std::uint64_t;

struct InferenceResult {
    std::vector<float> output;
};

struct InferenceRequest {
    RequestId id;

    std::vector<float> input;

    std::promise<InferenceResult> promise;
};

} // namespace minisrv
