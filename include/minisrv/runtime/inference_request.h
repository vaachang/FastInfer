#pragma once

#include <chrono>
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

    // Benchmark timestamps
    std::chrono::steady_clock::time_point submit_time;
};

} // namespace minisrv
