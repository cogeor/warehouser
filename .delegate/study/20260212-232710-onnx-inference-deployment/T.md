# Template: ONNX Inference Deployment for Robotics

Created: 2026-02-12T23:35:00Z

## Source

No templates available in `.delegate/templates/`.

Analysis based on web research documented in `S.md`:
- ONNX Runtime official documentation
- TensorRT execution provider patterns
- Safety-critical robotics deployment patterns
- Edge device optimization techniques
- Production monitoring and observability patterns

## Pattern

### 1. ONNX Runtime Session Configuration

**Pattern**: Pre-optimized model loading with configurable execution providers

**Key Insight**: For robotics with frequent node restarts, offline graph optimization reduces startup latency by 30-40%. TensorRT engine caching prevents rebuild overhead on subsequent sessions.

**C++ Implementation Template**:

```cpp
#pragma once

#include <onnxruntime_cxx_api.h>
#include <expected>
#include <string>
#include <memory>
#include <chrono>

namespace warehouser::inference {

struct SessionConfig {
    std::string model_path;
    std::string execution_provider = "CPU";  // CPU, CUDA, TensorRT
    int32_t intra_op_threads = 4;
    int32_t inter_op_threads = 2;
    GraphOptimizationLevel optimization_level = GraphOptimizationLevel::ORT_ENABLE_ALL;
    std::string tensorrt_cache_path = "";  // Empty = no caching
    bool enable_fp16 = false;
};

class InferenceSession {
public:
    static std::expected<std::unique_ptr<InferenceSession>, std::string>
    create(const SessionConfig& config) {
        try {
            auto session = std::make_unique<InferenceSession>();

            // Create session options
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(config.intra_op_threads);
            session_options.SetInterOpNumThreads(config.inter_op_threads);
            session_options.SetGraphOptimizationLevel(config.optimization_level);

            // Configure execution provider
            if (config.execution_provider == "CUDA") {
                OrtCUDAProviderOptions cuda_options{};
                session_options.AppendExecutionProvider_CUDA(cuda_options);
            }
            else if (config.execution_provider == "TensorRT") {
                OrtTensorRTProviderOptions trt_options{};

                // Enable TensorRT engine caching for faster subsequent startups
                if (!config.tensorrt_cache_path.empty()) {
                    trt_options.trt_engine_cache_enable = 1;
                    trt_options.trt_engine_cache_path = config.tensorrt_cache_path.c_str();
                }

                // Enable FP16 for 2x speedup with <0.5% accuracy loss
                if (config.enable_fp16) {
                    trt_options.trt_fp16_enable = 1;
                }

                session_options.AppendExecutionProvider_TensorRT(trt_options);
                // Fallback to CUDA if TensorRT fails
                session_options.AppendExecutionProvider_CUDA(OrtCUDAProviderOptions{});
            }

            // Create session
            session->session_ = std::make_unique<Ort::Session>(
                session->env_,
                config.model_path.c_str(),
                session_options
            );

            // Extract input/output metadata
            session->extractMetadata();

            return session;

        } catch (const Ort::Exception& e) {
            return std::unexpected(std::string("ONNX Runtime error: ") + e.what());
        }
    }

    struct InferenceResult {
        std::vector<float> output;
        float confidence;
        std::chrono::microseconds latency;
    };

    std::expected<InferenceResult, std::string>
    infer(const std::vector<float>& input_data) {
        auto start = std::chrono::high_resolution_clock::now();

        try {
            // Create input tensor
            std::vector<int64_t> input_shape = {1, static_cast<int64_t>(input_data.size())};

            auto memory_info = Ort::MemoryInfo::CreateCpu(
                OrtArenaAllocator,
                OrtMemTypeDefault
            );

            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                memory_info,
                const_cast<float*>(input_data.data()),
                input_data.size(),
                input_shape.data(),
                input_shape.size()
            );

            // Run inference
            auto output_tensors = session_->Run(
                Ort::RunOptions{nullptr},
                input_names_.data(),
                &input_tensor,
                1,
                output_names_.data(),
                output_names_.size()
            );

            // Extract output
            float* output_data = output_tensors[0].GetTensorMutableData<float>();
            size_t output_size = output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount();

            std::vector<float> output(output_data, output_data + output_size);

            // Calculate confidence (example: softmax max value)
            float max_val = *std::max_element(output.begin(), output.end());

            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            return InferenceResult{
                .output = std::move(output),
                .confidence = max_val,
                .latency = latency
            };

        } catch (const Ort::Exception& e) {
            return std::unexpected(std::string("Inference error: ") + e.what());
        }
    }

private:
    InferenceSession() : env_(ORT_LOGGING_LEVEL_WARNING, "warehouser_inference") {}

    void extractMetadata() {
        // Get input names
        Ort::AllocatorWithDefaultOptions allocator;
        size_t num_input_nodes = session_->GetInputCount();

        for (size_t i = 0; i < num_input_nodes; i++) {
            auto input_name = session_->GetInputNameAllocated(i, allocator);
            input_names_.push_back(input_name.get());
        }

        // Get output names
        size_t num_output_nodes = session_->GetOutputCount();
        for (size_t i = 0; i < num_output_nodes; i++) {
            auto output_name = session_->GetOutputNameAllocated(i, allocator);
            output_names_.push_back(output_name.get());
        }
    }

    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
};

} // namespace warehouser::inference
```

### 2. Model Validation and Safety

**Pattern**: Pre-deployment validation with test inputs before accepting model as production-ready

**Key Insight**: Never deploy a model without validating it produces expected outputs on known test cases. Prevents deploying corrupted or incompatible models.

**C++ Implementation Template**:

```cpp
#pragma once

#include "inference_session.hpp"
#include <expected>
#include <vector>
#include <cmath>

namespace warehouser::inference {

struct ValidationTestCase {
    std::vector<float> input;
    std::vector<float> expected_output;
    float tolerance = 0.01f;
};

struct ValidationResult {
    bool passed;
    std::string error_message;
    std::vector<float> actual_output;
};

class ModelValidator {
public:
    static ValidationResult validate(
        InferenceSession& session,
        const std::vector<ValidationTestCase>& test_cases
    ) {
        for (size_t i = 0; i < test_cases.size(); ++i) {
            const auto& test = test_cases[i];

            auto result = session.infer(test.input);
            if (!result) {
                return ValidationResult{
                    .passed = false,
                    .error_message = "Inference failed: " + result.error(),
                    .actual_output = {}
                };
            }

            // Check shape
            if (result->output.size() != test.expected_output.size()) {
                return ValidationResult{
                    .passed = false,
                    .error_message = "Output shape mismatch on test " + std::to_string(i),
                    .actual_output = result->output
                };
            }

            // Check values
            for (size_t j = 0; j < test.expected_output.size(); ++j) {
                float diff = std::abs(result->output[j] - test.expected_output[j]);
                if (diff > test.tolerance) {
                    return ValidationResult{
                        .passed = false,
                        .error_message = "Output value mismatch on test " + std::to_string(i) +
                                       ", element " + std::to_string(j) +
                                       ": expected " + std::to_string(test.expected_output[j]) +
                                       ", got " + std::to_string(result->output[j]),
                        .actual_output = result->output
                    };
                }
            }

            // Check for NaN/Inf
            for (float val : result->output) {
                if (std::isnan(val) || std::isinf(val)) {
                    return ValidationResult{
                        .passed = false,
                        .error_message = "Output contains NaN or Inf on test " + std::to_string(i),
                        .actual_output = result->output
                    };
                }
            }
        }

        return ValidationResult{
            .passed = true,
            .error_message = "",
            .actual_output = {}
        };
    }

    static std::expected<float, std::string>
    validateLatency(InferenceSession& session,
                   const std::vector<float>& test_input,
                   int32_t num_warmup = 10,
                   int32_t num_iterations = 100) {
        // Warmup
        for (int32_t i = 0; i < num_warmup; ++i) {
            auto result = session.infer(test_input);
            if (!result) {
                return std::unexpected("Warmup inference failed: " + result.error());
            }
        }

        // Measure
        std::vector<float> latencies;
        latencies.reserve(num_iterations);

        for (int32_t i = 0; i < num_iterations; ++i) {
            auto result = session.infer(test_input);
            if (!result) {
                return std::unexpected("Latency test inference failed: " + result.error());
            }
            latencies.push_back(result->latency.count() / 1000.0f);  // Convert to ms
        }

        // Calculate p95
        std::sort(latencies.begin(), latencies.end());
        size_t p95_idx = static_cast<size_t>(num_iterations * 0.95);

        return latencies[p95_idx];
    }
};

} // namespace warehouser::inference
```

### 3. Timeout and Fallback Policy

**Pattern**: Hard timeout on inference with automatic fallback to safe action

**Key Insight**: For safety-critical robotics, inference delays >3 seconds make perception data stale and unsafe. Target <100ms for perception-action loops. Always have a provably safe fallback policy.

**C++ Implementation Template**:

```cpp
#pragma once

#include "inference_session.hpp"
#include <future>
#include <chrono>
#include <expected>

namespace warehouser::inference {

class SafeInferenceWrapper {
public:
    struct Config {
        std::chrono::milliseconds timeout{50};
        float confidence_threshold = 0.7f;
        bool enable_fallback = true;
    };

    SafeInferenceWrapper(std::unique_ptr<InferenceSession> session, Config config)
        : session_(std::move(session))
        , config_(config) {}

    enum class FallbackReason {
        None,
        Timeout,
        LowConfidence,
        InferenceError,
        OutputInvalid
    };

    struct SafeInferenceResult {
        std::vector<float> action;
        float confidence;
        FallbackReason fallback_reason = FallbackReason::None;
        std::chrono::microseconds latency;
    };

    SafeInferenceResult inferWithFallback(
        const std::vector<float>& observation,
        const std::function<std::vector<float>()>& fallback_policy
    ) {
        // Launch inference in async task
        auto future = std::async(std::launch::async, [this, &observation]() {
            return session_->infer(observation);
        });

        // Wait with timeout
        auto status = future.wait_for(config_.timeout);

        if (status == std::future_status::timeout) {
            // Timeout - use fallback
            timeout_count_++;
            return SafeInferenceResult{
                .action = fallback_policy(),
                .confidence = 0.0f,
                .fallback_reason = FallbackReason::Timeout,
                .latency = config_.timeout
            };
        }

        auto result = future.get();

        if (!result) {
            // Inference error - use fallback
            error_count_++;
            return SafeInferenceResult{
                .action = fallback_policy(),
                .confidence = 0.0f,
                .fallback_reason = FallbackReason::InferenceError,
                .latency = std::chrono::microseconds{0}
            };
        }

        // Check for invalid output (NaN/Inf)
        bool has_invalid = std::any_of(result->output.begin(), result->output.end(),
            [](float v) { return std::isnan(v) || std::isinf(v); });

        if (has_invalid) {
            invalid_output_count_++;
            return SafeInferenceResult{
                .action = fallback_policy(),
                .confidence = 0.0f,
                .fallback_reason = FallbackReason::OutputInvalid,
                .latency = result->latency
            };
        }

        // Check confidence threshold
        if (config_.enable_fallback && result->confidence < config_.confidence_threshold) {
            low_confidence_count_++;
            return SafeInferenceResult{
                .action = fallback_policy(),
                .confidence = result->confidence,
                .fallback_reason = FallbackReason::LowConfidence,
                .latency = result->latency
            };
        }

        // All checks passed - use model output
        successful_count_++;
        return SafeInferenceResult{
            .action = std::move(result->output),
            .confidence = result->confidence,
            .fallback_reason = FallbackReason::None,
            .latency = result->latency
        };
    }

    struct Statistics {
        uint64_t successful_count;
        uint64_t timeout_count;
        uint64_t error_count;
        uint64_t low_confidence_count;
        uint64_t invalid_output_count;

        float fallback_rate() const {
            uint64_t total = successful_count + timeout_count + error_count +
                           low_confidence_count + invalid_output_count;
            if (total == 0) return 0.0f;
            return static_cast<float>(timeout_count + error_count +
                   low_confidence_count + invalid_output_count) / total;
        }
    };

    Statistics getStatistics() const {
        return Statistics{
            .successful_count = successful_count_,
            .timeout_count = timeout_count_,
            .error_count = error_count_,
            .low_confidence_count = low_confidence_count_,
            .invalid_output_count = invalid_output_count_
        };
    }

    void resetStatistics() {
        successful_count_ = 0;
        timeout_count_ = 0;
        error_count_ = 0;
        low_confidence_count_ = 0;
        invalid_output_count_ = 0;
    }

private:
    std::unique_ptr<InferenceSession> session_;
    Config config_;

    // Statistics
    std::atomic<uint64_t> successful_count_{0};
    std::atomic<uint64_t> timeout_count_{0};
    std::atomic<uint64_t> error_count_{0};
    std::atomic<uint64_t> low_confidence_count_{0};
    std::atomic<uint64_t> invalid_output_count_{0};
};

} // namespace warehouser::inference
```

### 4. Hot-Swap Model Loading

**Pattern**: Load new model in background, validate, then atomically swap without stopping inference service

**Key Insight**: For robotics where downtime is unacceptable, pre-load and validate new models before swapping to avoid inference failures during transition.

**C++ Implementation Template**:

```cpp
#pragma once

#include "inference_session.hpp"
#include "model_validator.hpp"
#include <shared_mutex>
#include <expected>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace warehouser::inference {

struct ModelMetadata {
    std::string version;
    std::string training_timestamp;
    float expected_p95_latency_ms;
    std::vector<ValidationTestCase> validation_cases;

    static std::expected<ModelMetadata, std::string>
    loadFromJson(const std::filesystem::path& json_path) {
        try {
            std::ifstream f(json_path);
            nlohmann::json j = nlohmann::json::parse(f);

            ModelMetadata meta;
            meta.version = j["version"];
            meta.training_timestamp = j["training_timestamp"];
            meta.expected_p95_latency_ms = j["expected_p95_latency_ms"];

            // Load validation cases
            for (const auto& test : j["validation_cases"]) {
                ValidationTestCase tc;
                tc.input = test["input"].get<std::vector<float>>();
                tc.expected_output = test["expected_output"].get<std::vector<float>>();
                tc.tolerance = test.value("tolerance", 0.01f);
                meta.validation_cases.push_back(tc);
            }

            return meta;
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Failed to load metadata: ") + e.what());
        }
    }
};

class HotSwapModelManager {
public:
    HotSwapModelManager(const SessionConfig& base_config)
        : base_config_(base_config) {}

    std::expected<void, std::string>
    loadInitialModel(const std::filesystem::path& model_path,
                    const std::filesystem::path& metadata_path) {
        // Load metadata
        auto meta_result = ModelMetadata::loadFromJson(metadata_path);
        if (!meta_result) {
            return std::unexpected(meta_result.error());
        }

        // Load model
        auto config = base_config_;
        config.model_path = model_path.string();

        auto session_result = InferenceSession::create(config);
        if (!session_result) {
            return std::unexpected(session_result.error());
        }

        // Validate
        auto validation = ModelValidator::validate(
            *session_result.value(),
            meta_result->validation_cases
        );

        if (!validation.passed) {
            return std::unexpected("Model validation failed: " + validation.error_message);
        }

        // Check latency
        if (!meta_result->validation_cases.empty()) {
            auto latency_result = ModelValidator::validateLatency(
                *session_result.value(),
                meta_result->validation_cases[0].input
            );

            if (!latency_result) {
                return std::unexpected(latency_result.error());
            }

            // Warn if latency exceeds expected
            if (*latency_result > meta_result->expected_p95_latency_ms * 1.5f) {
                // Log warning but don't fail
                // In production, you might want to publish this to diagnostics
            }
        }

        // Atomic swap
        {
            std::unique_lock lock(mutex_);
            current_session_ = std::move(session_result.value());
            current_metadata_ = std::move(*meta_result);
        }

        return {};
    }

    std::expected<void, std::string>
    hotSwapModel(const std::filesystem::path& new_model_path,
                const std::filesystem::path& new_metadata_path) {
        // Load and validate new model WITHOUT holding lock
        auto meta_result = ModelMetadata::loadFromJson(new_metadata_path);
        if (!meta_result) {
            return std::unexpected(meta_result.error());
        }

        auto config = base_config_;
        config.model_path = new_model_path.string();

        auto session_result = InferenceSession::create(config);
        if (!session_result) {
            return std::unexpected(session_result.error());
        }

        auto validation = ModelValidator::validate(
            *session_result.value(),
            meta_result->validation_cases
        );

        if (!validation.passed) {
            return std::unexpected("New model validation failed: " + validation.error_message);
        }

        // Only lock for the atomic swap
        {
            std::unique_lock lock(mutex_);
            current_session_ = std::move(session_result.value());
            current_metadata_ = std::move(*meta_result);
        }

        return {};
    }

    std::expected<InferenceSession::InferenceResult, std::string>
    infer(const std::vector<float>& input) {
        std::shared_lock lock(mutex_);
        if (!current_session_) {
            return std::unexpected("No model loaded");
        }
        return current_session_->infer(input);
    }

    ModelMetadata getCurrentMetadata() const {
        std::shared_lock lock(mutex_);
        return current_metadata_;
    }

private:
    SessionConfig base_config_;
    mutable std::shared_mutex mutex_;
    std::unique_ptr<InferenceSession> current_session_;
    ModelMetadata current_metadata_;
};

} // namespace warehouser::inference
```

### 5. Performance Monitoring

**Pattern**: Sliding window statistics with p50/p95/p99 latency tracking and ROS2 diagnostics integration

**Key Insight**: Track p95 latency (not just average) and alert if it exceeds 2x baseline. Log model version, execution provider, and fallback events for debugging production issues.

**C++ Implementation Template**:

```cpp
#pragma once

#include <deque>
#include <chrono>
#include <algorithm>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>

namespace warehouser::inference {

class PerformanceMonitor {
public:
    struct Config {
        size_t window_size = 1000;  // Number of inferences to track
        float p95_alert_threshold_multiplier = 2.0f;
        float fallback_rate_alert_threshold = 0.01f;  // 1%
    };

    PerformanceMonitor(Config config = {}) : config_(config) {}

    void recordInference(float latency_ms, float confidence, bool fallback_used) {
        latencies_.push_back(latency_ms);
        confidences_.push_back(confidence);

        if (fallback_used) {
            fallback_count_++;
        }
        total_count_++;

        // Maintain window size
        if (latencies_.size() > config_.window_size) {
            latencies_.pop_front();
            confidences_.pop_front();
        }
    }

    struct Statistics {
        float p50_latency_ms;
        float p95_latency_ms;
        float p99_latency_ms;
        float mean_latency_ms;
        float mean_confidence;
        float fallback_rate;
        uint64_t total_inferences;
    };

    Statistics getStatistics() const {
        if (latencies_.empty()) {
            return Statistics{};
        }

        // Calculate latency percentiles
        std::vector<float> sorted_latencies(latencies_.begin(), latencies_.end());
        std::sort(sorted_latencies.begin(), sorted_latencies.end());

        size_t p50_idx = static_cast<size_t>(sorted_latencies.size() * 0.50);
        size_t p95_idx = static_cast<size_t>(sorted_latencies.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(sorted_latencies.size() * 0.99);

        float p50 = sorted_latencies[std::min(p50_idx, sorted_latencies.size() - 1)];
        float p95 = sorted_latencies[std::min(p95_idx, sorted_latencies.size() - 1)];
        float p99 = sorted_latencies[std::min(p99_idx, sorted_latencies.size() - 1)];

        // Calculate mean latency
        float mean_latency = std::accumulate(latencies_.begin(), latencies_.end(), 0.0f)
                           / latencies_.size();

        // Calculate mean confidence
        float mean_conf = std::accumulate(confidences_.begin(), confidences_.end(), 0.0f)
                        / confidences_.size();

        // Calculate fallback rate
        float fallback_rate = total_count_ > 0
            ? static_cast<float>(fallback_count_) / total_count_
            : 0.0f;

        return Statistics{
            .p50_latency_ms = p50,
            .p95_latency_ms = p95,
            .p99_latency_ms = p99,
            .mean_latency_ms = mean_latency,
            .mean_confidence = mean_conf,
            .fallback_rate = fallback_rate,
            .total_inferences = total_count_
        };
    }

    diagnostic_msgs::msg::DiagnosticStatus toDiagnosticStatus(
        const std::string& model_version,
        const std::string& execution_provider
    ) const {
        auto stats = getStatistics();

        diagnostic_msgs::msg::DiagnosticStatus status;
        status.name = "ONNX Inference";
        status.hardware_id = execution_provider;

        // Determine status level
        bool p95_alert = baseline_p95_latency_ms_ > 0 &&
                        stats.p95_latency_ms > baseline_p95_latency_ms_ *
                        config_.p95_alert_threshold_multiplier;

        bool fallback_alert = stats.fallback_rate > config_.fallback_rate_alert_threshold;

        if (p95_alert || fallback_alert) {
            status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
            if (p95_alert) {
                status.message = "P95 latency exceeded threshold";
            } else {
                status.message = "High fallback rate";
            }
        } else {
            status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
            status.message = "Inference performing normally";
        }

        // Add key-value pairs
        status.values.push_back(makeKeyValue("model_version", model_version));
        status.values.push_back(makeKeyValue("execution_provider", execution_provider));
        status.values.push_back(makeKeyValue("p50_latency_ms", stats.p50_latency_ms));
        status.values.push_back(makeKeyValue("p95_latency_ms", stats.p95_latency_ms));
        status.values.push_back(makeKeyValue("p99_latency_ms", stats.p99_latency_ms));
        status.values.push_back(makeKeyValue("mean_latency_ms", stats.mean_latency_ms));
        status.values.push_back(makeKeyValue("mean_confidence", stats.mean_confidence));
        status.values.push_back(makeKeyValue("fallback_rate", stats.fallback_rate));
        status.values.push_back(makeKeyValue("total_inferences",
                                            std::to_string(stats.total_inferences)));

        return status;
    }

    void setBaselineP95Latency(float baseline_ms) {
        baseline_p95_latency_ms_ = baseline_ms;
    }

private:
    static diagnostic_msgs::msg::KeyValue makeKeyValue(
        const std::string& key,
        const std::string& value
    ) {
        diagnostic_msgs::msg::KeyValue kv;
        kv.key = key;
        kv.value = value;
        return kv;
    }

    static diagnostic_msgs::msg::KeyValue makeKeyValue(
        const std::string& key,
        float value
    ) {
        return makeKeyValue(key, std::to_string(value));
    }

    Config config_;
    std::deque<float> latencies_;
    std::deque<float> confidences_;
    uint64_t fallback_count_ = 0;
    uint64_t total_count_ = 0;
    float baseline_p95_latency_ms_ = 0.0f;
};

} // namespace warehouser::inference
```

### 6. ROS2 Integration Pattern

**Pattern**: Complete ROS2 node with inference service, diagnostics publishing, and model hot-swap service

**C++ Implementation Template**:

```cpp
#pragma once

#include <rclcpp/rclcpp.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <std_srvs/srv/trigger.hpp>
#include "warehouser_msgs/srv/inference.hpp"
#include "warehouser_msgs/srv/load_model.hpp"
#include "hot_swap_model_manager.hpp"
#include "safe_inference_wrapper.hpp"
#include "performance_monitor.hpp"

namespace warehouser::inference {

class InferenceNode : public rclcpp::Node {
public:
    InferenceNode() : Node("inference_node") {
        // Declare parameters
        this->declare_parameter("model_path", "");
        this->declare_parameter("metadata_path", "");
        this->declare_parameter("execution_provider", "CPU");
        this->declare_parameter("intra_op_threads", 4);
        this->declare_parameter("inter_op_threads", 2);
        this->declare_parameter("timeout_ms", 50);
        this->declare_parameter("confidence_threshold", 0.7);
        this->declare_parameter("tensorrt_cache_path", "");
        this->declare_parameter("enable_fp16", false);

        // Load configuration
        SessionConfig config;
        config.execution_provider = this->get_parameter("execution_provider").as_string();
        config.intra_op_threads = this->get_parameter("intra_op_threads").as_int();
        config.inter_op_threads = this->get_parameter("inter_op_threads").as_int();
        config.tensorrt_cache_path = this->get_parameter("tensorrt_cache_path").as_string();
        config.enable_fp16 = this->get_parameter("enable_fp16").as_bool();

        // Create model manager
        model_manager_ = std::make_unique<HotSwapModelManager>(config);

        // Load initial model
        auto model_path = this->get_parameter("model_path").as_string();
        auto metadata_path = this->get_parameter("metadata_path").as_string();

        if (!model_path.empty() && !metadata_path.empty()) {
            auto result = model_manager_->loadInitialModel(model_path, metadata_path);
            if (!result) {
                RCLCPP_ERROR(this->get_logger(), "Failed to load initial model: %s",
                           result.error().c_str());
                throw std::runtime_error(result.error());
            }

            RCLCPP_INFO(this->get_logger(), "Loaded model version: %s",
                       model_manager_->getCurrentMetadata().version.c_str());
        }

        // Create services
        inference_service_ = this->create_service<warehouser_msgs::srv::Inference>(
            "inference",
            std::bind(&InferenceNode::handleInference, this,
                     std::placeholders::_1, std::placeholders::_2)
        );

        load_model_service_ = this->create_service<warehouser_msgs::srv::LoadModel>(
            "load_model",
            std::bind(&InferenceNode::handleLoadModel, this,
                     std::placeholders::_1, std::placeholders::_2)
        );

        // Create diagnostics publisher
        diagnostics_pub_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
            "/diagnostics", 10
        );

        // Create diagnostics timer (1 Hz)
        diagnostics_timer_ = this->create_wall_timer(
            std::chrono::seconds(1),
            std::bind(&InferenceNode::publishDiagnostics, this)
        );

        RCLCPP_INFO(this->get_logger(), "Inference node initialized");
    }

private:
    void handleInference(
        const std::shared_ptr<warehouser_msgs::srv::Inference::Request> request,
        std::shared_ptr<warehouser_msgs::srv::Inference::Response> response
    ) {
        auto result = model_manager_->infer(request->observation);

        if (!result) {
            response->success = false;
            response->message = result.error();

            // Record failed inference
            perf_monitor_.recordInference(0.0f, 0.0f, true);
            return;
        }

        response->success = true;
        response->action = result->output;
        response->confidence = result->confidence;
        response->latency_ms = result->latency.count() / 1000.0f;

        // Record inference
        perf_monitor_.recordInference(
            response->latency_ms,
            result->confidence,
            false
        );
    }

    void handleLoadModel(
        const std::shared_ptr<warehouser_msgs::srv::LoadModel::Request> request,
        std::shared_ptr<warehouser_msgs::srv::LoadModel::Response> response
    ) {
        RCLCPP_INFO(this->get_logger(), "Loading new model: %s",
                   request->model_path.c_str());

        auto result = model_manager_->hotSwapModel(
            request->model_path,
            request->metadata_path
        );

        if (!result) {
            response->success = false;
            response->message = result.error();
            RCLCPP_ERROR(this->get_logger(), "Failed to load model: %s",
                        result.error().c_str());
            return;
        }

        auto metadata = model_manager_->getCurrentMetadata();
        response->success = true;
        response->message = "Successfully loaded model version " + metadata.version;

        RCLCPP_INFO(this->get_logger(), "%s", response->message.c_str());
    }

    void publishDiagnostics() {
        auto metadata = model_manager_->getCurrentMetadata();

        auto status = perf_monitor_.toDiagnosticStatus(
            metadata.version,
            this->get_parameter("execution_provider").as_string()
        );

        diagnostic_msgs::msg::DiagnosticArray array;
        array.header.stamp = this->now();
        array.status.push_back(status);

        diagnostics_pub_->publish(array);
    }

    std::unique_ptr<HotSwapModelManager> model_manager_;
    PerformanceMonitor perf_monitor_;

    rclcpp::Service<warehouser_msgs::srv::Inference>::SharedPtr inference_service_;
    rclcpp::Service<warehouser_msgs::srv::LoadModel>::SharedPtr load_model_service_;
    rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_pub_;
    rclcpp::TimerBase::SharedPtr diagnostics_timer_;
};

} // namespace warehouser::inference
```

### 7. Model Metadata JSON Format

**Pattern**: Store validation test cases and performance expectations alongside model

**Example JSON** (`models/policy_v1.2.3_metadata.json`):

```json
{
  "version": "1.2.3",
  "training_timestamp": "2026-02-12T20:30:00Z",
  "expected_p95_latency_ms": 15.0,
  "model_architecture": "PPO",
  "input_shape": [1, 724],
  "output_shape": [1, 3],
  "validation_cases": [
    {
      "name": "zero_observation",
      "input": [0.0, 0.0, 0.0],
      "expected_output": [0.0, 0.0, 0.0],
      "tolerance": 0.01
    },
    {
      "name": "typical_observation",
      "input": [1.0, 0.5, -0.3],
      "expected_output": [0.2, 0.1, 0.0],
      "tolerance": 0.05
    }
  ],
  "deployment_notes": "First production version with obstacle avoidance"
}
```

### 8. CMakeLists.txt Integration

**Pattern**: Link ONNX Runtime and dependencies in ROS2 package

```cmake
cmake_minimum_required(VERSION 3.8)
project(ros_inference)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 23)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)

# ONNX Runtime from vcpkg
find_package(onnxruntime CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Inference library
add_library(${PROJECT_NAME}_lib
  src/inference_session.cpp
  src/model_validator.cpp
  src/safe_inference_wrapper.cpp
  src/hot_swap_model_manager.cpp
  src/performance_monitor.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}_lib
  onnxruntime::onnxruntime
  nlohmann_json::nlohmann_json
)

ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  diagnostic_msgs
  warehouser_msgs
)

# Inference node executable
add_executable(inference_node src/inference_node.cpp)
target_link_libraries(inference_node ${PROJECT_NAME}_lib)

install(TARGETS
  inference_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(${PROJECT_NAME}_test
    test/test_inference_session.cpp
    test/test_model_validator.cpp
    test/test_safe_inference.cpp
  )

  target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME}_lib)
endif()

ament_package()
```

### 9. Configuration File

**Pattern**: ROS2 parameter YAML for deployment configuration

**Example** (`config/inference.yaml`):

```yaml
inference_node:
  ros__parameters:
    # Model paths
    model_path: "models/policy_v1.2.3.onnx"
    metadata_path: "models/policy_v1.2.3_metadata.json"

    # Execution provider: CPU, CUDA, TensorRT
    execution_provider: "TensorRT"

    # Threading
    intra_op_threads: 4  # Parallelize within operators
    inter_op_threads: 2  # Parallelize across operators

    # TensorRT configuration
    tensorrt_cache_path: "/tmp/trt_cache"  # Cache engines for faster startup
    enable_fp16: true  # 2x speedup, <0.5% accuracy loss

    # Safety
    timeout_ms: 50  # Hard timeout for real-time control
    confidence_threshold: 0.7  # Fallback if confidence < threshold

    # Monitoring
    diagnostics_rate: 1.0  # Hz
```

## Application

### Implementation Roadmap for Warehouser ros_inference Package

#### Phase 1: Core Inference (Week 1)

1. **Create package structure**:
   ```
   ros_inference/
   ├── include/ros_inference/
   │   ├── inference_session.hpp
   │   ├── model_validator.hpp
   │   └── ...
   ├── src/
   │   ├── inference_session.cpp
   │   ├── inference_node.cpp
   │   └── ...
   ├── test/
   │   ├── test_inference_session.cpp
   │   └── ...
   ├── config/
   │   └── inference.yaml
   └── CMakeLists.txt
   ```

2. **Implement InferenceSession** using template above:
   - Support CPU, CUDA, TensorRT execution providers
   - Extract input/output metadata
   - Return structured results with latency

3. **Create test ONNX models**:
   ```python
   # Export dummy model for testing
   import torch
   import torch.onnx

   class DummyPolicy(torch.nn.Module):
       def __init__(self):
           super().__init__()
           self.fc = torch.nn.Linear(724, 3)

       def forward(self, x):
           return torch.tanh(self.fc(x))

   model = DummyPolicy()
   dummy_input = torch.randn(1, 724)
   torch.onnx.export(model, dummy_input, "test_policy.onnx")
   ```

4. **Unit tests**:
   - Test model loading with different execution providers
   - Test inference with various input sizes
   - Test error handling (missing model, invalid input)

#### Phase 2: Safety and Validation (Week 2)

1. **Implement ModelValidator**:
   - Copy validation template above
   - Add validation test cases to model metadata
   - Implement latency profiling

2. **Implement SafeInferenceWrapper**:
   - Timeout mechanism using std::async
   - Confidence thresholding
   - NaN/Inf detection
   - Statistics tracking

3. **Create fallback policy**:
   - Simple stop action: `[0.0, 0.0, 0.0]`
   - Or basic obstacle avoidance using lidar directly

4. **Integration tests**:
   - Test timeout behavior
   - Test fallback triggers
   - Test statistics accuracy

#### Phase 3: Hot-Swap and Monitoring (Week 3)

1. **Implement HotSwapModelManager**:
   - Shared mutex for thread-safe reads
   - Background loading and validation
   - Atomic swap

2. **Implement PerformanceMonitor**:
   - Sliding window statistics
   - ROS2 diagnostics integration
   - Alert thresholds

3. **Create ROS2 services**:
   ```idl
   # warehouser_msgs/srv/Inference.srv
   float32[] observation
   ---
   bool success
   string message
   float32[] action
   float32 confidence
   float32 latency_ms

   # warehouser_msgs/srv/LoadModel.srv
   string model_path
   string metadata_path
   ---
   bool success
   string message
   ```

4. **System tests**:
   - Test hot-swap during active inference
   - Test diagnostics publishing
   - Load testing (throughput, latency distribution)

#### Phase 4: Production Deployment (Week 4)

1. **Export trained policy from training package**:
   ```python
   # training/export_onnx.py
   import torch
   import json
   from datetime import datetime

   def export_policy(policy, save_path, version):
       # Export ONNX
       dummy_obs = torch.randn(1, 724)
       torch.onnx.export(
           policy,
           dummy_obs,
           f"{save_path}/policy_v{version}.onnx",
           input_names=['observation'],
           output_names=['action'],
           dynamic_axes={'observation': {0: 'batch_size'}}
       )

       # Create metadata
       metadata = {
           "version": version,
           "training_timestamp": datetime.utcnow().isoformat() + "Z",
           "expected_p95_latency_ms": 15.0,
           "validation_cases": [
               {
                   "name": "zero_obs",
                   "input": [0.0] * 724,
                   "expected_output": [0.0, 0.0, 0.0],
                   "tolerance": 0.01
               }
           ]
       }

       with open(f"{save_path}/policy_v{version}_metadata.json", "w") as f:
           json.dump(metadata, f, indent=2)
   ```

2. **Configure for target hardware**:
   - Jetson: TensorRT + FP16
   - Desktop: CUDA or CPU
   - Profile p95 latency and set baseline

3. **Create launch file**:
   ```python
   # ros_inference/launch/inference.launch.py
   from launch import LaunchDescription
   from launch_ros.actions import Node

   def generate_launch_description():
       return LaunchDescription([
           Node(
               package='ros_inference',
               executable='inference_node',
               name='inference_node',
               parameters=['config/inference.yaml'],
               output='screen'
           )
       ])
   ```

4. **Integration with rl_bridge**:
   - Call inference service from rl_bridge step callback
   - Log inference results
   - Handle fallback gracefully

#### Key Success Metrics

- **Latency**: p95 < 50ms on target hardware
- **Reliability**: Fallback rate < 1%
- **Startup time**: < 2 seconds with TensorRT cache
- **Hot-swap**: Zero inference failures during model swap
- **Test coverage**: >80% line coverage

#### Safety Checklist

- [ ] Timeout prevents stale perception data
- [ ] Fallback policy is provably safe (stops robot)
- [ ] NaN/Inf detection prevents invalid actions
- [ ] Model validation prevents deploying broken models
- [ ] Diagnostics alert on performance degradation
- [ ] All inference calls logged with model version
- [ ] Emergency stop on repeated failures

#### Edge Deployment Optimizations

For Jetson or other edge devices:

1. **Use TensorRT with FP16**:
   - 2x speedup with <0.5% accuracy loss
   - Test accuracy degradation before deploying

2. **Enable engine caching**:
   - Set `tensorrt_cache_path` in config
   - Reduces startup from 30s to <2s

3. **Profile on target hardware**:
   ```bash
   # Run latency benchmarks
   ros2 service call /inference warehouser_msgs/srv/Inference \
     "{observation: [0.0, 0.0, ...]}"

   # Check diagnostics
   ros2 topic echo /diagnostics
   ```

4. **Power management**:
   - Adjust inference frequency based on battery level
   - Use smaller model when power constrained

#### Monitoring Dashboard

Create simple CLI tool for monitoring:

```python
# tools/monitor_inference.py
import rclpy
from diagnostic_msgs.msg import DiagnosticArray

def main():
    rclpy.init()
    node = rclpy.create_node('inference_monitor')

    def callback(msg):
        for status in msg.status:
            if status.name == "ONNX Inference":
                print(f"\n{status.message}")
                for kv in status.values:
                    print(f"  {kv.key}: {kv.value}")

    sub = node.create_subscription(DiagnosticArray, '/diagnostics', callback, 10)
    rclpy.spin(node)
```

### Copy-Paste Ready Integration

For immediate use, copy these files to `ros_inference/`:

1. **Core classes**: `inference_session.hpp`, `model_validator.hpp`, `safe_inference_wrapper.hpp`
2. **Hot-swap**: `hot_swap_model_manager.hpp`
3. **Monitoring**: `performance_monitor.hpp`
4. **ROS2 node**: `inference_node.cpp`
5. **Config**: `config/inference.yaml`
6. **Build**: `CMakeLists.txt`

Customize execution provider, timeout, and confidence threshold based on hardware and task requirements.

### Testing Strategy

```cpp
// test/test_safe_inference.cpp
#include <gtest/gtest.h>
#include "ros_inference/safe_inference_wrapper.hpp"

TEST(SafeInference, TimeoutTriggersiFallback) {
    // Create session with very short timeout
    SafeInferenceWrapper::Config config;
    config.timeout = std::chrono::milliseconds(1);  // 1ms timeout

    // Mock session that takes >1ms
    auto slow_session = createSlowMockSession();
    SafeInferenceWrapper wrapper(std::move(slow_session), config);

    // Fallback should be triggered
    auto result = wrapper.inferWithFallback(
        {0.0f},
        []() { return std::vector<float>{999.0f}; }
    );

    EXPECT_EQ(result.fallback_reason, FallbackReason::Timeout);
    EXPECT_FLOAT_EQ(result.action[0], 999.0f);
}

TEST(SafeInference, LowConfidenceTriggersiFallback) {
    SafeInferenceWrapper::Config config;
    config.confidence_threshold = 0.7f;

    // Mock session that returns low confidence
    auto low_conf_session = createLowConfidenceMockSession(0.5f);
    SafeInferenceWrapper wrapper(std::move(low_conf_session), config);

    auto result = wrapper.inferWithFallback(
        {0.0f},
        []() { return std::vector<float>{888.0f}; }
    );

    EXPECT_EQ(result.fallback_reason, FallbackReason::LowConfidence);
    EXPECT_FLOAT_EQ(result.action[0], 888.0f);
}
```

## Sources

Research documented in `S.md`:
- ONNX Runtime official documentation
- NVIDIA TensorRT execution provider guide
- Safety-critical robotics patterns from academic research
- Edge deployment best practices
- Production monitoring patterns from industry sources

