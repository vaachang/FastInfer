#pragma once

#include "minisrv/runtime/batch.h"

namespace minisrv {

class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;

    virtual void infer(Batch& batch) = 0;
};

} // namespace minisrv
