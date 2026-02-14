# TASK: Harden ONNX Inference System for Production Deployment

Created: 2026-02-12T23:45:00Z
Build: ENVIRONMENT_ISSUE (colcon not found, pydantic missing)
Tests: FAILED (dependency issues)

## Summary

Transform the current functional baseline ONNX inference implementation into a production-grade system with comprehensive safety mechanisms, performance monitoring, model versioning, and hot-swap capabilities. The existing implementation provides basic ONNX model loading and inference but lacks critical safety features (timeout, fallback, staleness detection), monitoring (latency tracking, diagnostics), and deployment robustness (versioning, validation, optimization) required for real-world robot deployment.

## Context

### Sources

**[S] Search Findings - Production Deployment Patterns**:
- ONNX Runtime optimization techniques reduce startup latency by 30-40% through offline graph optimization
- TensorRT engine caching prevents rebuild overhead on node restarts (critical for ROS2)
- For safety-critical robotics, inference delays >3 seconds make perception stale; target <100ms for perception-action loops
- Safety filter framework: ML policy -> safety filter -> fallback policy provides formal guarantees
- Hot-swap pattern: load new model in background, validate, then atomically swap without stopping service
- Essential monitoring metrics: p50/p95/p99 latency (not just average), confidence distribution, fallback rate
- Model validation with test cases prevents deploying corrupted or incompatible models

**[I] Introspection Findings - Current Implementation Gaps**:
- Basic ONNX Runtime session created but hardcoded to CPU with 1 thread (no GPU/TensorRT support)
- No inference timeout mechanism (could hang indefinitely)
- No observation staleness detection (could act on old sensor data)
- No output validation (NaN/Inf checks missing)
- No fallback policy when inference fails
- No latency measurement or diagnostics publishing
- No model metadata or versioning system
- LoadModel service exists but doesn't return metadata
- Deployment readiness: 40% - works for testing but NOT production-ready

**[T] Template Findings - Reference Implementations**:
- Complete C++ templates for InferenceSession with CPU/CUDA/TensorRT execution providers
- ModelValidator class with pre-deployment test cases and latency profiling
- SafeInferenceWrapper with timeout, confidence thresholding, NaN detection, statistics tracking
- HotSwapModelManager with background loading, validation, atomic swap using shared_mutex
- PerformanceMonitor with sliding window statistics and ROS2 diagnostics integration
- Model metadata JSON format with validation test cases and performance expectations

## Objective

Create a production-ready ONNX inference system that:
1. Prevents unsafe robot behavior through timeout, validation, and fallback mechanisms
2. Provides comprehensive monitoring and diagnostics for observability
3. Supports model versioning and hot-swap without service interruption
4. Optimizes inference performance for real-time control (<50ms p95 latency)
5. Enables safe deployment to edge devices (Jetson with TensorRT)

## Implementation Plan

### Phase 1: Safety Mechanisms [CRITICAL]

**Objective**: Prevent unsafe robot behavior through defensive programming

- [ ] Add observation staleness detection in inference_node.cpp
  - Check timestamp on last_observation_, reject if >100ms old
  - Publish warning on /diagnostics when stale data detected
  - Return safe stop action [0.0, 0.0, 0.0]

- [ ] Implement input validation in policy_inference.cpp
  - Check for NaN/Inf in observation vector before inference
  - Return error via std::expected if invalid input detected
  - Log validation failures

- [ ] Implement output validation in policy_inference.cpp
  - Check for NaN/Inf in action outputs after inference
  - Clamp to safety bounds even if model produces extreme values
  - Track and log validation failures

- [ ] Add inference timeout mechanism
  - Use std::async with timeout in SafeInferenceWrapper class
  - Default timeout: 50ms (configurable via parameter)
  - Trigger fallback policy on timeout
  - Log timeout events with observation context

- [ ] Implement fallback policy
  - Simple stop action: [0.0, 0.0, 0.0] for v_linear, omega, pick, place
  - Alternative: reactive obstacle avoidance using lidar directly
  - Configurable via enable_fallback parameter

### Phase 2: Performance Monitoring [HIGH PRIORITY]

**Objective**: Provide visibility into inference health and performance

- [ ] Create PerformanceMonitor class
  - Track latency in sliding window (default 1000 samples)
  - Calculate p50, p95, p99 percentiles (not just average)
  - Track confidence distribution
  - Count fallback triggers by reason (timeout, low confidence, invalid output)
  - Provide getStatistics() API

- [ ] Add latency measurement to inference calls
  - Use std::chrono::high_resolution_clock before/after inference
  - Record in microseconds, report in milliseconds
  - Include in InferenceResult struct

- [ ] Create ROS2 diagnostics publisher
  - Publish diagnostic_msgs/DiagnosticArray on /diagnostics topic
  - Rate: 1 Hz
  - Include: model version, execution provider, p50/p95/p99 latency, mean confidence, fallback rate
  - Set status level to WARN if p95 > 2x baseline or fallback rate > 1%

- [ ] Add inference statistics to LoadModel service response
  - Return current p95 latency, total inference count
  - Populate model_info field in LoadModel.srv response

### Phase 3: Model Versioning and Validation [HIGH PRIORITY]

**Objective**: Ensure only validated models are deployed

- [ ] Define ModelMetadata struct
  - Fields: version (string), training_timestamp, expected_p95_latency_ms
  - validation_cases (vector of test inputs/outputs with tolerance)
  - deployment_notes

- [ ] Create ModelValidator class
  - validate() method: run test cases, check output values within tolerance
  - Check for NaN/Inf in outputs
  - validateLatency() method: run warmup + benchmark iterations, return p95
  - Return ValidationResult with passed/failed and error details

- [ ] Implement metadata JSON loading
  - Use nlohmann/json library (via vcpkg)
  - Load from {model_path}_metadata.json
  - Parse validation_cases array
  - Handle missing/malformed JSON gracefully

- [ ] Update export_onnx.py to generate metadata
  - Embed version, training timestamp, expected latency
  - Create 2-3 validation test cases (zero observation, typical observation)
  - Compare PyTorch vs ONNX outputs on test inputs (numerical validation)
  - Save metadata JSON alongside ONNX file

- [ ] Validate model before accepting in LoadModel service
  - Run ModelValidator.validate() before swapping
  - Reject model if validation fails
  - Log validation errors with details

### Phase 4: Hot-Swap and Configuration [MEDIUM PRIORITY]

**Objective**: Enable zero-downtime model updates

- [ ] Create HotSwapModelManager class
  - Use std::shared_mutex for reader-writer lock
  - infer() takes shared_lock (multiple readers)
  - loadModel() takes unique_lock only for atomic swap (single writer)
  - Load and validate new model WITHOUT holding lock
  - Swap only if validation passes

- [ ] Refactor inference_node.cpp to use HotSwapModelManager
  - Replace direct PolicyInference with HotSwapModelManager
  - LoadModel service calls hotSwapModel()
  - Inference continues uninterrupted during model load

- [ ] Add execution provider configuration
  - ROS2 parameter: execution_provider (CPU, CUDA, TensorRT)
  - intra_op_threads, inter_op_threads parameters
  - tensorrt_cache_path parameter (for engine caching)
  - enable_fp16 parameter (2x speedup on NVIDIA GPUs)

- [ ] Create SessionConfig struct
  - Aggregate all ONNX Runtime session options
  - Pass to InferenceSession::create() factory method
  - Load from ROS2 parameters

### Phase 5: Optimization [MEDIUM PRIORITY]

**Objective**: Achieve <50ms p95 latency on target hardware

- [ ] Implement TensorRT execution provider support
  - Add TensorRT provider options in InferenceSession
  - Configure engine caching to avoid rebuild on restart
  - Enable FP16 mode for 2x speedup with <0.5% accuracy loss
  - Fallback to CUDA, then CPU if TensorRT unavailable

- [ ] Add session warmup
  - Run 10 dummy inferences after model load
  - Primes caches and avoids first-inference latency spike
  - Log warmup completion

- [ ] Optimize tensor allocation
  - Reuse input/output tensors instead of allocating each call
  - Store as class members in PolicyInference
  - Only reallocate if observation dimension changes

- [ ] Add offline graph optimization
  - Use GraphOptimizationLevel::ORT_ENABLE_ALL
  - Consider exporting pre-optimized model from training
  - Document optimization level choice

### Phase 6: Testing and Documentation [LOW PRIORITY]

**Objective**: Ensure correctness and maintainability

- [ ] Create unit tests for new classes
  - test_model_validator.cpp: validation logic, latency profiling
  - test_safe_inference.cpp: timeout, confidence thresholds, fallback triggers
  - test_hot_swap_manager.cpp: concurrent access, atomic swap
  - test_performance_monitor.cpp: statistics calculation, percentiles

- [ ] Create integration tests with real models
  - Export small test ONNX model from training
  - Test full load -> validate -> infer pipeline
  - Test hot-swap during active inference
  - Test timeout with artificially slow model

- [ ] Add configuration example
  - Create config/inference.yaml with documented parameters
  - Show CPU, CUDA, TensorRT configurations
  - Document performance trade-offs

- [ ] Update README
  - Document safety mechanisms
  - Explain monitoring and diagnostics
  - Provide hot-swap usage examples
  - Add troubleshooting section

## Interface Definitions

### Core Classes

```cpp
namespace warehouser::inference {

// Session configuration
struct SessionConfig {
    std::string model_path;
    std::string execution_provider = "CPU";  // CPU, CUDA, TensorRT
    int32_t intra_op_threads = 4;
    int32_t inter_op_threads = 2;
    GraphOptimizationLevel optimization_level = GraphOptimizationLevel::ORT_ENABLE_ALL;
    std::string tensorrt_cache_path = "";
    bool enable_fp16 = false;
};

// Inference session with execution provider support
class InferenceSession {
public:
    struct InferenceResult {
        std::vector<float> output;
        float confidence;
        std::chrono::microseconds latency;
    };

    static std::expected<std::unique_ptr<InferenceSession>, std::string>
    create(const SessionConfig& config);

    std::expected<InferenceResult, std::string>
    infer(const std::vector<float>& input_data);
};

// Model validation with test cases
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
    );

    static std::expected<float, std::string>
    validateLatency(
        InferenceSession& session,
        const std::vector<float>& test_input,
        int32_t num_warmup = 10,
        int32_t num_iterations = 100
    );
};

// Safety wrapper with timeout and fallback
class SafeInferenceWrapper {
public:
    struct Config {
        std::chrono::milliseconds timeout{50};
        float confidence_threshold = 0.7f;
        bool enable_fallback = true;
    };

    enum class FallbackReason {
        None, Timeout, LowConfidence, InferenceError, OutputInvalid
    };

    struct SafeInferenceResult {
        std::vector<float> action;
        float confidence;
        FallbackReason fallback_reason = FallbackReason::None;
        std::chrono::microseconds latency;
    };

    SafeInferenceWrapper(std::unique_ptr<InferenceSession> session, Config config);

    SafeInferenceResult inferWithFallback(
        const std::vector<float>& observation,
        const std::function<std::vector<float>()>& fallback_policy
    );

    struct Statistics {
        uint64_t successful_count;
        uint64_t timeout_count;
        uint64_t error_count;
        uint64_t low_confidence_count;
        uint64_t invalid_output_count;
        float fallback_rate() const;
    };

    Statistics getStatistics() const;
    void resetStatistics();
};

// Model metadata
struct ModelMetadata {
    std::string version;
    std::string training_timestamp;
    float expected_p95_latency_ms;
    std::vector<ValidationTestCase> validation_cases;

    static std::expected<ModelMetadata, std::string>
    loadFromJson(const std::filesystem::path& json_path);
};

// Hot-swap model manager
class HotSwapModelManager {
public:
    HotSwapModelManager(const SessionConfig& base_config);

    std::expected<void, std::string>
    loadInitialModel(
        const std::filesystem::path& model_path,
        const std::filesystem::path& metadata_path
    );

    std::expected<void, std::string>
    hotSwapModel(
        const std::filesystem::path& new_model_path,
        const std::filesystem::path& new_metadata_path
    );

    std::expected<InferenceSession::InferenceResult, std::string>
    infer(const std::vector<float>& input);

    ModelMetadata getCurrentMetadata() const;
};

// Performance monitoring
class PerformanceMonitor {
public:
    struct Config {
        size_t window_size = 1000;
        float p95_alert_threshold_multiplier = 2.0f;
        float fallback_rate_alert_threshold = 0.01f;
    };

    PerformanceMonitor(Config config = {});

    void recordInference(float latency_ms, float confidence, bool fallback_used);

    struct Statistics {
        float p50_latency_ms;
        float p95_latency_ms;
        float p99_latency_ms;
        float mean_latency_ms;
        float mean_confidence;
        float fallback_rate;
        uint64_t total_inferences;
    };

    Statistics getStatistics() const;

    diagnostic_msgs::msg::DiagnosticStatus toDiagnosticStatus(
        const std::string& model_version,
        const std::string& execution_provider
    ) const;

    void setBaselineP95Latency(float baseline_ms);
};

} // namespace warehouser::inference
```

### ROS2 Service Updates

```
# warehouser_msgs/srv/Inference.srv (NEW)
float32[] observation
---
bool success
string message
float32[] action
float32 confidence
float32 latency_ms

# warehouser_msgs/srv/LoadModel.srv (MODIFY)
string model_path
string metadata_path  # NEW: path to metadata JSON
---
bool success
string message
string model_version  # NEW: loaded model version
float32 p95_latency_ms  # NEW: current p95 latency
```

### Configuration Schema

```yaml
# config/inference.yaml
inference_node:
  ros__parameters:
    # Model paths
    model_path: "models/policy_v1.2.3.onnx"
    metadata_path: "models/policy_v1.2.3_metadata.json"

    # Execution provider: CPU, CUDA, TensorRT
    execution_provider: "CPU"  # Use "TensorRT" on Jetson

    # Threading
    intra_op_threads: 4
    inter_op_threads: 2

    # TensorRT configuration
    tensorrt_cache_path: "/tmp/trt_cache"
    enable_fp16: false  # Set true on NVIDIA GPUs

    # Safety
    timeout_ms: 50
    confidence_threshold: 0.7
    enable_fallback: true

    # Monitoring
    diagnostics_rate: 1.0  # Hz
    window_size: 1000  # samples for statistics
```

### Model Metadata Format

```json
{
  "version": "1.2.3",
  "training_timestamp": "2026-02-12T20:30:00Z",
  "expected_p95_latency_ms": 15.0,
  "model_architecture": "PPO",
  "input_shape": [1, 8],
  "output_shape": [1, 4],
  "validation_cases": [
    {
      "name": "zero_observation",
      "input": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
      "expected_output": [0.0, 0.0, 0.0, 0.0],
      "tolerance": 0.01
    },
    {
      "name": "typical_observation",
      "input": [1.0, 0.5, -0.3, 0.2, 0.0, 0.0, 1.0, 0.0],
      "expected_output": [0.2, 0.1, 0.0, 0.0],
      "tolerance": 0.05
    }
  ],
  "deployment_notes": "First production version with obstacle avoidance"
}
```

## Files to Create

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_inference/include/ros_inference/inference_session.hpp` | ONNX Runtime session wrapper with execution provider support |
| `ros_ws/src/warehouser_inference/include/ros_inference/model_validator.hpp` | Pre-deployment validation with test cases and latency profiling |
| `ros_ws/src/warehouser_inference/include/ros_inference/safe_inference_wrapper.hpp` | Timeout, fallback, validation wrapper |
| `ros_ws/src/warehouser_inference/include/ros_inference/hot_swap_model_manager.hpp` | Thread-safe model hot-swap with background loading |
| `ros_ws/src/warehouser_inference/include/ros_inference/performance_monitor.hpp` | Sliding window statistics and diagnostics integration |
| `ros_ws/src/warehouser_inference/src/inference_session.cpp` | Implementation of InferenceSession |
| `ros_ws/src/warehouser_inference/src/model_validator.cpp` | Implementation of ModelValidator |
| `ros_ws/src/warehouser_inference/src/safe_inference_wrapper.cpp` | Implementation of SafeInferenceWrapper |
| `ros_ws/src/warehouser_inference/src/hot_swap_model_manager.cpp` | Implementation of HotSwapModelManager |
| `ros_ws/src/warehouser_inference/src/performance_monitor.cpp` | Implementation of PerformanceMonitor |
| `ros_ws/src/warehouser_inference/config/inference.yaml` | Example configuration for CPU, CUDA, TensorRT |
| `ros_ws/src/warehouser_inference/test/test_model_validator.cpp` | Unit tests for validation logic |
| `ros_ws/src/warehouser_inference/test/test_safe_inference.cpp` | Unit tests for timeout and fallback |
| `ros_ws/src/warehouser_inference/test/test_hot_swap_manager.cpp` | Unit tests for concurrent access and atomic swap |
| `ros_ws/src/warehouser_inference/test/test_performance_monitor.cpp` | Unit tests for statistics calculation |
| `ros_ws/src/warehouser_msgs/srv/Inference.srv` | New service definition for inference calls |

## Files to Modify

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_inference/src/inference_node.cpp` | Replace PolicyInference with HotSwapModelManager, add diagnostics publisher, add staleness checks, add latency measurement |
| `ros_ws/src/warehouser_inference/src/policy_inference.cpp` | Add input/output NaN/Inf validation, return latency in InferenceResult, optimize tensor allocation |
| `ros_ws/src/warehouser_inference/include/ros_inference/policy_inference.hpp` | Update InferenceResult to include latency field |
| `ros_ws/src/warehouser_inference/CMakeLists.txt` | Add nlohmann_json dependency, link new source files, add new test targets |
| `ros_ws/src/warehouser_msgs/srv/LoadModel.srv` | Add metadata_path, model_version, p95_latency_ms fields |
| `training/training/scripts/export_onnx.py` | Generate metadata JSON with validation cases, add numerical validation (PyTorch vs ONNX comparison) |
| `.arch/ros_inference/README.md` | Update with safety mechanisms, monitoring, hot-swap usage, configuration examples |

## Architecture Notes

### Modularity

The design follows separation of concerns:
- **InferenceSession**: Pure ONNX Runtime wrapper, no ROS dependencies
- **ModelValidator**: Standalone validation logic, reusable for offline testing
- **SafeInferenceWrapper**: Safety layer, can wrap any inference implementation
- **HotSwapModelManager**: Thread-safe orchestration, owns session lifecycle
- **PerformanceMonitor**: Pure statistics, minimal ROS coupling (only diagnostics message)
- **InferenceNode**: ROS2 glue code, orchestrates above components

This allows:
- Unit testing without ROS
- Reusing components in non-ROS contexts
- Easy mocking for integration tests

### Thread Safety

- **InferenceNode**: Single-threaded (ROS2 executor), no mutex needed for node logic
- **HotSwapModelManager**: Uses std::shared_mutex
  - Multiple readers (infer calls) can proceed concurrently
  - Single writer (model load) blocks all access during swap
  - Critical section minimized: only lock during pointer swap, not during model load/validation
- **PerformanceMonitor**: Uses std::atomic for counters, std::deque protected by implicit single-writer (node thread)

### Error Handling

- Use std::expected<T, std::string> for all fallible operations (C++23)
- Never throw exceptions across ROS service boundaries
- Always provide descriptive error messages with context
- Log errors at appropriate levels (ERROR for failures, WARN for fallbacks, INFO for state changes)

### Performance Considerations

- **Avoid allocations in hot path**: Reuse tensors, preallocate buffers
- **Minimize lock contention**: shared_mutex allows concurrent reads, short critical sections
- **Warmup on startup**: Prime caches before serving production traffic
- **Sliding window statistics**: Fixed-size deque prevents unbounded memory growth
- **TensorRT caching**: Avoid rebuilding engines on every node restart (saves 20-30s)

### Safety Design

Layered defense:
1. **Input validation**: Reject NaN/Inf, check staleness before inference
2. **Timeout**: Hard deadline prevents using stale perception data
3. **Output validation**: Detect NaN/Inf from model before sending to robot
4. **Confidence gating**: Fallback if model is uncertain
5. **Fallback policy**: Provably safe action (stop or simple reactive behavior)
6. **Monitoring**: Alert on degraded performance before catastrophic failure

### Deployment Strategy

1. **Development**: CPU execution provider, no timeout (for debugging)
2. **Integration Testing**: CPU with timeout, fallback enabled, diagnostics monitored
3. **Edge Device (Jetson)**: TensorRT + FP16, engine caching, strict timeout
4. **Production**: Hot-swap enabled, baseline p95 latency set, alerting configured

### Future Extensions

- **A/B testing**: Load two models, route % of traffic to each, compare metrics
- **Shadow deployment**: Run new model alongside production, log predictions without acting
- **Adaptive timeout**: Adjust based on observed p99 latency
- **Power-aware inference**: Reduce frequency or switch to smaller model on low battery
- **Drift detection**: Compare observation distribution to training data
- **Automatic retraining trigger**: Alert when performance degrades beyond threshold

## Verification

### Phase 1 - Safety

- [ ] Unit test: Observation with NaN triggers validation error
- [ ] Unit test: Observation with Inf triggers validation error
- [ ] Unit test: Action output with NaN triggers fallback
- [ ] Unit test: Inference timeout triggers fallback and logs timeout reason
- [ ] Integration test: Stale observation (old timestamp) rejected, robot stops
- [ ] Integration test: Repeated inference failures trigger emergency stop

### Phase 2 - Monitoring

- [ ] Unit test: PerformanceMonitor calculates correct p50/p95/p99 from sample data
- [ ] Unit test: Fallback rate calculation is accurate
- [ ] Integration test: Diagnostics published at 1 Hz with correct values
- [ ] Integration test: Diagnostic status is WARN when p95 > 2x baseline
- [ ] Integration test: Diagnostic status is WARN when fallback rate > 1%

### Phase 3 - Versioning

- [ ] Unit test: ModelValidator rejects model with incorrect output shape
- [ ] Unit test: ModelValidator rejects model with output values outside tolerance
- [ ] Unit test: ModelValidator measures latency correctly
- [ ] Integration test: LoadModel service rejects model with failed validation
- [ ] Integration test: Metadata JSON parsing handles missing fields gracefully
- [ ] Integration test: export_onnx.py produces valid metadata JSON

### Phase 4 - Hot-Swap

- [ ] Unit test: HotSwapModelManager atomic swap completes without inference errors
- [ ] Unit test: Concurrent infer calls during hotSwapModel don't crash
- [ ] Integration test: LoadModel service succeeds while inference service is being called
- [ ] Integration test: Inference continues using old model if new model validation fails
- [ ] Load test: 100 concurrent infer calls during hot-swap complete successfully

### Phase 5 - Optimization

- [ ] Benchmark: Measure p95 latency with CPU, CUDA, TensorRT on target hardware
- [ ] Benchmark: Verify TensorRT engine caching reduces startup time
- [ ] Benchmark: Verify FP16 mode achieves ~2x speedup with <0.5% accuracy delta
- [ ] Benchmark: Verify session warmup eliminates first-inference latency spike
- [ ] Profile: Confirm no unnecessary tensor allocations in steady-state inference

### Phase 6 - End-to-End

- [ ] Deploy to Jetson with TensorRT + FP16, verify p95 latency < 50ms
- [ ] Run 24-hour stress test, monitor fallback rate and diagnostics
- [ ] Trigger hot-swap 10 times during active inference, verify zero failures
- [ ] Simulate sensor failure (stop publishing observations), verify robot stops safely
- [ ] Simulate model failure (deploy broken model), verify validation rejects it

## Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| p95 inference latency | < 50ms on Jetson with TensorRT | ROS2 diagnostics topic |
| Fallback rate | < 1% in normal operation | Statistics published via diagnostics |
| Hot-swap success rate | 100% with valid models | LoadModel service response |
| Startup time with TensorRT cache | < 2 seconds | Time from node start to first inference |
| Test coverage | > 80% line coverage | gcov/lcov report |
| Model validation rejection rate | 0% for valid models, 100% for corrupted models | Unit test suite |
| Zero-downtime deployment | 0 inference failures during hot-swap | Integration test |

## Dependencies

**Required Libraries** (via vcpkg):
- `onnxruntime` (already present)
- `nlohmann-json` (for metadata parsing)

**ROS2 Packages**:
- `rclcpp` (already present)
- `diagnostic_msgs` (for diagnostics publishing)
- `warehouser_msgs` (modify LoadModel.srv, add Inference.srv)

**Build System**:
- Verify ONNX Runtime is findable via CMake
- Link nlohmann_json via vcpkg

## Rollout Plan

### Week 1: Safety and Validation
- Implement SafeInferenceWrapper
- Implement ModelValidator
- Add input/output NaN/Inf checks
- Create unit tests
- Update export_onnx.py to generate metadata

### Week 2: Monitoring and Hot-Swap
- Implement PerformanceMonitor
- Implement HotSwapModelManager
- Integrate diagnostics publishing
- Refactor inference_node.cpp
- Create integration tests

### Week 3: Optimization and Configuration
- Add TensorRT execution provider support
- Implement session warmup
- Create configuration examples
- Profile on target hardware
- Document performance trade-offs

### Week 4: Testing and Documentation
- Complete test coverage
- Run load tests and stress tests
- Update README and architecture docs
- Create troubleshooting guide
- Final review and deployment

## Notes

**Critical Path**: Phase 1 (Safety) and Phase 2 (Monitoring) are blockers for production deployment. These MUST be completed before deploying to physical robots.

**Performance Baseline**: Current implementation has no latency measurement, so we don't know actual performance. First step is adding measurement infrastructure, then optimizing based on data.

**Edge Deployment**: TensorRT support is essential for Jetson deployment. Without it, CPU inference may not meet <50ms latency target.

**Model Versioning**: Metadata JSON format must be finalized early and remain stable. Breaking changes require migrating all existing models.

**Testing Strategy**: Unit tests can use small dummy ONNX models. Integration tests should use actual trained policies from training package for realism.
