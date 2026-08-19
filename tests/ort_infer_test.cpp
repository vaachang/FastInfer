#include <onnxruntime_cxx_api.h>

#include <array>
#include <iostream>
#include <vector>

int main() {
    Ort::Env env(
        ORT_LOGGING_LEVEL_WARNING,
        "MiniServe"
    );

    Ort::SessionOptions session_options;

    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);

    Ort::Session session(
        env,
        "models/mul2.onnx",
        session_options
    );

    std::array<float, 6> input_data = {
        1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f
    };

    std::array<int64_t, 2> input_shape = {
        2,
        3
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

    const char* input_names[] = {
        "input"
    };

    const char* output_names[] = {
        "output"
    };

    auto output_tensors = session.Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_names,
        1
    );

    float* output_data =
        output_tensors[0].GetTensorMutableData<float>();

    std::cout << "Output:";

    for (int i = 0; i < 6; ++i) {
        std::cout << ' ' << output_data[i];
    }

    std::cout << '\n';

    return 0;
}
