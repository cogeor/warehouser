# Introspect: Observation Space Design

Created: 2026-02-12 22:19:09

## Focus

Analysis of the observation system architecture in the warehouser codebase, examining all observation versions (V1, V2, V3), data flow from simulation to training, normalization practices, and gaps compared to RL best practices.

## Architecture Overview

The observation system is cleanly separated into three layers:
1. **Simulation Layer** (C++ ROS2) - ObservationBuilder builds observations from WorldState
2. **Transport Layer** (ROS2 Services) - RLReset/RLStep services carry Observation messages
3. **Training Layer** (Python) - Gymnasium environments consume observations as numpy arrays

### Key Files and Roles

**C++ Observation Building:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\include\warehouser_observations\observation_builder.hpp` - Defines ObservationVersion enum and ObservationBuilder class
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observation_builder.cpp` - Implements V1 and V3 observation generation
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\include\warehouser_observations\observations_node.hpp` - ROS2 node that publishes observations
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observations_node.cpp` - Node implementation with service handlers

**Sensor Simulation:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\include\warehouser_observations\lidar_simulator.hpp` - Lidar raycast simulation (60 rays, 180° FOV)
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\lidar_simulator.cpp` - Implements raycast with noise model
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\include\warehouser_observations\odometry_simulator.hpp` - Odometry with drift noise
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\odometry_simulator.cpp` - Computes dx/dy/dtheta with Gaussian noise

**Noise/Domain Randomization:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\include\warehouser_observations\noise_model.hpp` - NoiseModel with Gaussian + dropout
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\include\warehouser_observations\sensor_interface.hpp` - ISensor interface for polymorphic sensors

**Python Training Environments:**
- `C:\Users\costa\src\warehouser\training\training\envs\ros_env.py` - Single-agent Gymnasium environment
- `C:\Users\costa\src\warehouser\training\training\envs\pettingzoo_env.py` - Multi-agent PettingZoo environment
- `C:\Users\costa\src\warehouser\training\training\models\config.py` - Pydantic configs with obs_dim validation

**Message Definitions:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\Observation.msg` - Observation message (version + float32[])
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\srv\RLReset.srv` - Returns initial observations
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\srv\RLStep.srv` - Returns observation after step

**Configuration:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\config\observations_params.yaml` - ROS params for observation version, rates, lidar config

## Observation Versions

### V1_Position (8 dimensions) - DEFAULT, FULLY IMPLEMENTED

**Contents (in order):**
1. `robot_x` - Absolute X position in world frame (meters)
2. `robot_y` - Absolute Y position in world frame (meters)
3. `robot_theta` - Absolute heading in world frame (radians, [-π, π])
4. `goal_dx` - Goal X delta (goal.x - robot.x)
5. `goal_dy` - Goal Y delta (goal.y - robot.y)
6. `goal_dist` - Euclidean distance to goal: sqrt(dx² + dy²)
7. `goal_heading` - Goal bearing in robot frame (radians, [-π, π])
8. `is_carrying` - Binary flag (0.0 or 1.0)

**Implementation:** `observation_builder.cpp:42-82` (buildV1)

**Data Flow:**
- WorldState.entities[robot_index] provides robot pose
- Goal message provides goal position
- goal_heading computed as: atan2(dy, dx) - robot.theta (normalized)

**Coordinate Systems:**
- Absolute positions in world frame (REP-103: X forward, Y left, Z up)
- Goal heading transformed to robot's egocentric frame

**Bounds:** No clipping applied - observation space is Box(low=-inf, high=inf)

**Use Case:** Position-based policy training (no vision)

**Strengths:**
- Simple, interpretable
- Low dimensional (fast training)
- Contains essential task information

**Weaknesses:**
- Absolute positions break generalization across different world sizes
- No obstacle information (relies on collision penalty from reward)
- Goal heading redundant with goal_dx/goal_dy (over-specified)

### V2_Lidar (63 dimensions) - SPECIFIED BUT NOT IMPLEMENTED

**Planned Contents (from comments in observation_builder.hpp:18-20):**
1-60. `lidar_ranges[60]` - 60 range measurements from 180° FOV lidar
61. `goal_bearing` - Goal bearing relative to robot
62. `goal_dist` - Distance to goal
63. `is_carrying` - Binary flag

**Status:** Partially stubbed in `observation_builder.cpp:17-20` - falls back to V1

**Lidar Simulator EXISTS and is FUNCTIONAL:**
- LidarSimulator fully implemented in `lidar_simulator.cpp`
- Used for visualization (frontend) and SLAM compatibility
- Publishes to `/observations/lidar_debug` and `/scan` topics
- NOT integrated into observation vector

**Lidar Configuration:**
- 60 rays, 180° FOV (π radians)
- Max range: 10.0m, min range: 0.1m
- Raycast step size: 0.05m (5cm resolution)
- Noise model available: range_stddev=0.02m, dropout_prob=0.01

**Noise/Domain Randomization (available but not used):**
- Gaussian range noise (σ = 2cm)
- 1% dropout probability (returns max_range on dropout)
- Configurable via LidarNoiseConfig

**Gap:** V2 would provide spatial awareness but is not connected to training

### V3_MultiRobot (8 + 3×max_other_robots dimensions) - FULLY IMPLEMENTED

**Contents:**
1-8. Ego state (identical to V1):
   - robot_x, robot_y, robot_theta
   - goal_dx, goal_dy, goal_dist, goal_heading
   - is_carrying

9+. Other robots (3 dims per robot, up to max_other_robots):
   - rel_x: X position in ego robot's frame
   - rel_y: Y position in ego robot's frame
   - rel_theta: Heading relative to ego robot

**Implementation:** `observation_builder.cpp:84-139` (buildV3)

**Configuration:** max_other_robots parameter (default 3)

**Default Dimension:** 8 + 3×3 = 17 (for max_other_robots=3)

**Coordinate Transform:**
```cpp
// World-frame delta
float world_dx = other->x - ego->x;
float world_dy = other->y - ego->y;
// Transform to ego's frame (rotation by -ego->theta)
float cos_ego = std::cos(-ego->theta);
float sin_ego = std::sin(-ego->theta);
rel_x = cos_ego * world_dx - sin_ego * world_dy;
rel_y = sin_ego * world_dx + cos_ego * world_dy;
rel_theta = normalizeAngle(other->theta - ego->theta);
```

**Zero Padding:** If fewer robots than max_other_robots, remaining slots filled with zeros

**Multi-Agent Support:**
- Each robot gets its own observation (per-robot perspective)
- PettingZoo environment (`pettingzoo_env.py`) uses V3 with robot_id parameter
- RLReset returns observations[] array (one per robot)
- RLStep takes robot_id and returns that robot's observation

**Use Case:** Multi-agent coordination, collision avoidance between robots

**Strengths:**
- Egocentric frame (better for policy generalization)
- Supports decentralized MARL training
- Fixed-size observation (zero padding)

**Weaknesses:**
- Still includes absolute ego position (breaks generalization)
- No attention mechanism (permutation variance issue)
- max_other_robots is a hard limit (doesn't scale to arbitrary robot counts)

## Data Flow: Simulation → Training

### 1. World State Generation
- Simulation maintains WorldState with entities (type, x, y, theta, is_carrying)
- Published to `/world/state` topic

### 2. Observation Building (C++)
- ObservationsNode subscribes to `/world/state` and `/task/goal`
- Timer callback (20 Hz) calls `builder_.build(last_world_, last_goal_, robot_index)`
- ObservationBuilder::build() dispatches to buildV1/buildV2/buildV3 based on version
- Returns Observation message (int32 version + float32[] data)

### 3. Service Transport
- RLReset service: Returns initial observation(s) after reset
- RLStep service: Takes action, returns next observation
- Observations passed as `warehouser_msgs/Observation` messages

### 4. Python Consumption
- `ros_env.py` calls RLReset/RLStep via rclpy service clients
- Extracts observation.data as numpy array:
  ```python
  obs = np.array(response.observation.data, dtype=np.float32)
  ```
- Validates dimension matches config.obs_dim (raises error if mismatch)

### 5. Gymnasium Space Definition
- `ros_env.py:47-49`:
  ```python
  self.observation_space = gym.spaces.Box(
      low=-np.inf, high=np.inf, shape=(self.config.obs_dim,), dtype=np.float32
  )
  ```
- obs_dim from EnvConfig (default 8 for V1, 17 for V3 multi-agent)

## Normalization Status - CRITICAL GAP

### Current State: NO NORMALIZATION

**Evidence:**
1. Observation space bounds: `low=-np.inf, high=np.inf` (unbounded)
2. No VecNormalize wrapper in `train.py`
3. Grep for "normalize" only found in config.py (theta normalization, not observation normalization)
4. Raw values passed directly to policy network

**Implications:**
- Absolute positions (V1: robot_x, robot_y) range [0, 10] for 10m world
- Distances range [0, ~14.14] (diagonal of 10x10 world)
- Angles range [-π, π] ≈ [-3.14, 3.14]
- Binary flag: {0.0, 1.0}

**Problem:** Input features have vastly different scales
- robot_x ∈ [0, 10]
- goal_heading ∈ [-3.14, 3.14]
- is_carrying ∈ {0, 1}

This causes:
1. Slow neural network training (gradient imbalance)
2. Sensitivity to world_size parameter
3. Poor generalization to different world configurations

### Best Practice Gap: VecNormalize

**Standard approach in SB3:**
```python
from stable_baselines3.common.vec_env import VecNormalize

env = DummyVecEnv([lambda: make_env(env_config)])
env = VecNormalize(env, norm_obs=True, norm_reward=False)
```

**VecNormalize behavior:**
- Tracks running mean and stddev of observations
- Normalizes to zero mean, unit variance
- Clips normalized values to [-10, 10] by default
- Saves statistics with model for deployment

**Current code (`train.py:112`):**
```python
env = DummyVecEnv([lambda: make_env(env_config)])
# No VecNormalize!
```

### Manual Normalization Option

Could normalize in ObservationBuilder:
```cpp
// In V1, divide positions by world_size
obs.data[0] = robot->x / config_.world_size;  // → [0, 1]
obs.data[1] = robot->y / config_.world_size;  // → [0, 1]
obs.data[3] = dx / (config_.world_size * 1.414);  // Normalize by max possible distance
```

**Pros:**
- Explicit, deterministic
- No running statistics needed

**Cons:**
- Hardcoded to world_size
- Doesn't adapt to actual data distribution
- Still need to handle angle vs position scale differences

## Temporal Information - NOT PRESENT

### No History/Stacking

**Current:** Single timestep observation only
- No frame stacking (common in Atari RL)
- No action history
- No velocity information (except in odometry, not in observations)

**Implications:**
- Policy is reactive (no temporal context)
- Cannot infer velocities from position deltas
- Velocity control relies on direct odometry (if included in observation, which it's NOT in V1/V3)

**Markov Property:** Questionable
- Task may require knowing previous positions to navigate efficiently
- No memory of explored areas (unless learned in recurrent policy)

### Recurrent Policy Support

**Current PPO setup (`train.py:134`):**
```python
model = PPO("MlpPolicy", env, ...)  # MlpPolicy = feedforward MLP
```

**Options not used:**
- No LSTM policy (`RecurrentPPO` or `policy="MlpLstmPolicy"`)
- No frame stacking wrapper
- No FrameStack observation wrapper

**Gap:** For navigation tasks, recurrent policies or frame stacking often improve performance

## Sensor Observations

### Lidar - SIMULATED BUT NOT USED IN TRAINING

**Lidar Simulator (`lidar_simulator.cpp`):**
- Raycasting implementation with step size 0.05m
- Checks wall collision via AABB test
- Checks world bounds
- Returns vector of 60 range values

**Noise Model:**
- Gaussian noise: mean=0, stddev=0.02m (2cm)
- Dropout: 1% probability, returns max_range on dropout
- Configurable seed for reproducibility

**Publishing:**
- `/observations/lidar_debug` (custom LidarDebug message for frontend)
- `/scan` (sensor_msgs/LaserScan for SLAM tools)
- Rate: 10 Hz (configurable)

**Gap:** Lidar data is generated but NOT integrated into V2 observation vector

### Odometry - SIMULATED FOR PUBLISHING, NOT IN OBSERVATION

**Odometry Simulator (`odometry_simulator.cpp`):**
- Tracks previous pose, computes dx/dy/dtheta
- Noise proportional to motion (linear_stddev=0.01, angular_stddev=0.02)
- Published as nav_msgs/Odometry to `/odom` topic (50 Hz)

**Noise Model:**
- Linear noise: σ = 0.01 × distance_traveled
- Angular noise: σ = 0.02 × angle_rotated
- Covariance matrix included in Odometry message

**Gap:** Odometry reading (dx/dy/dtheta/velocities) NOT included in observation vector
- V1 has absolute positions, not velocities
- Policy cannot directly observe robot's velocity
- Must be inferred from position changes (if using frame stacking)

## Task Information

### Goal Encoding - INCLUDED (V1/V3)

**In V1/V3 observations:**
- goal_dx, goal_dy: Cartesian vector to goal
- goal_dist: Euclidean distance
- goal_heading: Bearing in robot frame

**Redundancy:** goal_dx/goal_dy fully specify the goal vector; goal_dist and goal_heading are derived
- Potential for conflicting information if computed inconsistently
- Over-specification may slow learning (network must learn they're redundant)

**Alternative:** Could use only polar (dist, heading) or only Cartesian (dx, dy)

### Task State - MINIMAL

**Only:** is_carrying flag (0.0 or 1.0)

**Missing:**
- Which object is carried (if multiple object types)
- Object goal color/type matching
- Task progress (e.g., how many objects delivered)
- Subgoal information

**Current design:** Single goal, single object type, binary carrying state

## Observation Space Definition

### Gymnasium Space (Python)

**V1 (Single Agent):**
```python
gym.spaces.Box(low=-np.inf, high=np.inf, shape=(8,), dtype=np.float32)
```

**V3 (Multi-Agent, max_other_robots=3):**
```python
gym.spaces.Box(low=-np.inf, high=np.inf, shape=(17,), dtype=np.float32)
```

**Issues:**
1. Unbounded space (low/high = ±inf) doesn't reflect actual ranges
2. No semantic structure (flat vector)
3. obs_dim hardcoded in config, must match builder version

### Data Type Consistency

**Good:** float32 used consistently
- C++ observation_builder.cpp uses float
- ROS message Observation.msg uses float32[]
- Python numpy array uses np.float32
- Gymnasium space dtype=np.float32

**REP-103 Compliance:**
- Theta normalized to [-π, π] via atan2(sin, cos)
- World frame: X forward, Y left, Z up
- Frontend flips Y for canvas (Y-down) - not relevant to observation

## Domain Randomization

### Noise Infrastructure - PRESENT BUT UNDERUTILIZED

**NoiseModel class (`noise_model.hpp`):**
- Gaussian noise with configurable mean/stddev
- Dropout with configurable probability
- Seed setting for reproducibility
- Applied to vectors in-place

**Sensor-Specific Configs:**
- LidarNoiseConfig: range_stddev=0.02m, dropout_prob=0.01
- OdomNoiseConfig: linear_stddev=0.01, angular_stddev=0.02

**Current Usage:**
- Lidar noise: Applied in lidar_simulator.cpp (if enabled)
- Odometry noise: Applied in odometry_simulator.cpp (if enabled)

**Gap:** Neither lidar nor odometry are in the observation vector
- Noise models exist but don't affect training
- V1 observations are raw positions from WorldState (no noise)

**Opportunity:**
- Add position noise to V1 observations
- Enable lidar/odom noise when V2 is implemented
- Use for sim-to-real transfer

## Testing Coverage

### Unit Tests (`test_observation_builder.cpp`)

**V1 Tests (comprehensive):**
- Dimension checks ✓
- Robot position extraction ✓
- Goal delta calculation ✓
- Distance and heading calculation ✓
- Carrying flag ✓
- Edge cases (no robot, invalid index) ✓

**V3 Tests (comprehensive):**
- Dimension with max_other_robots ✓
- Ego state matches V1 ✓
- Relative position transforms ✓
- Frame rotation correctness ✓
- Zero padding ✓
- Multi-perspective (different robot_id) ✓

**V2 Tests:** None (V2 not implemented)

**Integration Tests:**
- `test_observations_node.cpp` exists (not examined in detail)

**Python Tests:**
- `test_env.py` - Unit tests for ROSGymEnv
- `test_pettingzoo_env.py` - Tests for multi-agent env
- `test_gym_env.py` - Integration tests

## Findings

### Critical Issues

1. **`observation_builder.cpp:15-25` - V2_Lidar not implemented**
   - Falls back to V1 despite lidar infrastructure being ready
   - Lidar data generated but wasted (only for visualization)
   - No vision-based policy training possible

2. **`train.py:112` - No observation normalization**
   - Raw, unbounded observations fed to neural network
   - Different scales (positions [0,10], angles [-3.14,3.14], binary {0,1})
   - Slows training, reduces generalization

3. **`observation_builder.cpp:57-59` - Absolute positions break generalization**
   - V1 includes robot_x, robot_y in world frame
   - Policy learns world-size-specific behavior
   - Cannot transfer to different environments

4. **`ros_env.py:47-49` - Unbounded observation space**
   - Box(low=-inf, high=inf) doesn't reflect actual bounds
   - Could specify realistic bounds for better SB3 behavior

### Design Issues

5. **`observation_builder.cpp:62-76` - Redundant goal encoding**
   - Both Cartesian (dx, dy) AND polar (dist, heading) included
   - Increases observation dimension unnecessarily
   - Policy must learn redundancy

6. **No temporal information**
   - Single timestep observation (no history)
   - No velocity information in V1/V3
   - Odometry computed but not included in observation

7. **`observation_builder.cpp:89-116` - V3 uses absolute ego position**
   - Should be fully egocentric for generalization
   - First 8 dims include robot_x, robot_y (world coordinates)

8. **No attention mechanism for V3 multi-robot**
   - Fixed max_other_robots limit
   - Zero padding for fewer robots
   - Not permutation invariant (order matters)

### Infrastructure Gaps

9. **Noise models not applied to V1 observations**
   - Domain randomization ready but not used
   - Could add position/angle noise for robustness

10. **`config.py:100` - obs_dim hardcoded in MultiAgentConfig**
    - Default obs_dim=17 assumes max_other_robots=3
    - Fragile: changing max_other_robots breaks without updating obs_dim

### Documentation

11. **`Observation.msg:7-8` - V2 comment mentions "future"**
    - Indicates intent but not implemented
    - Misleading (lidar is actually functional, just not connected)

## Proposal

### High Priority: Enable Observation Normalization

**File:** `C:\Users\costa\src\warehouser\training\training\scripts\train.py`

**Change:** Wrap environment with VecNormalize after line 112:
```python
env = DummyVecEnv([lambda: make_env(env_config)])
env = VecNormalize(env, norm_obs=True, norm_reward=False, clip_obs=10.0)
```

**Rationale:**
- Zero mean, unit variance observations
- Faster, more stable training
- Standard practice in SB3

**Save/Load:** Update model save/load to include VecNormalize stats:
```python
model.save(path)
env.save(path.replace('.zip', '_vecnormalize.pkl'))
```

### Medium Priority: Implement V2_Lidar Observation

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observation_builder.cpp`

**Change:** Replace lines 17-20 with actual V2 implementation:
```cpp
case ObservationVersion::V2_Lidar:
    return buildV2(world, goal, robot_index);
```

**Add buildV2 method:**
```cpp
warehouser_msgs::msg::Observation ObservationBuilder::buildV2(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {

    warehouser_msgs::msg::Observation obs;
    obs.version = 2;
    obs.data.resize(63, 0.0f);

    const auto* robot = findRobotByIndex(world, robot_index);
    if (!robot) return obs;

    // Get lidar scan (reuse existing lidar_ simulator from node)
    // Note: requires LidarSimulator instance in ObservationBuilder
    auto ranges = lidar_.scan(robot->x, robot->y, robot->theta, world);
    std::copy(ranges.begin(), ranges.end(), obs.data.begin());

    // Goal bearing and distance
    float dx = goal.x - robot->x;
    float dy = goal.y - robot->y;
    float goal_angle = std::atan2(dy, dx);
    obs.data[60] = normalizeAngle(goal_angle - robot->theta);
    obs.data[61] = std::sqrt(dx*dx + dy*dy);
    obs.data[62] = robot->is_carrying ? 1.0f : 0.0f;

    return obs;
}
```

**Impact:** Enables vision-based policy training with obstacle awareness

### Medium Priority: Make V1/V3 Egocentric

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observation_builder.cpp`

**Change in V1 (lines 57-59):** Remove absolute positions, use only relative goal info:
```cpp
// REMOVE:
// obs.data[0] = robot->x;
// obs.data[1] = robot->y;
// obs.data[2] = robot->theta;

// REPLACE WITH egocentric version:
obs.data[0] = 0.0f;  // Ego always at origin in own frame
obs.data[1] = 0.0f;
obs.data[2] = 0.0f;  // Facing forward in own frame
```

**Adjust dimension to 5:** [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]

**Rationale:** Policy generalizes across world positions and orientations

**Breaking change:** Update obs_dim in config, retrain models

### Low Priority: Remove Redundant Goal Encoding

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observation_builder.cpp`

**Option A:** Keep only polar (dist, heading) - 2 dims
**Option B:** Keep only Cartesian (dx, dy) - 2 dims

**Recommendation:** Keep polar for navigation tasks (directly actionable)

**Impact:** Reduces V1 from 8 to 6 dimensions (if made egocentric: 3 dims)

### Low Priority: Add Velocity to Observations

**File:** Extend V1/V3 to include robot velocities

**Add to observation:**
- linear_velocity (v)
- angular_velocity (omega)

**Source:** Get from odometry_simulator or track in WorldState

**Benefit:** Policy can reason about dynamics, smoother control

### Low Priority: Apply Domain Randomization to V1

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observation_builder.cpp`

**Add position noise to V1:**
```cpp
if (config_.enable_noise) {
    obs.data[0] += position_noise_.apply(0.0f);  // Add ~N(0, σ) to x
    obs.data[1] += position_noise_.apply(0.0f);  // Add ~N(0, σ) to y
    obs.data[2] += angle_noise_.apply(0.0f);     // Add ~N(0, σ) to theta
}
```

**Configure via ObservationConfig:**
```cpp
struct ObservationConfig {
    // ... existing fields ...
    bool enable_noise = false;
    float position_stddev = 0.05f;  // 5cm noise
    float angle_stddev = 0.05f;     // ~3 degree noise
};
```

**Benefit:** Robustness to sensor noise, sim-to-real transfer

---

## Summary

The warehouser observation system has a solid architectural foundation with clean separation between simulation and training layers. V1 and V3 are fully implemented and tested. However, there are significant gaps:

1. **No normalization** - critical for training performance
2. **V2 lidar not connected** - infrastructure exists but unused
3. **Absolute positions** - breaks generalization
4. **No temporal information** - limits policy capabilities
5. **Noise models underutilized** - domain randomization not applied

Implementing VecNormalize is the highest-impact, lowest-effort improvement. Making observations egocentric and implementing V2 would substantially improve policy generalization and capability.
