#include "minisrv/runtime/onnxruntime_backend.h"

#include <stdexcept>
#include <vector>
#include <cstring>
#include <array>

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
    if (batch.size() == 0) {
        return;
    }

    constexpr std::size_t feature_size = 3;

    const std::size_t batch_size = batch.size();

    // --------------------------------------------------
    // 1. 将 Batch 中所有请求的数据拼接成连续内存
    //
    // Request 1: [1, 2, 3]
    // Request 2: [4, 5, 6]
    //
    // =>
    //
    // [1, 2, 3, 4, 5, 6]
    // shape = [2, 3]
    // --------------------------------------------------

    std::vector<float> input_data;
    input_data.reserve(batch_size * feature_size);

    for (const auto& request : batch.requests()) {
        if (request->input.size() != feature_size) {
            throw std::runtime_error(
                "Invalid input size"
            );
        }

        input_data.insert(
            input_data.end(),
            request->input.begin(),
            request->input.end()
        );
    }

    // --------------------------------------------------
    // 2. 创建 ONNX Tensor
    // --------------------------------------------------

    std::array<int64_t, 2> input_shape = {
        static_cast<int64_t>(batch_size),
        static_cast<int64_t>(feature_size)
    };

    Ort::MemoryInfo memory_info =
        Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator,
            OrtMemTypeDefault
        );

    Ort::Value input_tensor =
        Ort::Value::CreateTensor<float>(
            memory_info,
            input_data.data(),
            input_data.size(),
            input_shape.data(),
            input_shape.size()
        );

    // --------------------------------------------------
    // 3. 执行一次 ONNX Runtime 推理
    // --------------------------------------------------

    const char* input_names[] = {
        input_name_.c_str()
    };

    const char* output_names[] = {
        output_name_.c_str()
    };

    auto output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_names,
        1
    );

    // --------------------------------------------------
    // 4. 获取输出 Tensor
    // --------------------------------------------------

    float* output_data =
        output_tensors[0]
            .GetTensorMutableData<float>();

    // --------------------------------------------------
    // 5. 将 Batch 输出拆回各个请求
    //
    // [2,4,6, 8,10,12]
    //
    // =>
    //
    // Request 1 -> [2,4,6]
    // Request 2 -> [8,10,12]
    // --------------------------------------------------

    for (std::size_t i = 0; i < batch_size; ++i) {
        InferenceResult result;

        result.output.assign(
            output_data + i * feature_size,
            output_data + (i + 1) * feature_size
        );

        batch.requests()[i]->promise.set_value(
            std::move(result)
        );
    }
}

} // namespace minisrv
