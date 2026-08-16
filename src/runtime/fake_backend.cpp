#include "minisrv/runtime/fake_backend.h"

namespace minisrv {

void FakeInferenceBackend::infer(Batch& batch) {
    for (const auto& request : batch.requests()) {
        InferenceResult result;

        result.output.reserve(
            request->input.size()
        );

        for (float value : request->input) {
            result.output.push_back(value * 2.0f);
        }

        request->promise.set_value(
            std::move(result)
        );
    }
}

} // namespace minisrv
