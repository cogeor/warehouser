# Introspect: Safety and Collision Avoidance

Created: 2026-02-13

## Focus

Comprehensive analysis of safety systems, collision detection, and collision avoidance mechanisms in the Warehouser ROS2 warehouse robot simulation system.

## Executive Summary

Warehouser has a **well-designed but partially integrated** safety system. The `SafetyController` is implemented and tested, but is only integrated in the full system launch (not in training). Collision detection exists via simple wall collision checks and bounds checking, but there is **no collision avoidance** - only reactive collision response (rollback). Robot-robot collision detection is **absent**.

## Current Safety Architecture

### 1. SafetyController Implementation

**Location:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_safety\`

**Design Pattern:** Reactive safety layer as a ROS2 filter node between policy output and robot actuators.

**Core Components:**

#### SafetyController (`safety_controller.hpp/cpp`)
- **State Machine:** NOMINAL → SLOWDOWN → EMERGENCY → STOPPED
- **Forward cone monitoring:** ±60 degrees (1.05 radians) from heading
- **Emergency stop trigger:** Distance < 0.3m
- **Slowdown zone:** 0.3m - 0.8m with linear velocity scaling
- **Velocity limits:** max_linear=1.0 m/s, max_angular=2.0 rad/s

#### Key Algorithm (safety_controller.cpp:60-109)
```cpp
// Emergency stop condition
if (front_dist < config_.min_distance) {
    state_ = SafetyState::EMERGENCY;
    cmd_safe.linear = 0.0f;
    cmd_safe.angular = 0.0f;
    return cmd_safe;
}

// Slowdown with linear scaling
if (front_dist < config_.slowdown_distance) {
    state_ = SafetyState::SLOWDOWN;
    float scale = (distance - min_distance) / (slowdown_distance - min_distance);
    if (cmd_safe.linear > 0.0f) {
        cmd_safe.linear *= scale;
    }
}
```

**Gaps:**
- Line 92-100: Only checks forward motion safety, backward motion unchecked (no rear sensors assumed)
- Line 66-67: Hard-coded forward cone angle (±60°), not configurable
- No time-to-collision (TTC) calculation implemented (was in README spec but missing from code)
- No directional safety for rotation (was in README spec lines 158-166 but missing from implementation)

### 2. SafetyNode Integration

**Topic Architecture:**
- **Input:** `/cmd_vel_raw` (from policy/inference)
- **Input:** `/observations/lidar_debug` (for obstacle distances)
- **Output:** `/cmd_vel` (filtered safe commands)
- **Output:** `/safety/status` (state monitoring at 10 Hz)

**Critical Issue (safety_node.cpp:50-57):**
```cpp
if (lidar_received_) {
    cmd_safe = controller_.applySafetyLimits(cmd_raw, last_lidar_);
} else {
    // No lidar data - pass through but log warning
    cmd_safe = cmd_raw;
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                         "No lidar data received - safety checks disabled");
}
```
**Safety violation:** Commands pass through unchanged when lidar is unavailable. Should default to STOP, not pass-through.

### 3. Integration Status

**Integrated in:**
- `full_system.launch.py` (line 51-57): Safety node included in production deployment
- Test suite exists: `test_safety_controller.cpp` with 9 test cases

**NOT integrated in:**
- `training.launch.py`: **Safety node absent during RL training**
- RL Bridge does not route commands through safety layer
- Simulation publishes directly to `/cmd_vel`, bypassing safety

**Consequence:** Policies are trained **without safety constraints**, then safety is added at deployment. This creates a distribution mismatch - the policy never learns to operate within safety limits.

## Collision Detection Implementation

### 1. Wall Collision Detection

**Location:** `world_manager.cpp:206-213`

```cpp
bool WorldManager::checkCollision(float px, float py) const {
    for (const auto& wall : walls_) {
        if (wall->contains(px, py)) {
            return true;
        }
    }
    return false;
}
```

**Method:** Axis-Aligned Bounding Box (AABB) containment check
- Simple point-in-rectangle test
- Used for boundary walls (4 thin AABBs forming perimeter)

**Usage in Simulation (world_manager.cpp:115-127):**
```cpp
// Store previous position for collision rollback
float prev_x = robot->x;
float prev_y = robot->y;

robot->update(dt);

// Check collision and rollback if needed
if (checkCollision(robot->x, robot->y) || !isInBounds(robot->x, robot->y)) {
    robot->x = prev_x;
    robot->y = prev_y;
    robot->stop();
}
```

**Approach:** **Rollback on collision** - move robot, check collision, undo if collision occurred.

**Gaps:**
- Point-based collision check ignores robot radius (Robot::kRadius = 0.3m defined but unused)
- No swept volume collision detection (robot could tunnel through thin walls at high speed)
- No collision normal calculation (can't determine direction of collision for recovery)

### 2. Bounds Checking

**Location:** `world_manager.hpp:121-123`

```cpp
bool isInBounds(float px, float py) const {
    return px >= 0 && px <= config_.width && py >= 0 && py <= config_.height;
}
```

**Simple rectangular world bounds check.** Used alongside wall collision detection.

### 3. Robot-Robot Collision

**Status:** **NOT IMPLEMENTED**

Evidence:
- Multi-robot support added recently (git commits show V3_MultiRobot observations)
- `world_manager.cpp:114-137` only checks wall collisions, not robot-robot
- No spatial indexing or entity-entity collision system
- `Robot::contains()` method exists (robot.hpp:74-76) but is never called

**Implication:** Multiple robots can occupy the same space without detection or penalty.

### 4. Lidar-Based Collision Detection

**Location:** `lidar_simulator.cpp:109-138`

Lidar raycasting does detect obstacles:
```cpp
float LidarSimulator::raycast(float ox, float oy, float angle,
                               const warehouser_msgs::msg::WorldState& world) const {
    while (dist < config_.max_range) {
        float px = ox + dist * cos_a;
        float py = oy + dist * sin_a;

        if (checkWallCollision(px, py, world)) {  // Line 125
            return dist;
        }

        if (!isInBounds(px, py, world_width, world_height)) {  // Line 130
            return dist;
        }

        dist += config_.step_size;  // Raycast step = 5cm
    }
    return config_.max_range;
}
```

**Lidar detects:**
- Wall collisions (AABB checks)
- World bounds

**Lidar does NOT detect:**
- Other robots (no entity type filtering for TYPE_ROBOT)
- Pickable objects (could be obstacles but ignored)
- Dynamic obstacles

## Velocity Limits and Safety Margins

### 1. Robot Physical Limits

**Location:** `robot.hpp:31-33`
```cpp
static constexpr float kVMax = 1.0f;      // Max linear velocity (m/s)
static constexpr float kOmegaMax = 2.0f;  // Max angular velocity (rad/s)
static constexpr float kRadius = 0.3f;    // Robot radius for collision (m)
```

**Enforced in:** `robot.hpp:53-56`
```cpp
void setCommand(float linear, float angular) {
    v = std::clamp(linear, -kVMax, kVMax);
    omega = std::clamp(angular, -kOmegaMax, kOmegaMax);
}
```

### 2. Safety Layer Limits

**Configured in:** `safety_params.yaml`
```yaml
safety:
  ros__parameters:
    min_distance: 0.3      # Emergency stop threshold (m)
    slowdown_distance: 0.8 # Slowdown zone start (m)
    max_linear_vel: 1.0    # Max allowed linear velocity (m/s)
    max_angular_vel: 2.0   # Max allowed angular velocity (rad/s)
```

**Safety margin:** 0.3m emergency stop distance equals robot radius (0.3m) - **zero safety buffer**.

**Industry standard:** Typically 1.5-2x robot radius for safety buffer.
- Recommended min_distance: 0.45-0.6m for 0.3m radius robot
- Current setting allows collision before emergency stop triggers

### 3. Acceleration Limits

**Status:** **NOT IMPLEMENTED**

- No maximum acceleration/deceleration constraints
- Robot can change from full forward to full reverse instantaneously
- Unrealistic dynamics could lead to:
  - Carried object physics violations
  - Overly aggressive policies that fail on real hardware

## RL Training Safety

### 1. Collision Penalties

**Location:** `training/models/config.py:58`
```python
collision_penalty: float = Field(default=-100.0, description="Penalty for collision")
```

**Implementation:** `reward_strategy.cpp:85-98`
```cpp
RewardResult CollisionRewardStrategy::calculate(const RewardContext& ctx) const {
    const auto* curr_robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);

    if (!curr_robot) {
        // Robot not found indicates collision/failure
        result.terminated = true;
        result.termination_reason = "Robot not found";
        result.reward = config_.collision_penalty;
    }

    return result;
}
```

**Detection method:** Collision is inferred when robot entity disappears from world state.

**Issue:** This never triggers in current implementation because:
1. `world_manager.cpp:123-127` does rollback, robot never disappears
2. Collision detection doesn't remove robot from world state
3. Penalty is **never applied** during training

**Actual collision handling during training:** Robot position rollback with zero penalty beyond time penalty.

### 2. Safety During Training

**Current approach:**
- Robot hits wall → position rollback → continues episode
- No explicit collision termination
- No collision counter or cumulative penalty
- Episode only terminates on:
  - Goal reached (success_bonus = +100)
  - Max steps exceeded (truncated)
  - Time accumulation penalty (-0.1 per step)

**Consequence:** Policy learns that collisions are free (no cost beyond time). This encourages risky navigation.

### 3. Constraint Handling

**Status:** **NO CONSTRAINED RL MECHANISMS**

- No Constrained Policy Optimization (CPO)
- No Lagrangian relaxation for safety constraints
- No safety budget tracking
- PPO with collision penalty is theoretically sufficient, but penalty is not applied

## Recovery and Episode Termination

### 1. Collision Recovery

**Current behavior (world_manager.cpp:123-127):**
```cpp
if (checkCollision(robot->x, robot->y) || !isInBounds(robot->x, robot->y)) {
    robot->x = prev_x;
    robot->y = prev_y;
    robot->stop();  // Sets v=0, omega=0
}
```

**Recovery strategy:** Rollback to previous valid position and stop.

**Issues:**
- Robot can get stuck oscillating at boundary (try to move → collide → rollback → repeat)
- No recovery maneuver (e.g., back up and turn)
- `robot->stop()` clears velocity but policy immediately applies new command next step

### 2. Episode Termination

**Termination conditions (reward_strategy.cpp):**

1. **Goal reached:** Distance to goal < 0.5m (line 52-56)
2. **Max steps:** 500 steps default (time_penalty.cpp:114)
3. **Robot not found:** Never actually triggers (line 91-94)

**Missing termination conditions:**
- Excessive collision count (e.g., 3 collisions = episode end)
- Stuck detection (position unchanged for N steps)
- Velocity violations (attempted unsafe speeds)

### 3. Reset Behavior

**Location:** `world_manager.cpp:81-106`

```cpp
void WorldManager::reset() {
    sim_time_ = 0.0f;
    running_ = false;

    // Reset all robots to initial configurations
    for (size_t i = 0; i < robots_.size(); ++i) {
        const auto& config = initial_robot_configs_[i];
        robots_[i]->x = config.x;
        robots_[i]->y = config.y;
        robots_[i]->theta = config.theta;
        robots_[i]->v = 0.0f;
        robots_[i]->omega = 0.0f;
        robots_[i]->is_carrying = false;
        robots_[i]->carried_object_id.clear();
    }

    // Reset objects to initial positions
    // ...
}
```

**Clean reset:** All state properly reset to initial conditions. No residual state issues.

## Sensor Processing for Safety

### 1. Lidar Configuration

**Location:** `lidar_simulator.hpp:18-24`
```cpp
struct LidarConfig {
    int num_rays = 60;
    float fov = 3.14159265f;  // 180 degrees (π radians)
    float max_range = 10.0f;
    float min_range = 0.1f;
    float step_size = 0.05f;  // Raycast step resolution (5cm)
    LidarNoiseConfig noise;
};
```

**Coverage:** 180° forward-facing sensor (±90° from heading)
- No rear coverage for backward motion safety
- 60 rays = 3° angular resolution
- 10m max range, 0.1m min range

### 2. Noise Model for Domain Randomization

**Location:** `lidar_simulator.cpp:9-17`
```cpp
NoiseConfig noise_cfg;
noise_cfg.mean = 0.0f;
noise_cfg.stddev = config.noise.range_stddev;
noise_cfg.dropout_prob = config.noise.dropout_prob;
noise_cfg.dropout_value = config.max_range;  // Max range on dropout
noise_cfg.enabled = config.noise.enabled;
```

**Domain randomization support exists** but integration during training unknown.

### 3. Lidar Processing Frequency

**SafetyNode:** Subscribes to `/observations/lidar_debug` at queue depth 10
- Processes on every command received (`cmdRawCallback`)
- Uses most recent lidar scan (`last_lidar_`)
- Potential race condition if lidar update rate < command rate

**No timestamp checking:** Could use stale lidar data without detection.

## Gap Analysis

### Critical Safety Gaps

| Gap | Severity | Impact |
|-----|----------|--------|
| Safety not integrated in training | **CRITICAL** | Policy never learns safety constraints, distribution mismatch at deployment |
| Collision penalty never applied | **CRITICAL** | No cost for collisions during training, encourages risky behavior |
| Robot-robot collision absent | **HIGH** | Multi-robot scenarios unsafe |
| Zero safety margin (0.3m) | **HIGH** | No buffer before emergency stop, collision likely |
| Lidar passthrough on failure | **HIGH** | Unsafe fallback behavior |
| No TTC calculation | **MEDIUM** | Can't predict future collisions, only reacts to current state |
| Point-based collision | **MEDIUM** | Ignores robot radius, tunneling possible |
| No acceleration limits | **MEDIUM** | Unrealistic dynamics |
| Stuck detection absent | **LOW** | Can waste episode time oscillating |

### Missing Collision Avoidance

**Status:** NO COLLISION AVOIDANCE IMPLEMENTED

Current system is **purely reactive:**
- SafetyController: Reacts to current obstacle distance (slowdown/stop)
- WorldManager: Reacts to collision by rollback

**No predictive avoidance:**
- No Dynamic Window Approach (DWA)
- No Vector Field Histogram (VFH)
- No Optimal Reciprocal Collision Avoidance (ORCA)
- No path planning integration
- No local costmap

**Consequence:** Robot will approach obstacles until triggering slowdown/emergency stop, then become stuck. No path replanning or avoidance maneuver.

### ISO 3691-4 Safety Standards Gap

ISO 3691-4 (Industrial trucks - Safety requirements for automated guided vehicles):

**Required but missing:**
1. **Safety zones:** Only emergency stop zone (0-0.3m), should have warning zone, safety zone, detection zone
2. **Speed reduction curves:** Linear scaling implemented, but should be configurable for different environments
3. **Emergency stop time:** No time constraints, instantaneous stop assumed
4. **Obstacle classification:** All obstacles treated equally, no priority for humans vs static objects
5. **Safety-rated sensors:** No sensor health monitoring or redundancy
6. **Fail-safe behavior:** Passthrough on sensor failure is not fail-safe

## Specific File Roles Summary

### Safety System Files

| File | Role | Status |
|------|------|--------|
| `warehouser_safety/include/warehouser_safety/safety_controller.hpp` | SafetyController interface | Implemented |
| `warehouser_safety/src/safety_controller.cpp` | Core safety logic (state machine, scaling) | Implemented, gaps noted |
| `warehouser_safety/include/warehouser_safety/safety_node.hpp` | ROS2 node wrapper | Implemented |
| `warehouser_safety/src/safety_node.cpp` | ROS topic integration | Implemented, unsafe fallback |
| `warehouser_safety/config/safety_params.yaml` | Safety parameters | Configured, margins too tight |
| `warehouser_safety/test/test_safety_controller.cpp` | Unit tests | 9 tests, good coverage |

### Collision Detection Files

| File | Role | Status |
|------|------|--------|
| `warehouser_simulation/include/warehouser_simulation/world_manager.hpp` | Collision check interface | Implemented |
| `warehouser_simulation/src/world_manager.cpp` | Wall collision, bounds check, rollback | Implemented, point-based only |
| `warehouser_simulation/include/warehouser_simulation/robot.hpp` | Robot radius constant | Defined but unused for collision |
| `warehouser_observations/src/lidar_simulator.cpp` | Obstacle detection via raycasting | Implemented, walls only |

### Training Integration Files

| File | Role | Status |
|------|------|--------|
| `warehouser_rl_bridge/src/reward_strategy.cpp` | Collision penalty calculation | Implemented but never triggers |
| `warehouser_rl_bridge/config/rl_bridge_params.yaml` | Reward config including collision_penalty | Configured (-100.0) |
| `training/models/config.py` | Training config with collision_penalty | Configured |
| `warehouser_bringup/launch/training.launch.py` | Training launch file | **Safety node not included** |
| `warehouser_bringup/launch/full_system.launch.py` | Production launch file | Safety node included (line 51-57) |

## Proposal

### Immediate Fixes (P0 - Before Next Training Run)

1. **Integrate safety in training loop:**
   - Add safety node to `training.launch.py`
   - Route RL Bridge commands through `/cmd_vel_raw` → safety → `/cmd_vel` → simulation
   - Ensures policy learns within safety constraints

2. **Fix collision penalty:**
   - Modify `world_manager.cpp` to track collision count per robot
   - Add collision event to world state message
   - Wire collision event to reward calculator
   - Verify -100 penalty is applied

3. **Fix lidar fallback:**
   - Change `safety_node.cpp:54` from passthrough to full stop when lidar unavailable
   - Add timeout check for stale lidar data (e.g., > 200ms old)

4. **Increase safety margin:**
   - Change `min_distance` from 0.3m to 0.5m (1.66x robot radius)
   - Provides safety buffer before emergency stop

### High Priority (P1 - Next Sprint)

5. **Implement robot-robot collision:**
   - Add spatial hash or quadtree for efficient entity queries
   - Check robot-robot distance in `world_manager.cpp:step()`
   - Use sum of radii for collision threshold (0.6m for two 0.3m robots)

6. **Add collision-based termination:**
   - Terminate episode after 3 collisions
   - Prevents policy from learning to ignore collisions
   - Add collision count to info dict for analysis

7. **Implement TTC calculation:**
   - Add `estimateTTC()` method per README spec
   - Consider velocity projection onto obstacle direction
   - Emergency stop if TTC < 1.0s even if distance > min_distance

### Medium Priority (P2 - Future)

8. **Add acceleration limits:**
   - Max linear acceleration: 1.0 m/s²
   - Max angular acceleration: 2.0 rad/s²
   - Smooth velocity commands over multiple timesteps

9. **Upgrade to radius-based collision:**
   - Use `Robot::kRadius` in collision detection
   - Implement swept circle collision check
   - Prevents tunneling at high speeds

10. **Add collision avoidance:**
    - Implement DWA for local planning
    - Or train avoidance into policy by ensuring collision penalty is learned
    - Requires safety integration in training (#1)

11. **Sensor health monitoring:**
    - Track lidar data age
    - Detect sensor dropout patterns
    - Publish sensor health status
    - Graceful degradation instead of passthrough

### Low Priority (P3 - Polish)

12. **Stuck detection and recovery:**
    - Track position history (last 10 steps)
    - Detect stuck state (position variance < threshold)
    - Terminate episode or trigger recovery maneuver

13. **ISO 3691-4 compliance:**
    - Multiple safety zones (detection, warning, protection)
    - Configurable speed reduction curves
    - Emergency stop timing validation
    - Obstacle classification system

## Conclusion

Warehouser has a **solid foundation for safety** with a well-structured SafetyController and comprehensive unit tests. However, **critical integration gaps** prevent safety from being effective:

1. Safety is bypassed during training (policy never learns constraints)
2. Collision penalties are configured but never applied
3. Multi-robot collision detection is absent
4. Safety margins are too tight

The architecture is sound and fixing these issues is straightforward - primarily integration work rather than redesign. Priority should be on integrating safety into the training loop (P0 item #1) to ensure sim-to-real transfer success.

**Recommendation:** Complete P0 items before next training run, then progressively implement P1-P2 items for production-ready safety.
