#include <onnxruntime_cxx_api.h>

#include <iostream>

int main() {
    Ort::Env env(
        ORT_LOGGING_LEVEL_WARNING,
        "MiniServe"
    );

    Ort::SessionOptions session_options;

    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);

    std::cout << "ONNX Runtime initialized successfully\n";

    return 0;
}
