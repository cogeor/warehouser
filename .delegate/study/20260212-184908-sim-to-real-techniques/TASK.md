# TASK: Implement Comprehensive Domain Randomization for Sim-to-Real Transfer

Created: 2026-02-12 18:49:08
Build: DEGRADED (Python deps missing, C++ not verified)
Tests: FAIL (missing pydantic dependency)

## Summary

Enhance Warehouser's sim-to-real transfer capabilities by implementing physics randomization, action delay modeling, temporal observation history, and configuration infrastructure. Current implementation has basic sensor noise (lidar/odometry) but lacks critical dynamics modeling, action delays, and automated domain randomization needed for robust real-world deployment.

## Context

### [S] Research Findings: State-of-the-Art 2025-2026

The field has evolved beyond uniform domain randomization toward sophisticated integrated pipelines. Key findings:

**Domain Randomization Best Practices:**
- System identification (SysID) for measurable parameters (mass, dimensions) is crucial
- Reserve DR for hard-to-measure parameters (friction, contact dynamics, sensor noise)
- Active Domain Randomization (ADR) automatically tunes parameter ranges based on policy performance
- Curriculum learning: Start narrow ranges, expand as policy improves

**Critical Techniques:**
- Range-dependent sensor noise: LiDAR noise σ = 2% of range (not fixed absolute)
- Action delays: Real robots have 35-160ms total delay (sensing + communication + actuation)
- History augmentation: Include last 3-5 observations and 5-10 actions in state
- Action smoothness penalties: Prevent bang-bang control that fails on real hardware
- Physics randomization: Mass ±20%, friction [0.8, 1.2], wheel slip modeling

**Validated Parameter Ranges:**
- LiDAR: range noise 2% of distance, dropout 1-5%, intensity-based failures
- Odometry: drift 0.5-1.5% per meter with random walk component
- Action delays: [1-8] steps at 20Hz = [50-400ms]
- Robot mass: ±20% of nominal (e.g., 24-36kg for 30kg robot)
- Wheel friction: [0.8, 1.2] coefficient multiplier
- Slip events: Beta(5,1) distribution for realistic traction variation

**Evaluation Protocol:**
- 50+ episodes in both sim and real environments
- Performance ratio target: real/sim success rate > 0.8
- Robustness target: coefficient of variation < 0.3
- Failure mode categorization and analysis

### [I] Current Implementation Analysis

**IMPLEMENTED (Commit fe7a19d):**
- Generic noise model with Gaussian + dropout support
- LiDAR noise: 2cm fixed stddev, 1% dropout
- Odometry noise: 1% linear drift, 2% angular drift (motion-proportional)
- Seeded RNG for reproducibility
- Clean separation: NoiseConfig → NoiseModel → Sensor simulators

**CRITICAL GAPS:**
1. Physics randomization: NO dynamics modeling (kinematic only)
2. Action delays: 0ms delay (instantaneous actuation)
3. Temporal history: No observation/action buffering
4. Configuration: Noise parameters hardcoded, not in YAML
5. Automated DR: No ADR or curriculum learning
6. Evaluation: Basic metrics only, no reality gap measurement

**Reality Gap Risk: MODERATE-HIGH (40-60% transfer probability)**

Issues that will cause failures:
- Instantaneous velocity changes → oscillations on real motors
- No delay modeling → policies can't compensate for latency
- Fixed sensor noise → doesn't match range-dependent real sensors
- No physics variation → brittle to surface/mass changes
- No temporal context → can't predict future states

**Code Quality:** Excellent foundation with strong typing, seeded RNG, and clean abstractions. Ready for extension.

### [T] Reference Implementation Patterns

**Existing Pattern (well-designed):**
```cpp
struct NoiseConfig {
    float mean, stddev, dropout_prob, dropout_value;
    bool enabled;
};
class NoiseModel { /* Gaussian + dropout */ };
```

**Missing Patterns (to implement):**
- Domain randomization config system (YAML-based)
- Action delay buffer (FIFO queue with stochastic delays)
- Dynamics randomization (per-episode parameter sampling)
- History augmentation (observation/action buffers)
- Active DR manager (discriminator-based curriculum)
- Evaluation infrastructure (sim-to-real metrics)

## Target State

A complete sim-to-real transfer system with:

1. **Configuration-driven DR:** Single YAML file controlling all randomization
2. **Physics fidelity:** Mass, friction, inertia, wheel slip modeling
3. **Temporal realism:** Action delays [1-8] steps, observation/action history
4. **Robust sensors:** Range-dependent noise, cumulative drift, slip events
5. **Smooth control:** Action smoothness penalties to prevent jerky behavior
6. **Automated tuning:** ADR for curriculum learning (Phase 2)
7. **Rigorous evaluation:** 50+ episode protocols with reality gap metrics

**Success Criteria:**
- Real-world success rate > 80% of simulation performance
- Coefficient of variation < 0.3 in real deployment
- No bang-bang control oscillations
- Policies exhibit temporal reasoning (use history effectively)

## Implementation Plan

### Phase 1: Core Domain Randomization (Priority 1)

#### 1.1 Configuration Infrastructure

**File:** `C:\Users\costa\src\warehouser\ros_ws\config\domain_randomization.yaml`

```yaml
domain_randomization:
  enabled: true
  seed: 42

  dynamics:
    robot:
      mass: [24.0, 36.0]  # ±20% of 30kg
      wheel_friction: [0.8, 1.2]
      motor_damping: [0.8, 1.2]
      slip_factor_alpha: 5.0  # Beta(5,1)
      slip_factor_beta: 1.0

  sensors:
    lidar:
      range_noise_percent: 0.02  # 2% of range
      dropout_prob: [0.01, 0.05]
      intensity_threshold: [0.2, 0.4]
    odometry:
      linear_drift_per_meter: [0.005, 0.015]
      angular_drift_per_radian: [0.01, 0.03]
      random_walk_std: 0.005

  delays:
    enabled: true
    control_frequency: 20.0
    total_delay_steps: [1, 8]  # 50-400ms
    delay_noise_std: 0.5
    observation_history_length: 3
    action_history_length: 5
```

**Tasks:**
- [ ] Create YAML config with all DR parameters
- [ ] Add yaml-cpp dependency to CMakeLists.txt
- [ ] Create DRConfig C++ struct with parsing

#### 1.2 Action Delay System

**File:** `C:\Users\costa\src\warehouser\training\training\wrappers\action_delay.py`

Python Gymnasium wrapper implementing:
- FIFO action buffer (deque with max_delay + 5 capacity)
- Stochastic delay sampling: uniform([1, 8]) + Gaussian(0, 0.5)
- Observation augmentation: [current_obs, last_3_obs, last_5_actions]
- Modified observation space: base_dim + 3*base_dim + 5*action_dim

**C++ integration point:** `rl_bridge_node.cpp`
- Add action_buffer_ member (std::deque)
- In step service: buffer action, apply delayed action
- Track delay per step for debugging

**Tasks:**
- [ ] Implement ActionDelayWrapper Python class
- [ ] Update observation space dimensions in ROSGymEnv
- [ ] Add delay buffer to RLBridgeNode (optional for C++ side)
- [ ] Test with dummy environment (verify history shapes)

#### 1.3 Action Smoothness Penalty

**File:** `C:\Users\costa\src\warehouser\training\training\wrappers\smoothness.py`

Wrapper adding reward penalty: `-0.1 * ||action_t - action_{t-1}||^2`

**Tasks:**
- [ ] Implement ActionSmoothnessWrapper
- [ ] Add smoothness_weight to training config
- [ ] Track last_action, compute delta penalty
- [ ] Log smoothness metrics during training

#### 1.4 Physics Randomization

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\ros_simulation\include\ros_simulation\domain_randomizer.hpp`

C++ class for per-episode parameter sampling:
```cpp
struct DynamicsParameters {
    float mass, wheel_friction, motor_damping;
    float com_offset_x, com_offset_y;
    float slip_factor;
    bool slip_event;
};

class DomainRandomizer {
    DynamicsParameters sampleParameters();
    void applyToRobot(DynamicsParameters&, Robot&);
};
```

**Robot entity changes:**
- Add mass, friction, damping members to Robot class
- Modify update() to use acceleration limits (not instant velocity)
- Apply slip_factor to commanded velocities
- Integrate acceleration: v_new = v_old + accel*dt with damping

**Tasks:**
- [ ] Create DomainRandomizer class with Beta/Uniform sampling
- [ ] Extend Robot with dynamics parameters
- [ ] Replace kinematic update with dynamic integration
- [ ] Call sampleParameters() in WorldManager::reset()
- [ ] Expose dynamics to ROS parameters

### Phase 2: Sensor Improvements (Priority 2)

#### 2.1 Range-Dependent LiDAR Noise

**Current:** Fixed 2cm noise
**Target:** σ = 0.02 * range (proportional)

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\lidar_simulator.cpp`

Modify noise application:
```cpp
for (size_t i = 0; i < ranges.size(); ++i) {
    float noise_stddev = config_.noise.range_stddev_percent * ranges[i];
    ranges[i] += noise_model_.sample(0.0f, noise_stddev);
}
```

**Tasks:**
- [ ] Change LidarNoiseConfig: range_stddev → range_stddev_percent
- [ ] Apply per-ray noise based on measured range
- [ ] Update tests to verify range-dependent behavior

#### 2.2 Cumulative Odometry Drift

**Current:** Motion-proportional noise (good)
**Missing:** Random walk component, systematic bias

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\odometry_simulator.cpp`

Add state:
```cpp
class OdometrySimulator {
    float accumulated_drift_x_ = 0.0f;
    float accumulated_drift_y_ = 0.0f;
    float accumulated_drift_theta_ = 0.0f;
};
```

Each step:
```cpp
// Motion-proportional noise (existing)
float motion_noise = linear_dist * config_.linear_stddev;

// Random walk component (new)
accumulated_drift_x_ += normal(0, config_.random_walk_std);
accumulated_drift_y_ += normal(0, config_.random_walk_std);

// Apply both
dx += motion_noise + accumulated_drift_x_;
dy += accumulated_drift_y_;
```

**Tasks:**
- [ ] Add accumulated drift state to OdometrySimulator
- [ ] Implement random walk component
- [ ] Reset accumulated drift on episode reset
- [ ] Add config parameter for random_walk_std

#### 2.3 YAML Configuration Exposure

**Current:** Noise params hardcoded in C++ headers
**Target:** All parameters in YAML, loaded at runtime

**File:** `C:\Users\costa\src\warehouser\ros_ws\config\observations_params.yaml`

Add:
```yaml
observations:
  ros__parameters:
    # Existing params...

    # LiDAR noise
    lidar_noise_enabled: true
    lidar_range_noise_percent: 0.02
    lidar_dropout_prob_min: 0.01
    lidar_dropout_prob_max: 0.05

    # Odometry noise
    odom_noise_enabled: true
    odom_linear_drift_min: 0.005
    odom_linear_drift_max: 0.015
    odom_angular_drift_min: 0.01
    odom_angular_drift_max: 0.03
    odom_random_walk_std: 0.005
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observations_node.cpp`

Load in constructor:
```cpp
float lidar_noise_percent = declare_parameter("lidar_range_noise_percent", 0.02);
float dropout_min = declare_parameter("lidar_dropout_prob_min", 0.01);
// ... apply to LidarConfig
```

**Tasks:**
- [ ] Add all noise parameters to YAML
- [ ] Declare and load parameters in observations_node
- [ ] Randomize per episode (sample from [min, max] ranges)
- [ ] Document parameters in README

### Phase 3: Evaluation Infrastructure (Priority 2)

#### 3.1 Sim-to-Real Metrics

**File:** `C:\Users\costa\src\warehouser\training\training\evaluation\sim_to_real_metrics.py`

Classes:
- `EpisodeResult`: Dataclass storing success, reward, trajectory, actions, timing
- `SimToRealEvaluator`: Accumulates sim/real results, computes metrics

Metrics:
```python
{
    "sim_success_rate": float,
    "real_success_rate": float,
    "performance_ratio": float,  # Target: > 0.8
    "real_cv": float,  # Coefficient of variation, target: < 0.3
    "sim_collision_rate": float,
    "real_collision_rate": float,
    "failure_modes": {"collision": int, "timeout": int, ...}
}
```

**Tasks:**
- [ ] Implement EpisodeResult dataclass
- [ ] Implement SimToRealEvaluator with metric computation
- [ ] Add failure mode categorization
- [ ] Generate markdown reports with recommendations

#### 3.2 Evaluation Protocol Script

**File:** `C:\Users\costa\src\warehouser\training\scripts\evaluate_transfer.py`

Script workflow:
1. Load trained policy
2. Run 50 episodes in simulation
3. Deploy to real robot (manual step)
4. Run 50 episodes in reality
5. Generate comparison report

**Tasks:**
- [ ] Create evaluation script
- [ ] Add trajectory logging
- [ ] Implement real-robot interface (stub for now)
- [ ] Document evaluation procedure in README

### Phase 4: Advanced Features (Future Work)

#### 4.1 Active Domain Randomization (ADR)

**File:** `C:\Users\costa\src\warehouser\training\training\wrappers\active_dr.py`

Components:
- Discriminator network (3-layer MLP)
- Trajectory buffer (reference vs randomized)
- Parameter range adaptation (expand/contract based on performance)

Algorithm:
1. Collect reference trajectories (no randomization)
2. Train discriminator to distinguish randomized from reference
3. Sample "hard" environments (high discriminator confidence)
4. Adjust parameter ranges based on success rate thresholds

**Tasks (deferred):**
- [ ] Implement Discriminator PyTorch model
- [ ] Create ActiveDRManager class
- [ ] Integrate with training loop
- [ ] Experiment with ADR vs fixed DR performance

#### 4.2 Complete V2_Lidar Observation

**Current:** buildV2() falls back to V1
**Target:** Full lidar-based observation (no ground truth position)

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observation_builder.cpp`

Implement buildV2:
```cpp
Observation buildV2(World& world, Goal& goal, size_t robot_idx) {
    // [lidar_ranges(60), goal_bearing, goal_dist, velocity, is_carrying]
    // NO absolute position (x, y)
}
```

**Tasks (deferred):**
- [ ] Implement buildV2 with lidar observations
- [ ] Update Python observation space dimensions
- [ ] Train policies with V2 observations
- [ ] Compare V1 vs V2 sim-to-real transfer

## Interface Definitions

### C++ Domain Randomizer

```cpp
// domain_randomizer.hpp
namespace warehouser {

struct DRConfig {
    bool enabled;
    unsigned int seed;

    struct {
        float mass_min, mass_max;
        float wheel_friction_min, wheel_friction_max;
        float motor_damping_min, motor_damping_max;
        float slip_alpha, slip_beta;
        float slip_event_prob;
    } dynamics;

    struct {
        float range_noise_percent;
        float dropout_prob_min, dropout_prob_max;
    } lidar;

    struct {
        float linear_drift_min, linear_drift_max;
        float angular_drift_min, angular_drift_max;
        float random_walk_std;
    } odometry;

    struct {
        bool enabled;
        int total_delay_steps_min, total_delay_steps_max;
        float delay_noise_std;
    } delays;
};

struct DynamicsParameters {
    float mass;
    float wheel_friction;
    float motor_damping;
    float slip_factor;
    bool slip_event;
};

class DomainRandomizer {
public:
    explicit DomainRandomizer(const DRConfig& config);

    DynamicsParameters sampleParameters();
    void applyToRobot(const DynamicsParameters& params, Robot& robot);
    void setSeed(unsigned int seed);
    const DRConfig& config() const;

private:
    DRConfig config_;
    std::mt19937 rng_;

    float sampleUniform(float min, float max);
    float sampleBeta(float alpha, float beta);
    bool sampleBernoulli(float prob);
};

} // namespace warehouser
```

### Python Action Delay Wrapper

```python
# action_delay.py
from collections import deque
import gymnasium as gym
import numpy as np
from numpy.typing import NDArray

class ActionDelayWrapper(gym.Wrapper):
    """Applies realistic action delays with history augmentation."""

    def __init__(
        self,
        env: gym.Env,
        min_delay_steps: int = 1,
        max_delay_steps: int = 8,
        delay_noise_std: float = 0.5,
        action_history_length: int = 5,
        obs_history_length: int = 3,
        enabled: bool = True,
    ):
        """Initialize delay wrapper with history buffers."""
        ...

    def reset(self, **kwargs) -> tuple[NDArray, dict]:
        """Reset and initialize history buffers with zeros."""
        ...

    def step(self, action: NDArray) -> tuple[NDArray, float, bool, bool, dict]:
        """Execute delayed action, return augmented observation."""
        ...

    def _sample_delay(self) -> int:
        """Sample stochastic delay: uniform + Gaussian noise."""
        ...

    def _augment_observation(self, obs: NDArray) -> NDArray:
        """Concatenate: [current_obs, obs_history, action_history]."""
        ...
```

### Python Smoothness Wrapper

```python
# smoothness.py
class ActionSmoothnessWrapper(gym.Wrapper):
    """Penalizes jerky control: -weight * ||Δaction||²"""

    def __init__(
        self,
        env: gym.Env,
        smoothness_weight: float = 0.1,
        max_action_change: float = 0.3,
        enabled: bool = True,
    ):
        ...

    def step(self, action: NDArray) -> tuple:
        """Compute smoothness penalty, add to reward."""
        ...
```

### Python Evaluation Infrastructure

```python
# sim_to_real_metrics.py
from dataclasses import dataclass
import numpy as np
from numpy.typing import NDArray

@dataclass
class EpisodeResult:
    success: bool
    reward: float
    episode_length: int
    collision_occurred: bool
    trajectory: NDArray  # [T, 2]
    actions: NDArray  # [T, action_dim]
    final_distance_to_goal: float
    time_seconds: float

class SimToRealEvaluator:
    def __init__(self):
        self.sim_results: list[EpisodeResult] = []
        self.real_results: list[EpisodeResult] = []

    def add_sim_result(self, result: EpisodeResult) -> None: ...
    def add_real_result(self, result: EpisodeResult) -> None: ...

    def compute_metrics(self) -> dict[str, Any]:
        """Returns: success rates, performance ratio, CV, collision rates."""
        ...

    def analyze_failure_modes(self) -> dict[str, int]:
        """Categorize failures: collision, timeout, stuck, goal_missed."""
        ...

    def generate_report(self, output_path: Path) -> None:
        """Generate markdown evaluation report."""
        ...
```

## Files to Create

| File | Purpose |
|------|---------|
| `ros_ws/config/domain_randomization.yaml` | Central DR configuration (all parameters) |
| `ros_ws/src/ros_simulation/include/ros_simulation/domain_randomizer.hpp` | C++ dynamics randomization |
| `ros_ws/src/ros_simulation/src/domain_randomizer.cpp` | Implementation |
| `ros_ws/src/ros_simulation/tests/test_domain_randomizer.cpp` | Unit tests |
| `training/training/wrappers/__init__.py` | Wrapper package init |
| `training/training/wrappers/action_delay.py` | Action delay + history augmentation |
| `training/training/wrappers/smoothness.py` | Action smoothness penalty |
| `training/training/wrappers/active_dr.py` | Active DR (Phase 4, deferred) |
| `training/training/evaluation/__init__.py` | Evaluation package init |
| `training/training/evaluation/sim_to_real_metrics.py` | Metrics and report generation |
| `training/scripts/evaluate_transfer.py` | Evaluation protocol script |
| `training/tests/test_action_delay.py` | Test delay wrapper |
| `training/tests/test_smoothness.py` | Test smoothness wrapper |

## Files to Modify

| File | Change |
|------|--------|
| `ros_ws/src/ros_simulation/include/ros_simulation/robot.hpp` | Add mass, friction, damping members; change update() to dynamic integration |
| `ros_ws/src/ros_simulation/src/robot.cpp` | Implement acceleration-based dynamics |
| `ros_ws/src/ros_simulation/src/world_manager.cpp` | Call DomainRandomizer in reset() |
| `ros_ws/src/warehouser_observations/include/warehouser_observations/noise_model.hpp` | Add range_stddev_percent, randomization ranges |
| `ros_ws/src/warehouser_observations/src/lidar_simulator.cpp` | Apply range-dependent noise |
| `ros_ws/src/warehouser_observations/src/odometry_simulator.cpp` | Add cumulative drift, random walk |
| `ros_ws/src/warehouser_observations/src/observations_node.cpp` | Load DR parameters from YAML |
| `ros_ws/config/observations_params.yaml` | Add all noise parameter ranges |
| `training/training/envs/ros_env.py` | Wrap with ActionDelayWrapper, ActionSmoothnessWrapper |
| `training/training/models/config.py` | Add DR config fields (delays, smoothness_weight) |
| `training/scripts/train.py` | Apply wrappers before training |
| `ros_ws/src/ros_simulation/CMakeLists.txt` | Add yaml-cpp dependency |
| `training/pyproject.toml` | Add torch dependency (for ADR, Phase 4) |

## Architecture Notes

### Design Principles

1. **Separation of Concerns:**
   - Configuration (YAML) ← Loading (ROS params) ← Randomization (C++ classes) ← Application (simulation)
   - Python wrappers are composable (delay + smoothness + ...) via Gymnasium API

2. **Reproducibility:**
   - All randomization is seeded
   - Episode seed stored in metadata
   - Enable deterministic debugging

3. **Modularity:**
   - Each wrapper is independently toggleable (enabled flag)
   - Ablation studies: disable individual components
   - Configuration-driven: no recompilation needed

4. **Performance:**
   - C++ randomization (fast): physics, sensors
   - Python wrappers (flexible): history, delays, evaluation
   - Minimize Python ↔ C++ boundary crossings

5. **Testability:**
   - Unit tests for each randomizer (verify distributions)
   - Integration tests (full pipeline)
   - Evaluation framework for sim-to-real gap

### Key Architectural Decisions

**Why Python wrappers for delays?**
- Gymnasium API standard, easy to compose
- History management simpler in Python (numpy arrays)
- C++ RLBridge stays lightweight

**Why C++ for physics randomization?**
- Tightly coupled to simulation step
- Performance critical (runs every timestep)
- Type safety for dynamics parameters

**Why YAML for configuration?**
- Single source of truth
- No recompilation for parameter tuning
- Easy to version control experiments
- Standard in ROS ecosystem

**Observation space expansion:**
- Current V1: 8 dims [robot_x, robot_y, theta, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
- With history: 8 + 8*3 + 2*5 = 42 dims (current + 3 obs history + 5 action history)
- Action dim: 2 [linear_vel, angular_vel]
- Network capacity: Existing [64, 64] policy should handle 42 dims, may benefit from [128, 128]

**Training strategy:**
- Start: Train without delays to learn basic navigation
- Curriculum: Gradually introduce delays [0→2→4→8] steps over training
- Randomization: Enable all sensor/dynamics DR from start
- Smoothness: Enable from start to learn smooth policies

## Verification

### Unit Tests

**C++ DomainRandomizer:**
- [ ] Verify uniform sampling in correct ranges
- [ ] Verify Beta(5,1) distribution for slip factor
- [ ] Verify Bernoulli sampling for slip events
- [ ] Test seed reproducibility
- [ ] Test parameter application to robot entity

**Python ActionDelayWrapper:**
- [ ] Verify observation space dimensions
- [ ] Verify action buffering (FIFO behavior)
- [ ] Verify history augmentation shape
- [ ] Test with zero delay (should work)
- [ ] Test with max delay (boundary condition)

**Python ActionSmoothnessWrapper:**
- [ ] Verify penalty computation
- [ ] Verify reward modification
- [ ] Test hard limit on action change
- [ ] Test with zero weight (no effect)

**Sensor Noise:**
- [ ] Range-dependent LiDAR noise (plot σ vs range)
- [ ] Odometry cumulative drift over 100m
- [ ] Random walk component statistical properties

### Integration Tests

**Training Pipeline:**
- [ ] Train with all wrappers enabled
- [ ] Verify observation shapes match network input
- [ ] Log delay statistics during training
- [ ] Monitor smoothness penalty values

**Episode Evaluation:**
- [ ] Run 50 sim episodes, compute metrics
- [ ] Verify metric computation (success rate, CV)
- [ ] Generate report, verify markdown format

### Ablation Studies

Compare policies trained with:
1. No DR (baseline)
2. Sensor noise only
3. Dynamics randomization only
4. Action delays only
5. Full DR suite

Evaluate each in simulation with:
- Nominal parameters (clean environment)
- Extreme randomization (stress test)
- Targeted perturbations (specific failure modes)

### Reality Gap Evaluation

**Prerequisites:**
- Trained policy with full DR
- Real robot setup with ROS2 interface
- Comparable test scenarios (sim and real)

**Protocol:**
1. Simulate 50 episodes, log all data
2. Deploy same policy to real robot
3. Execute 50 comparable episodes
4. Run SimToRealEvaluator.generate_report()
5. Analyze failure modes
6. Iterate: Adjust DR parameters based on gap findings

**Success Metrics:**
- Performance ratio > 0.8 → GOOD TRANSFER
- CV < 0.3 → ROBUST POLICY
- No oscillations → SMOOTH CONTROL
- Collision rate < 5% → SAFE OPERATION

### Debugging Checklist

If transfer fails (ratio < 0.8):
- [ ] Verify sensor noise matches real sensors (collect real data)
- [ ] Check delay distribution matches real system (measure latency)
- [ ] Validate physics parameters (measure real robot mass/friction)
- [ ] Inspect failure modes (collision vs timeout vs stuck)
- [ ] Review trajectories (sim vs real path comparison)

If high variance (CV > 0.3):
- [ ] Expand randomization ranges
- [ ] Add missing randomization sources
- [ ] Check for systematic bias (accumulated drift)
- [ ] Verify episode initial conditions are varied

If oscillations/instability:
- [ ] Increase smoothness penalty weight
- [ ] Add acceleration limits to dynamics
- [ ] Check control frequency (may need higher rate)
- [ ] Verify delay modeling is enabled

## Implementation Timeline

### Week 1: Core Infrastructure (40 hours)
- Day 1-2: YAML config + DomainRandomizer class (16h)
- Day 3: Action delay wrapper (8h)
- Day 4: Smoothness wrapper + physics randomization (8h)
- Day 5: YAML parameter exposure + integration (8h)

### Week 2: Sensor Improvements (24 hours)
- Day 1: Range-dependent LiDAR noise (6h)
- Day 2: Cumulative odometry drift (8h)
- Day 3: Testing and validation (8h)
- Day 4-5: Training experiments with full DR (buffer)

### Week 3: Evaluation Infrastructure (16 hours)
- Day 1: SimToRealEvaluator implementation (8h)
- Day 2: Evaluation script + report generation (6h)
- Day 3: Documentation + README updates (2h)

### Future: Advanced Features (56+ hours)
- Active DR implementation (40h)
- V2_Lidar observation completion (12h)
- Real robot integration (ongoing)

**Total Estimated Effort:** 80 hours (Weeks 1-3) + 56 hours (Future)

**Critical Path for Real Deployment:** Weeks 1-2 (64 hours)

## Success Criteria Summary

**Phase 1 Complete:**
- [ ] All DR parameters configurable via YAML
- [ ] Action delays [1-8] steps functional
- [ ] Observation history [3 obs + 5 actions] augmented
- [ ] Physics randomization (mass, friction, slip) active
- [ ] Smoothness penalty prevents oscillations
- [ ] Training converges with full DR enabled

**Phase 2 Complete:**
- [ ] Range-dependent sensor noise implemented
- [ ] Cumulative drift with random walk component
- [ ] All noise parameters in YAML, randomized per episode

**Phase 3 Complete:**
- [ ] SimToRealEvaluator functional
- [ ] Evaluation protocol documented and tested
- [ ] Baseline metrics collected (no DR vs full DR)

**Transfer Success (Final Goal):**
- [ ] Real/sim success rate > 0.8
- [ ] Real CV < 0.3
- [ ] Collision rate < 5%
- [ ] No bang-bang control artifacts
- [ ] Policies exhibit temporal reasoning (use history)

## References

- [S.md] Research findings: 2025-2026 state-of-the-art techniques
- [I.md] Current implementation analysis and gap assessment
- [T.md] Reference implementation patterns and code templates
- CLAUDE.md: Project coding standards (C++23, Python 3.12, TypeScript)
- Commit fe7a19d: Initial sensor noise implementation

## Next Steps

1. Review this TASK with stakeholders for scope agreement
2. Create feature branch: `feature/sim-to-real-dr`
3. Begin Phase 1.1: YAML configuration infrastructure
4. Implement DomainRandomizer C++ class with unit tests
5. Create Python wrappers with integration tests
6. Train baseline policies (no DR) for comparison
7. Train with progressive DR (sensor → dynamics → delays)
8. Document results and prepare for Phase 2

**Recommended Approach:** Implement and test each component independently, then integrate incrementally. Validate each phase with training experiments before moving to next phase.
