# TASK: Action Space Refactoring and Safety Integration

Created: 2026-02-12T23:15:00Z
Build: PARTIAL (Python tests have import errors, ROS build status unknown)
Tests: 0/28 passing (import errors prevent execution)

## Summary

The Warehouser action space implementation is functional but fragile. Actions work due to a fortunate coincidence: normalized [-1,1] values happen to match physical velocity limits, not by design. Critical gaps include: no explicit action scaling, disconnected safety controller, missing action smoothing, and poor discrete action integration. This task refactors the action processing pipeline to be explicit, safe, and suitable for sim-to-real transfer.

## Context

### [S] Search Findings - Best Practices from Recent Research

Research from 13+ papers (2024-2025) reveals key action space design principles:

1. **Joint Velocity Control**: Joint velocity action spaces achieve best sim-to-real transfer performance with lowest tracking error and control variability
2. **Hybrid Action Spaces**: Robotics naturally requires hybrid discrete-continuous actions (navigation + manipulation), but naive approaches lose structural information
3. **Action Smoothing**: RL policies produce jerky trajectories; EMA filters and motion-aware rewards are critical for realistic motion
4. **Action Normalization**: Normalize to [-1,1], then scale to physical limits based on joint bounds
5. **Hierarchical Abstraction**: Skill primitives (options framework) enable long-horizon tasks by decomposing "what to do" from "how to do it"

### [I] Introspection Findings - Critical Implementation Issues

Deep code analysis reveals the action processing pipeline has serious design gaps:

**Issue 1: Accidental Action Scaling**
- Actions in [-1,1] sent directly to `/cmd_vel` without scaling
- Works only because normalized range accidentally matches physical limits (v_max=1.0 m/s, omega_max=2.0 rad/s)
- If physical limits change, action space breaks
- Location: `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:153-163`

**Issue 2: Disconnected Safety Layer**
- SafetyController exists with emergency stop and slowdown logic
- NEVER called during training or action execution
- Policy never experiences safety interventions
- Location: `ros_ws/src/warehouser_safety/src/safety_controller.cpp:30-68`

**Issue 3: No Action Smoothing**
- No temporal filtering, acceleration limits, or jerk minimization
- Abrupt velocity changes cause unrealistic motion
- Sim-to-real gap: real robots have inertia and acceleration constraints

**Issue 4: Discrete Action Type Mismatch**
- Pick/place are discrete (binary) but encoded as continuous float32 in [-1,1]
- Threshold=0.5 hardcoded, undocumented
- No feedback whether pick/place succeeded (blind commands)
- No action masking to prevent invalid states (pick while carrying)

**Issue 5: Multi-Robot Support Incomplete**
- Action routing only works for robot_id=0
- Multi-robot action execution not implemented

### [T] Template Findings - Production-Ready Patterns

Copy-paste ready Gymnasium wrapper patterns for:
- Hybrid action space encoding (Dict vs threshold-based)
- Action scaling/normalization wrappers
- EMA and low-pass filtering for smoothing
- Acceleration limiting for realistic dynamics
- Action masking for invalid discrete actions
- Safety clipping layers
- Complete wrapper chain pipeline

## Objective

Refactor the action processing pipeline to be explicit, modular, and robust. Ensure actions are properly scaled, smoothed, safe, and suitable for sim-to-real transfer.

## Target State: Proper Action Space Architecture

### Action Processing Pipeline

```
Policy Output [-1, 1]
    |
    v
1. Action Validation (check dimensions, ranges)
    |
    v
2. Action Masking (prevent invalid pick/place)
    |
    v
3. Action Scaling (normalize to physical limits)
    |
    v
4. Action Smoothing (EMA filter for velocity)
    |
    v
5. Acceleration Limiting (enforce realistic dynamics)
    |
    v
6. Safety Layer Integration (emergency stop, slowdown)
    |
    v
7. ROS Command Publication
```

### Python Wrapper Architecture

```python
# Gymnasium wrapper chain
env = ROSGymEnv(config)
env = ActionScalingWrapper(env, velocity_limits={"linear": 0.5, "angular": 1.0})
env = ActionSmoothingWrapper(env, alpha=0.3)
env = AccelerationLimitWrapper(env, max_delta={"linear": 2.0, "angular": 4.0})
env = SafetyClippingWrapper(env, hard_limits={...})
env = TimeLimit(env, max_episode_steps=500)
```

### C++ RLBridge Integration

```cpp
// In RLBridgeNode::sendAction()
void RLBridgeNode::sendAction(float linear, float angular, float pick, float place) {
    // 1. Scale normalized actions to physical limits
    Velocity cmd;
    cmd.linear = linear * config_.v_max;      // [-1,1] -> [-v_max, v_max]
    cmd.angular = angular * config_.omega_max;

    // 2. Apply safety layer
    Velocity safe_cmd = safety_controller_.applySafetyLimits(cmd, last_lidar_);

    // 3. Publish safe command
    geometry_msgs::msg::Twist twist;
    twist.linear.x = safe_cmd.linear;
    twist.angular.z = safe_cmd.angular;
    cmd_pub_->publish(twist);

    // 4. Handle discrete actions with threshold
    if (pick > 0.5f && !is_carrying_) {
        pick_pub_->publish(std_msgs::msg::Empty());
    }
    if (place > 0.5f && is_carrying_) {
        unpick_pub_->publish(std_msgs::msg::Empty());
    }
}
```

## Implementation Plan

### Phase 1: Python Action Wrappers

Create modular Gymnasium wrappers for action processing:

**1.1 Action Scaling Wrapper**
- Maps normalized [-1,1] actions to physical velocity limits
- Configurable max_linear_vel, max_angular_vel
- Keeps pick/place in [-1,1] for threshold processing

**1.2 Action Smoothing Wrapper**
- Exponential Moving Average (EMA) filter for velocity commands
- Configurable alpha smoothing factor (default 0.3)
- Only smooths continuous actions, not discrete triggers

**1.3 Acceleration Limit Wrapper**
- Enforces max linear/angular acceleration between timesteps
- Simulates realistic motor dynamics and inertia
- Critical for sim-to-real transfer

**1.4 Action Masking Logic**
- Prevent pick action if already carrying
- Prevent place action if not carrying
- Implement in ROSGymEnv.step() or dedicated wrapper

**1.5 Safety Clipping Wrapper**
- Final hard bounds before ROS transmission
- Emergency stop condition callable
- Adds safety state to info dict

### Phase 2: RLBridge Safety Integration

**2.1 Connect SafetyController to Action Execution**
- Instantiate SafetyController in RLBridgeNode
- Cache latest lidar data from observations
- Call applySafetyLimits() before publishing cmd_vel
- Make safety interventions visible to policy

**2.2 Add Safety State to Observations**
- Include safety_state (NORMAL/SLOWDOWN/EMERGENCY) in observation
- Allow policy to learn from safety interventions
- Add to RLStepResponse service

**2.3 Make Velocity Limits Configurable**
- Add v_max, omega_max to RLBridgeNode parameters
- Remove hardcoded limits in Robot class
- Document that actions are normalized [-1,1]

### Phase 3: Discrete Action Feedback

**3.1 Add Pick/Place Success Flags to Observation**
- Return pick_success, place_success booleans in RLStepResponse
- Allow policy to learn optimal timing for manipulation
- Currently blind - no feedback loop

**3.2 Implement Action Masking in Simulation**
- Return is_carrying state in observation (already exists)
- Use for masking invalid actions in Python wrapper
- Document discrete action encoding (threshold=0.5)

### Phase 4: Multi-Robot Action Routing

**4.1 Complete Multi-Robot Support in RLBridge**
- Route actions to correct robot_id (currently only id=0)
- Use namespaced topics or array publishers
- Align with existing multi-robot observation code

### Phase 5: Documentation and Testing

**5.1 Document Action Space Contract**
- Actions are normalized to [-1,1] at policy output
- Explicit scaling happens in RLBridge
- Discrete actions use threshold=0.5
- Add to CLAUDE.md and code comments

**5.2 Add Action Wrapper Tests**
- Unit tests for each wrapper (smoothing, scaling, acceleration)
- Integration test for full wrapper chain
- Verify action bounds, masking, smoothing behavior

**5.3 Add RLBridge Integration Tests**
- Test safety controller integration
- Verify action scaling correctness
- Test discrete action triggering

## Interface Definitions

### Python Action Wrappers

```python
# training/training/wrappers/action_scaling.py
class ActionScalingWrapper(gym.ActionWrapper):
    """Scale normalized actions [-1, 1] to physical velocity limits."""

    def __init__(
        self,
        env: gym.Env,
        velocity_limits: dict[str, float]
    ):
        """
        Args:
            env: Base environment
            velocity_limits: {"linear": 0.5, "angular": 1.0} in m/s and rad/s
        """
        super().__init__(env)
        self.linear_max = velocity_limits["linear"]
        self.angular_max = velocity_limits["angular"]

    def action(self, action: np.ndarray) -> np.ndarray:
        """Scale action from [-1, 1] to physical limits."""
        scaled = action.copy()
        scaled[0] = action[0] * self.linear_max   # Linear velocity
        scaled[1] = action[1] * self.angular_max  # Angular velocity
        # Keep pick/place in [-1, 1] for threshold processing
        return scaled
```

```python
# training/training/wrappers/action_smoothing.py
class ActionSmoothingWrapper(gym.ActionWrapper):
    """Smooth actions using exponential moving average."""

    def __init__(self, env: gym.Env, alpha: float = 0.3):
        """
        Args:
            alpha: Smoothing factor in [0, 1]
                   0 = maximum smoothing (no new action)
                   1 = no smoothing (raw action)
                   Typical: 0.2-0.4 for robotics
        """
        super().__init__(env)
        self.alpha = alpha
        self.prev_action: np.ndarray | None = None

    def action(self, action: np.ndarray) -> np.ndarray:
        """Apply EMA smoothing to continuous actions only."""
        if self.prev_action is None:
            self.prev_action = action.copy()
            return action

        # Smooth only velocity (indices 0-1), not discrete (2-3)
        smoothed = action.copy()
        smoothed[:2] = (
            self.alpha * action[:2] +
            (1 - self.alpha) * self.prev_action[:2]
        )

        self.prev_action = smoothed
        return smoothed

    def reset(self, **kwargs):
        """Clear filter state on reset."""
        self.prev_action = None
        return self.env.reset(**kwargs)
```

```python
# training/training/wrappers/accel_limit.py
class AccelerationLimitWrapper(gym.ActionWrapper):
    """Limit action changes (acceleration) between timesteps."""

    def __init__(
        self,
        env: gym.Env,
        max_delta: dict[str, float],
        dt: float = 0.05  # Timestep in seconds
    ):
        """
        Args:
            max_delta: Maximum acceleration per timestep
                Example: {"linear": 2.0, "angular": 4.0} m/s² and rad/s²
            dt: Simulation timestep
        """
        super().__init__(env)
        self.max_linear_accel = max_delta["linear"]
        self.max_angular_accel = max_delta["angular"]
        self.dt = dt
        self.prev_velocity = np.zeros(2, dtype=np.float32)

    def action(self, action: np.ndarray) -> np.ndarray:
        """Clip acceleration to limits."""
        desired_velocity = action[:2].copy()

        # Compute desired acceleration
        accel = (desired_velocity - self.prev_velocity) / self.dt

        # Clip acceleration
        accel = np.clip(
            accel,
            [-self.max_linear_accel, -self.max_angular_accel],
            [self.max_linear_accel, self.max_angular_accel]
        )

        # Compute clamped velocity
        clamped_velocity = self.prev_velocity + accel * self.dt
        self.prev_velocity = clamped_velocity

        result = action.copy()
        result[:2] = clamped_velocity
        return result

    def reset(self, **kwargs):
        self.prev_velocity = np.zeros(2, dtype=np.float32)
        return self.env.reset(**kwargs)
```

```python
# training/training/wrappers/safety.py
class SafetyClippingWrapper(gym.ActionWrapper):
    """Final safety layer to ensure actions within safe bounds."""

    def __init__(
        self,
        env: gym.Env,
        hard_limits: dict[str, tuple[float, float]]
    ):
        """
        Args:
            hard_limits: Absolute bounds {"linear": (-0.5, 0.5), "angular": (-1.0, 1.0)}
        """
        super().__init__(env)
        self.limits = hard_limits

    def action(self, action: np.ndarray) -> np.ndarray:
        """Apply safety clipping."""
        safe_action = action.copy()
        safe_action[0] = np.clip(
            action[0],
            self.limits["linear"][0],
            self.limits["linear"][1]
        )
        safe_action[1] = np.clip(
            action[1],
            self.limits["angular"][0],
            self.limits["angular"][1]
        )
        return safe_action
```

```python
# training/training/envs/factory.py
def create_warehouser_env(config: dict) -> gym.Env:
    """Create fully wrapped Warehouser environment.

    Processing pipeline:
    1. Base ROSGymEnv
    2. Action scaling (normalize to physical limits)
    3. Action smoothing (EMA filter)
    4. Acceleration limiting (realistic dynamics)
    5. Safety clipping (hard bounds)
    6. Time limit

    Args:
        config: Environment configuration dict

    Returns:
        Wrapped environment ready for training
    """
    from training.envs.ros_env import ROSGymEnv
    from training.wrappers.action_scaling import ActionScalingWrapper
    from training.wrappers.action_smoothing import ActionSmoothingWrapper
    from training.wrappers.accel_limit import AccelerationLimitWrapper
    from training.wrappers.safety import SafetyClippingWrapper
    from gymnasium.wrappers import TimeLimit

    # Base environment
    env = ROSGymEnv(config.get("env_config"))

    # Action processing pipeline
    env = ActionScalingWrapper(
        env,
        velocity_limits={
            "linear": config.get("max_linear_vel", 0.5),
            "angular": config.get("max_angular_vel", 1.0)
        }
    )

    if config.get("enable_smoothing", True):
        env = ActionSmoothingWrapper(
            env,
            alpha=config.get("smoothing_alpha", 0.3)
        )

    if config.get("enable_accel_limits", True):
        env = AccelerationLimitWrapper(
            env,
            max_delta={
                "linear": config.get("max_linear_accel", 2.0),
                "angular": config.get("max_angular_accel", 4.0)
            },
            dt=config.get("timestep", 0.05)
        )

    env = SafetyClippingWrapper(
        env,
        hard_limits={
            "linear": (-0.5, 0.5),
            "angular": (-1.0, 1.0)
        }
    )

    env = TimeLimit(
        env,
        max_episode_steps=config.get("max_episode_steps", 500)
    )

    return env
```

### C++ RLBridge Changes

```cpp
// ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp
class RLBridgeNode : public rclcpp::Node {
private:
    // Add safety controller member
    std::unique_ptr<warehouser_safety::SafetyController> safety_controller_;

    // Add velocity limit parameters
    float v_max_;
    float omega_max_;

    // Cache robot carrying state for action masking
    std::vector<bool> robots_carrying_;

    // Add safety configuration
    warehouser_safety::SafetyConfig safety_config_;
};

// Update sendAction signature to include robot carrying state
void sendAction(
    size_t robot_id,
    float linear_normalized,    // In [-1, 1]
    float angular_normalized,   // In [-1, 1]
    float pick,                 // In [-1, 1], threshold at 0.5
    float place                 // In [-1, 1], threshold at 0.5
);
```

```cpp
// ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp
void RLBridgeNode::sendAction(
    size_t robot_id,
    float linear_normalized,
    float angular_normalized,
    float pick,
    float place
) {
    // 1. Scale normalized actions to physical limits
    warehouser_safety::Velocity cmd;
    cmd.linear = linear_normalized * v_max_;
    cmd.angular = angular_normalized * omega_max_;

    // 2. Apply safety layer with latest lidar data
    auto safe_cmd = safety_controller_->applySafetyLimits(
        cmd,
        last_lidar_data_[robot_id]
    );

    // 3. Publish safe velocity command
    geometry_msgs::msg::Twist twist_msg;
    twist_msg.linear.x = safe_cmd.linear;
    twist_msg.angular.z = safe_cmd.angular;
    cmd_pub_->publish(twist_msg);

    // 4. Handle discrete actions with masking
    bool is_carrying = robots_carrying_[robot_id];

    // Pick action: threshold at 0.5, only if not carrying
    if (pick > 0.5f && !is_carrying) {
        pick_pub_->publish(std_msgs::msg::Empty());
    }

    // Place action: threshold at 0.5, only if carrying
    if (place > 0.5f && is_carrying) {
        unpick_pub_->publish(std_msgs::msg::Empty());
    }
}
```

### ROS Service Extensions

```
# ros_ws/src/warehouser_msgs/srv/RLStep.srv
# Request
float32 action_linear      # Normalized to [-1, 1]
float32 action_angular     # Normalized to [-1, 1]
float32 action_pick        # Normalized to [-1, 1], threshold=0.5
float32 action_place       # Normalized to [-1, 1], threshold=0.5

---

# Response
float32[] observation
float32 reward
bool terminated
bool truncated

# NEW: Safety and discrete action feedback
uint8 safety_state         # 0=NORMAL, 1=SLOWDOWN, 2=EMERGENCY
bool pick_success          # Did pick action succeed?
bool place_success         # Did place action succeed?
```

## Files to Create

| File | Purpose |
|------|---------|
| `training/training/wrappers/__init__.py` | Wrapper module exports |
| `training/training/wrappers/action_scaling.py` | ActionScalingWrapper implementation |
| `training/training/wrappers/action_smoothing.py` | ActionSmoothingWrapper (EMA filter) |
| `training/training/wrappers/accel_limit.py` | AccelerationLimitWrapper implementation |
| `training/training/wrappers/safety.py` | SafetyClippingWrapper implementation |
| `training/training/envs/factory.py` | Environment creation with wrapper chain |
| `training/tests/test_action_wrappers.py` | Unit tests for all action wrappers |

## Files to Modify

| File | Change |
|------|--------|
| `training/training/envs/ros_env.py` | Add action masking logic in step(), document threshold encoding |
| `training/training/models/config.py` | Add action config fields (velocity_limits, smoothing params, etc.) |
| `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp` | Add SafetyController member, velocity limits, carrying state cache |
| `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp` | Implement action scaling, safety integration, action masking |
| `ros_ws/src/warehouser_rl_bridge/CMakeLists.txt` | Add dependency on warehouser_safety package |
| `ros_ws/src/warehouser_rl_bridge/package.xml` | Add warehouser_safety dependency |
| `ros_ws/src/warehouser_msgs/srv/RLStep.srv` | Add safety_state, pick_success, place_success to response |
| `ros_ws/src/warehouser_simulation/include/warehouser_simulation/robot.hpp` | Remove hardcoded velocity limits, make configurable |
| `ros_ws/src/warehouser_simulation/src/simulation_node.cpp` | Track and return pick/place success in callbacks |
| `CLAUDE.md` | Document action space contract and normalization convention |

## Architecture Notes

### Design Decisions

**1. Wrapper Chain Pattern**
- Use Gymnasium's wrapper pattern for composability
- Each wrapper has single responsibility
- Easy to enable/disable features via configuration
- Testable in isolation

**2. Scaling Location: Python vs C++**
- DECISION: Scale in Python wrappers, NOT in RLBridge
- RATIONALE: Python wrappers are easier to test, configure, and swap
- RLBridge should still apply safety as final check
- Alternative: Could scale in RLBridge, but less flexible

**3. Discrete Action Encoding**
- KEEP: Continuous encoding with threshold (simpler for PPO)
- REJECT: Dict or MultiDiscrete spaces (would require policy changes)
- DOCUMENT: threshold=0.5, action masking prevents invalid states

**4. Safety Controller Integration**
- RLBridge instantiates and calls SafetyController
- Safety interventions visible to policy via observation
- Policy learns to avoid triggering safety layer

**5. Action Smoothing: EMA vs Low-Pass**
- START WITH: EMA (simpler, no scipy dependency)
- FUTURE: Low-pass Butterworth if needed for more aggressive filtering

### Modularity Principles

**Separation of Concerns:**
- Python wrappers: Action preprocessing (scaling, smoothing, acceleration)
- RLBridge: Safety enforcement, ROS communication
- Simulation: Physics execution, ground truth

**Configuration Over Code:**
- All parameters configurable via EnvConfig
- Easy to experiment with different settings
- Default values from research best practices

**Testability:**
- Each wrapper independently testable
- Mock environments for unit tests
- Integration tests for full pipeline

### Future Extensions

**Phase 6: Hierarchical Skills (Post-MVP)**
- Options framework for temporal abstraction
- Skill primitives: navigate_to_goal, pickup_object, scan_area
- High-level policy selects skills, low-level policy executes
- Enables long-horizon warehouse tasks

**Phase 7: Advanced Safety**
- Predictive safety (trajectory forecasting)
- Recovery behaviors (back up if stuck)
- Dynamic safety bounds based on task

**Phase 8: Domain Randomization**
- Action noise injection for robustness
- Vary smoothing parameters across episodes
- Simulate actuator delays and dynamics variations

## Verification

### Unit Tests

- [ ] ActionScalingWrapper scales correctly to physical limits
- [ ] ActionSmoothingWrapper applies EMA filter (alpha=0.3)
- [ ] AccelerationLimitWrapper enforces acceleration bounds
- [ ] SafetyClippingWrapper clips to hard limits
- [ ] Wrapper chain composes correctly
- [ ] Reset clears wrapper state

### Integration Tests

- [ ] RLBridge scales actions from [-1,1] to velocity limits
- [ ] SafetyController called before publishing cmd_vel
- [ ] Discrete actions respect threshold=0.5
- [ ] Action masking prevents pick while carrying
- [ ] Action masking prevents place while not carrying
- [ ] Safety state returned in observation

### System Tests

- [ ] Full episode runs without crashes
- [ ] Actions produce smooth trajectories (visual inspection)
- [ ] Emergency stop triggers on close obstacles
- [ ] Pick/place success flags accurate
- [ ] Multi-robot action routing works (robot_id > 0)

### Acceptance Criteria

- [ ] Actions explicitly scaled from [-1,1] to physical limits
- [ ] SafetyController integrated into RLBridge action execution
- [ ] Action smoothing reduces trajectory jerkiness (measure acceleration variance)
- [ ] Acceleration limiting enforces realistic dynamics
- [ ] Discrete actions masked based on is_carrying state
- [ ] Pick/place success feedback in observation
- [ ] All unit tests pass
- [ ] Integration tests pass
- [ ] Documentation updated (CLAUDE.md)
- [ ] Code follows C++23 and Python type standards

## Priority Order

**Critical Path (Must Have):**
1. Phase 1: Python action wrappers (scaling, smoothing, acceleration)
2. Phase 2: RLBridge safety integration
3. Phase 5: Testing and documentation

**Important (Should Have):**
4. Phase 3: Discrete action feedback
5. Phase 4: Multi-robot action routing

**Future Work (Nice to Have):**
6. Hierarchical skills (options framework)
7. Advanced safety (predictive, recovery)
8. Domain randomization for actions

## References

- S.md: 13+ papers on action space design, hybrid spaces, sim-to-real transfer
- I.md: Deep code analysis of current implementation gaps
- T.md: Production-ready Gymnasium wrapper templates
- Best practice: Joint velocity spaces for sim-to-real (arXiv 2312.03673)
- Best practice: Action smoothing critical for realistic motion (arXiv 2502.14457)
