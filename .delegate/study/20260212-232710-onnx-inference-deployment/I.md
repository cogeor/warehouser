# Introspect: ONNX Inference Deployment

Created: 2026-02-12T23:35:00Z

## Focus

Analysis of ONNX model deployment architecture for real-time robot inference in the Warehouser project, covering the complete pipeline from training export through C++ runtime execution.

## Current Architecture Overview

The system implements a two-stage deployment pipeline:
1. **Training/Export (Python)**: PPO model training via Stable-Baselines3, export to ONNX
2. **Inference (C++)**: ONNX Runtime-based policy execution in ROS2 node

### Pipeline Flow
```
Training (Python/SB3) -> Export (torch.onnx) -> ONNX File -> Inference (C++/ONNX Runtime) -> Robot Actions
```

## Detailed Findings

### 1. Model Export Pipeline

**File**: `C:\Users\costa\src\warehouser\training\training\scripts\export_onnx.py`

**Current Implementation**:
- Extracts actor network from trained PPO model via `policy.actor.get_action_dist_params(obs)[0]`
- Uses PyTorch's `torch.onnx.export()` with opset version 17 (configurable, minimum 9)
- Exports deterministic action mean (not stochastic sampling)
- Defines dynamic batch axis for flexibility
- Validates exported model with `onnx.checker.check_model()`

**Strengths**:
- Comprehensive error handling with informative messages (lines 27-46)
- File existence and size validation post-export (lines 126-138)
- ONNX validation ensures structural correctness (lines 141-160)
- Supports custom observation dimensions (default 8)
- Explicit opset version control

**Gaps**:
- `C:\Users\costa\src\warehouser\training\training\scripts\export_onnx.py:97` - Direct indexing `[0]` assumes specific SB3 internal structure, brittle to library updates
- No numerical validation (comparing PyTorch vs ONNX outputs on same inputs)
- No metadata embedding (training steps, performance metrics, version info)
- Opset 17 may not be compatible with all ONNX Runtime versions
- No export of normalization parameters (if observation preprocessing exists)

### 2. ONNX Runtime Configuration

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp`

**Session Configuration** (lines 25-28):
```cpp
impl_->session_options.SetIntraOpNumThreads(1);
impl_->session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
```

**Strengths**:
- Single-threaded execution ensures deterministic timing
- Maximum graph optimization enabled (ORT_ENABLE_ALL)
- Warning-level logging (ORT_LOGGING_LEVEL_WARNING) balances verbosity

**Gaps**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp:26` - Hardcoded to 1 thread, no runtime configurability
- No execution provider configuration (CPU only, no GPU/TensorRT/DirectML options)
- No session warmup (first inference will be slower)
- Missing optimization options:
  - `SetExecutionMode()` not specified (sequential vs parallel)
  - `SetInterOpNumThreads()` not configured
  - `EnableMemPattern()` not explicitly set
  - `EnableCpuMemArena()` not configured
- No logging level configurability (hardcoded WARNING)

### 3. Model Loading and Validation

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp`

**Implementation** (lines 36-90):
- File existence check before loading (lines 38-42)
- Input/output count validation (lines 52-66)
- Shape inference to determine obs_dim and action_dim (lines 57-72)
- Exception handling for ONNX Runtime errors (lines 79-81)

**Strengths**:
- Uses `std::expected` for error handling (modern C++23)
- Validates model structure matches expectations
- Extracts dimensions dynamically from model

**Gaps**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp:57-61` - Only checks input shape size, doesn't validate batch dimension is dynamic or first dimension equals 1
- No dtype validation (assumes float32, doesn't verify)
- No opset version compatibility check
- No model hash/checksum verification for integrity
- No version metadata extraction
- Missing validation:
  - Input/output name verification (assumes "observation"/"action")
  - Action dimension bounds checking (expects 4, doesn't validate)
  - Model file size sanity check (could catch truncated downloads)

### 4. Inference Execution

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp`

**Implementation** (lines 92-175):
- Observation size validation (lines 97-102)
- Tensor creation with CPU allocator (lines 106-119)
- Data copy required for ONNX Runtime (line 112: `std::vector<float> obs_copy = observation`)
- Action clamping for linear/angular (lines 131-132)

**Strengths**:
- Pre-inference dimension validation
- Action clamping provides safety bounds
- Exception handling with informative errors

**Gaps**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp:112` - Unnecessary vector copy, could use const_cast (documented issue with ONNX Runtime API)
- No input bounds checking (observation values could be NaN/Inf)
- No output validation (action values could be NaN/Inf from model)
- Action indices hardcoded (lines 131-134), assumes specific ordering
- Pick/place threshold hardcoded to 0.5 (line 133-134)
- No inference timeout mechanism
- Memory allocation happens every inference call (no tensor reuse)

### 5. Integration with ROS2

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\inference_node.cpp`

**Inference Loop** (lines 84-113):
- Timer-based execution at configurable rate (default 20 Hz)
- Enabled flag for manual control (lines 60-63)
- Observation caching (last received message)
- Publishes both Twist commands and full Action messages

**Configuration** (lines 13-16):
```cpp
v_max_ = declare_parameter("v_max", 1.0);
omega_max_ = declare_parameter("omega_max", 2.0);
default_model_path_ = declare_parameter("default_model_path", "");
auto inference_rate = declare_parameter("inference_rate", 20.0);
```

**Strengths**:
- Configurable velocity limits for safety
- Enable/disable mechanism for runtime control
- LoadModel service allows hot-swapping (lines 65-82)
- Throttled error logging (line 93)

**Gaps**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\inference_node.cpp:86-88` - No staleness check on observations (could use old data)
- No inference latency measurement or logging
- No timeout on inference execution
- No fallback behavior when inference fails
- No action rate limiting (sudden changes not smoothed)
- No diagnostics publishing (inference health, model info, etc.)
- LoadModel service doesn't return model metadata in response (line 74, message field unused)
- No validation that observation version matches model expectations

### 6. Build System and Dependencies

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\CMakeLists.txt`

**Dependency Management** (lines 17-38):
- Optional ONNX Runtime dependency (builds stub if not found)
- Manual path hints for library discovery
- Conditional compilation via `ONNXRUNTIME_AVAILABLE` macro

**Strengths**:
- Graceful degradation when ONNX Runtime not available
- Flexible library path configuration
- Stub implementation enables testing without ONNX Runtime

**Gaps**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\CMakeLists.txt:18-30` - Manual library discovery, no vcpkg integration
- No version checking for ONNX Runtime compatibility
- No GPU/CUDA support configuration
- Stub implementation (lines 82-89, 141-174 in policy_inference.cpp) provides reactive behavior but doesn't truly test inference pipeline
- Missing build-time model validation step

### 7. Safety Mechanisms

**Current Implementations**:
1. Action clamping (policy_inference.cpp:131-132)
2. Enable/disable flag (inference_node.cpp:86)
3. Error logging (inference_node.cpp:93-95)
4. Default parameters (inference_node.cpp:13-16)

**Missing Safety Features**:
- No watchdog timer for inference hangs
- No fallback policy when model fails
- No action sanity checks (e.g., simultaneous pick and place)
- No observation validity checks (NaN/Inf detection)
- No model output validation before execution
- No emergency stop mechanism
- No action history for debugging
- No safe mode (reduced velocity limits on errors)

### 8. Monitoring and Observability

**Current State**: Minimal

**Logging**:
- Model load success/failure (inference_node.cpp:47-50, 75-80)
- Inference errors throttled to 1/sec (inference_node.cpp:93)
- Enable/disable state changes (inference_node.cpp:62)

**Missing Monitoring**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\inference_node.cpp` - No latency measurement or publishing
- No inference frequency tracking
- No model performance metrics (success rate, average reward, etc.)
- No diagnostics topic for health monitoring
- No trace logging for debugging
- No memory usage tracking
- No action distribution statistics
- No observation statistics (detecting distribution shift)

### 9. Model Versioning and Metadata

**Current State**: None

**Gaps**:
- No version field in LoadModel service
- No metadata embedding in ONNX export
- No model registry or catalog
- No automatic model selection based on task
- No A/B testing capability
- No rollback mechanism
- No training metrics association with deployed model
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\srv\LoadModel.srv:10` - `model_info` field exists but unused

### 10. Testing

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\test\test_policy_inference.cpp`

**Coverage**:
- Initial state validation (line 11-13)
- Inference without model failure (lines 15-20)
- Nonexistent model load failure (lines 22-25)
- Stub behavior tests (lines 27-96)

**Strengths**:
- Tests both ONNX Runtime and stub paths
- Validates stub reactive behavior

**Gaps**:
- No real ONNX model loading tests (only stub)
- No dimension mismatch tests
- No invalid observation tests (NaN, Inf)
- No concurrent inference tests
- No model hot-reload tests
- No performance benchmarks
- No memory leak detection
- No integration tests with actual trained models

## Architecture Comparison: Actual vs. Planned

### Discrepancies from README Specification

**`.arch/ros_inference/README.md` vs. Actual Implementation**:

1. **Observation Dimension**:
   - README specifies 67 dims (60 lidar + 7 other)
   - Actual default is 8 dims (position-based)
   - Code supports configurable dimensions, but mismatch in documentation

2. **Missing Topics/Services**:
   - README lists `/world/robot_state`, `/scan`, `/task/goal_pose` subscriptions
   - Actual: only `/observations` subscription (warehouser_msgs/Observation)
   - README lists `/inference/status` and `/inference/get_info`
   - Actual: these don't exist in implementation

3. **Observation Builder**:
   - README shows detailed `buildObservation()` function
   - Actual: observation building happens in separate `ros_observations` package
   - Inference node only consumes pre-built observations

4. **Performance Targets**:
   - README specifies < 5ms latency target
   - Actual: no latency measurement implemented to verify

5. **Model Metadata**:
   - README shows JSON metadata specification
   - Actual: no metadata handling in export or load

### Training Configuration Observations

**File**: `C:\Users\costa\src\warehouser\training\training\models\config.py`

**Strengths**:
- Comprehensive Pydantic validation (lines 1-297)
- REP-103 compliance checks for theta angles
- Detailed error messages for validation failures
- Reasonable default hyperparameters

**Observations**:
- `obs_dim` default is 8 (line 120), matches inference default
- Multi-agent support exists but inference node is single-robot only
- Reward configuration highly detailed but not used in inference

## Recommendations by Priority

### Critical (Safety & Correctness)

1. **Add observation staleness detection**
   - Location: `inference_node.cpp:86`
   - Check timestamp on last_observation_, reject if > 100ms old

2. **Implement output validation**
   - Location: `policy_inference.cpp:128-136`
   - Check for NaN/Inf in action outputs before returning

3. **Add inference timeout**
   - Location: `inference_node.cpp:91`
   - Abort inference if exceeds 50ms, use fallback

4. **Validate observation inputs**
   - Location: `policy_inference.cpp:92`
   - Check for NaN/Inf in observation vector

### High (Robustness & Operations)

5. **Implement latency measurement**
   - Location: `inference_node.cpp:84-113`
   - Track and publish inference timing on diagnostics topic

6. **Add numerical export validation**
   - Location: `export_onnx.py:162`
   - Compare PyTorch vs ONNX outputs on test inputs

7. **Embed model metadata in ONNX**
   - Location: `export_onnx.py:106-118`
   - Add training metrics, version, timestamp as model metadata

8. **Implement fallback policy**
   - Location: `inference_node.cpp:92-96`
   - Use simple obstacle avoidance when inference fails

### Medium (Performance & Maintainability)

9. **Optimize tensor allocation**
   - Location: `policy_inference.cpp:106-119`
   - Reuse input/output tensors instead of allocating each call

10. **Add execution provider configuration**
    - Location: `policy_inference.cpp:25-28`
    - Support CUDA/TensorRT via parameters

11. **Implement model registry**
    - New component
    - Track deployed models with version, performance, rollback

12. **Add diagnostic publishing**
    - Location: `inference_node.cpp`
    - Publish health status, latency, model info at 1 Hz

### Low (Nice to Have)

13. **Session warmup**
    - Location: `policy_inference.cpp:36-90`
    - Run dummy inference after model load to prime caches

14. **Action smoothing**
    - Location: `inference_node.cpp:100-104`
    - Low-pass filter to prevent jerky movements

15. **A/B testing framework**
    - New component
    - Load multiple models, select based on configuration

16. **Integration tests with real models**
    - Location: `test/`
    - Export small test model, verify loading and inference

## Security Considerations

**Current Gaps**:
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_inference\src\policy_inference.cpp:38` - No path traversal protection in model loading
- No model signature verification (could load malicious ONNX)
- No sandbox for ONNX Runtime execution
- LoadModel service has no authentication/authorization
- No rate limiting on LoadModel service (DoS vector)

## Performance Baseline (Estimated)

Based on configuration and implementation:

| Metric | Current | Target (README) | Gap |
|--------|---------|-----------------|-----|
| Inference Rate | 20 Hz | 20 Hz | ✓ |
| Latency | Unknown | < 5ms | Need measurement |
| Memory | Unknown | < 100MB | Need profiling |
| Startup | Unknown | < 2s | Need measurement |
| Model Size | N/A | < 10MB | Depends on architecture |

## Conclusion

The current ONNX inference implementation provides a **functional baseline** but lacks **production-grade robustness**:

**Strengths**:
- Clean separation of training and inference
- Modern C++ error handling (std::expected)
- Hot-reload capability
- Configurable parameters
- Comprehensive export validation

**Critical Gaps**:
- No runtime monitoring (latency, health, diagnostics)
- Minimal safety mechanisms (no fallback, timeout, or staleness checks)
- No metadata or versioning system
- Limited performance optimization
- Discrepancies between planned architecture (README) and actual implementation

**Deployment Readiness**: 40%
- Works for controlled testing
- **NOT ready for production** without addressing critical safety and monitoring gaps
- Requires significant hardening for real robot deployment

**Next Steps** (in order):
1. Implement safety mechanisms (staleness, validation, timeout, fallback)
2. Add comprehensive monitoring and diagnostics
3. Align implementation with architectural specification
4. Performance profiling and optimization
5. Model metadata and versioning system
6. Security hardening
