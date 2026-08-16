#include "minisrv/runtime/onnxruntime_backend.h"

#include <stdexcept>
#include <vector>

namespace minisrv {

ONNXRuntimeBackend::ONNXRuntimeBackend(
    const std::string& model_path
)
    : env_(
        ORT_LOGGING_LEVEL_WARNING,
        "MiniServe"
      ),
      session_options_(),
      session_(
          nullptr
      )
{
    session_options_.SetIntraOpNumThreads(1);
    session_options_.SetInterOpNumThreads(1);

    session_ = Ort::Session(
        env_,
        model_path.c_str(),
        session_options_
    );

    Ort::AllocatorWithDefaultOptions allocator;

    auto input_name =
        session_.GetInputNameAllocated(
            0,
            allocator
        );

    auto output_name =
        session_.GetOutputNameAllocated(
            0,
            allocator
        );

    input_name_ = input_name.get();
    output_name_ = output_name.get();
}

void ONNXRuntimeBackend::infer(Batch& batch) {
    // TODO
}

} // namespace minisrv
