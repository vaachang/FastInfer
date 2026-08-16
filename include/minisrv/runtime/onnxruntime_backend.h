#pragma once

#include "minisrv/runtime/inference_backend.h"

#include <onnxruntime_cxx_api.h>

#include <string>

namespace minisrv {

class ONNXRuntimeBackend : public InferenceBackend {
public:
    ONNXRuntimeBackend(
        const std::string& model_path
    );

    void infer(Batch& batch) override;

private:
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    Ort::Session session_;

    std::string input_name_;
    std::string output_name_;
};

} // namespace minisrv
