#pragma once

#include "minisrv/runtime/inference_backend.h"

namespace minisrv {

class FakeInferenceBackend : public InferenceBackend {
public:
    void infer(Batch& batch) override;
};

} // namespace minisrv
