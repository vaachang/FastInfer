#include "minisrv/runtime/batch.h"

#include <stdexcept>

namespace minisrv {

void Batch::add(std::shared_ptr<InferenceRequest> request) {
    if (!request) {
        throw std::invalid_argument(
            "cannot add null request to batch"
        );
    }

    requests_.push_back(std::move(request));
}

std::size_t Batch::size() const {
    return requests_.size();
}

const std::vector<std::shared_ptr<InferenceRequest>>&
Batch::requests() const {
    return requests_;
}

} // namespace minisrv
