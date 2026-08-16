#pragma once

#include "minisrv/runtime/inference_request.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace minisrv {

class Batch {
public:
    void add(std::shared_ptr<InferenceRequest> request);

    std::size_t size() const;

    const std::vector<std::shared_ptr<InferenceRequest>>&
    requests() const;

private:
    std::vector<std::shared_ptr<InferenceRequest>> requests_;
};

} // namespace minisrv
