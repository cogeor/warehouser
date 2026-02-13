#include "warehouser_inference/policy_inference.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

#ifdef ONNXRUNTIME_AVAILABLE
#include <onnxruntime_cxx_api.h>
#endif

namespace warehouser_inference {

struct PolicyInference::Impl {
#ifdef ONNXRUNTIME_AVAILABLE
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "inference"};
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    std::vector<const char*> input_names{"observation"};
    std::vector<const char*> output_names{"action"};
#endif
    ModelInfo model_info;
};

PolicyInference::PolicyInference() : impl_(std::make_unique<Impl>()) {
#ifdef ONNXRUNTIME_AVAILABLE
    impl_->session_options.SetIntraOpNumThreads(1);
    impl_->session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#endif
}

PolicyInference::~PolicyInference() = default;

PolicyInference::PolicyInference(PolicyInference&&) noexcept = default;
PolicyInference& PolicyInference::operator=(PolicyInference&&) noexcept = default;

Result<void> PolicyInference::loadModel(const std::string& model_path) {
    // Check file exists
    std::ifstream file(model_path);
    if (!file.good()) {
        return Result<void>::failure("Model file not found: " + model_path);
    }
    file.close();

#ifdef ONNXRUNTIME_AVAILABLE
    try {
        impl_->session = std::make_unique<Ort::Session>(
            impl_->env, model_path.c_str(), impl_->session_options);

        // Get input/output info
        Ort::AllocatorWithDefaultOptions allocator;

        auto input_count = impl_->session->GetInputCount();
        if (input_count != 1) {
            return Result<void>::failure("Expected 1 input, got " + std::to_string(input_count));
        }

        auto input_shape = impl_->session->GetInputTypeInfo(0)
            .GetTensorTypeAndShapeInfo().GetShape();
        if (input_shape.size() == 2) {
            impl_->model_info.obs_dim = input_shape[1];
        }

        auto output_count = impl_->session->GetOutputCount();
        if (output_count != 1) {
            return Result<void>::failure("Expected 1 output, got " + std::to_string(output_count));
        }

        auto output_shape = impl_->session->GetOutputTypeInfo(0)
            .GetTensorTypeAndShapeInfo().GetShape();
        if (output_shape.size() == 2) {
            impl_->model_info.action_dim = output_shape[1];
        }

        impl_->model_info.path = model_path;
        impl_->model_info.loaded = true;

        return Result<void>::success();

    } catch (const Ort::Exception& e) {
        return Result<void>::failure("ONNX Runtime error: " + std::string(e.what()));
    }
#else
    // Stub implementation - mark as loaded for testing
    impl_->model_info.path = model_path;
    impl_->model_info.loaded = true;
    impl_->model_info.obs_dim = 8;
    impl_->model_info.action_dim = 4;
    return Result<void>::success();
#endif
}

Result<Action> PolicyInference::infer(const std::vector<float>& observation) {
    if (!impl_->model_info.loaded) {
        return Result<Action>::failure("No model loaded");
    }

    if (static_cast<int64_t>(observation.size()) != impl_->model_info.obs_dim) {
        return Result<Action>::failure(
            "Observation size mismatch: expected " +
            std::to_string(impl_->model_info.obs_dim) +
            ", got " + std::to_string(observation.size()));
    }

#ifdef ONNXRUNTIME_AVAILABLE
    try {
        // Create input tensor
        std::vector<int64_t> input_shape = {1, impl_->model_info.obs_dim};
        Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        // Need non-const data for ONNX Runtime
        std::vector<float> obs_copy = observation;

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            mem_info,
            obs_copy.data(),
            obs_copy.size(),
            input_shape.data(),
            input_shape.size());

        // Run inference
        auto output_tensors = impl_->session->Run(
            Ort::RunOptions{nullptr},
            impl_->input_names.data(), &input_tensor, 1,
            impl_->output_names.data(), impl_->output_names.size());

        // Extract action
        float* action_data = output_tensors[0].GetTensorMutableData<float>();

        Action action;
        action.linear = std::clamp(action_data[0], -1.0f, 1.0f);
        action.angular = std::clamp(action_data[1], -1.0f, 1.0f);
        action.pick = action_data[2];
        action.place = action_data[3];

        return Result<Action>::success(action);

    } catch (const Ort::Exception& e) {
        return Result<Action>::failure("Inference error: " + std::string(e.what()));
    }
#else
    // Stub implementation - return simple reactive behavior
    // Move towards goal based on observation
    // obs = [robot_x, robot_y, robot_theta, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
    Action action;

    if (observation.size() >= 8) {
        float goal_heading = observation[6];
        float goal_dist = observation[5];

        // Simple proportional control
        action.angular = std::clamp(goal_heading * 2.0f, -1.0f, 1.0f);

        // Move forward if roughly facing goal
        if (std::abs(goal_heading) < 0.5f) {
            action.linear = std::clamp(goal_dist * 0.5f, 0.0f, 1.0f);
        } else {
            action.linear = 0.0f;  // Turn first
        }

        // Pick if close and not carrying
        bool is_carrying = observation[7] > 0.5f;
        if (goal_dist < 0.5f && !is_carrying) {
            action.pick = 1.0f;
        }

        // Place if carrying and at destination
        if (goal_dist < 0.5f && is_carrying) {
            action.place = 1.0f;
        }
    }

    return Result<Action>::success(action);
#endif
}

bool PolicyInference::isLoaded() const noexcept {
    return impl_->model_info.loaded;
}

const ModelInfo& PolicyInference::getModelInfo() const noexcept {
    return impl_->model_info;
}

}  // namespace warehouser_inference
