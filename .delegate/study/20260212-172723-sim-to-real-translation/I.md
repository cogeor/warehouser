# Introspect: Sim-to-Real Readiness Analysis

Created: 2026-02-12

## Focus

Deep architectural analysis of the Warehouser codebase for sim-to-real translation readiness, assessing current abstraction layers, simulation implementation, sensor interfaces, and identifying critical gaps for real robot deployment.

## Executive Summary

Warehouser is a **simulation-first** ROS2 warehouse robot system with reinforcement learning training. The architecture is well-structured with clear separation of concerns, but **lacks critical hardware abstraction layers** needed for real robot deployment. The system currently operates entirely within a custom C++ simulation without ros2_control, making direct hardware deployment impossible without significant refactoring.

**Key Strengths:**
- Strong sensor abstraction via ISensor interface
- Domain randomization for lidar and odometry sensors
- Multi-robot support architecture
- Standard ROS2 message interfaces
- Modular observation system with versioning

**Critical Gaps:**
- No ros2_control hardware interface layer
- Direct velocity commands without controller abstraction
- Simulation-specific kinematics with no real motor driver path
- Missing URDF/XACRO robot description
- No launch-based configuration system for sim/real switching
- Limited domain randomization (sensors only, no physics)

## 1. Simulation Layer Analysis

### 1.1 Core Simulation Architecture

**Location:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\`

**Implementation:** Custom C++ simulation engine running at 50Hz (20ms timestep)

**Key Components:**

#### Entity System (`entity.hpp`, `robot.hpp`, `pickable_object.hpp`, `wall.hpp`, `zone.hpp`)
```
Entity (abstract base)
├── Robot (differential drive kinematics)
├── PickableObject (stationary, pickable items)
├── Wall (static obstacles, AABB collision)
└── Zone (target areas for delivery)
```

**Physics Model:**
- **Kinematics:** Simple differential drive (x += v*cos(θ)*dt, y += v*sin(θ)*dt, θ += ω*dt)
- **Collision:** Basic geometric checks (AABB for walls, radius for robot)
- **No:** Dynamics, inertia, friction, motor lag, wheel slip
- **No:** Force-based simulation (direct velocity control)

**Configuration:** YAML-based world setup
- Location: `ros_ws/src/warehouser_bringup/config/world.yaml`
- Defines: Robot spawn, object positions, walls, zones
- **Gap:** No physics parameters (mass, friction, inertia) in config

#### Robot Entity (`robot.hpp:15-82`, `robot.cpp:1-56`)
```cpp
class Robot : public Entity {
    float theta, v, omega;           // State
    bool is_carrying;                // Manipulation state

    void update(float dt) {          // Direct kinematic integration
        x += v * cos(theta) * dt;
        y += v * sin(theta) * dt;
        theta = normalizeAngle(theta + omega * dt);
    }

    void setCommand(float linear, float angular) {
        v = clamp(linear, -kVMax, kVMax);   // Instant velocity change
        omega = clamp(angular, -kOmegaMax, kOmegaMax);
    }
};
```

**Critical Issue:** Instantaneous velocity changes — no acceleration, no motor lag, no inertia. Real robots cannot achieve this.

### 1.2 World Manager (`world_manager.cpp`, `world_manager.hpp`)

**Responsibilities:**
- Entity lifecycle management
- Simulation stepping
- Collision detection
- Pick/place action handling

**State Management:**
- Single robot instance (multi-robot support in progress)
- Vector of objects, walls, zones
- Fixed 50Hz simulation rate

**Services Exposed:**
- `/sim/start`, `/sim/pause`, `/sim/reset`
- `/sim/step` — Synchronous stepping for RL training

**Gap Analysis:**
- No parameter randomization (mass, friction, motor characteristics)
- No actuator modeling (torque limits, response curves)
- No environmental effects (floor friction, wheel slip)

### 1.3 Simulation Node (`simulation_node.cpp:1-150`)

**Topics Subscribed:**
- `/cmd_vel` (geometry_msgs/Twist) — Direct velocity commands from inference/RL

**Topics Published:**
- `/world/state` (WorldState) @ 50Hz — Full entity state
- `/clock` (Clock) — Simulation time

**Action Interface:**
- `/sim/pick`, `/sim/unpick` — Object manipulation triggers

**Issue:** Direct `/cmd_vel` subscription bypasses any controller layer. No abstraction for different motor drivers.

## 2. Observation Space Architecture

### 2.1 Sensor Abstraction Layer

**Location:** `ros_ws/src/warehouser_observations/`

**Excellent Design:** ISensor interface enables polymorphic sensor handling

```cpp
// sensor_interface.hpp:52-69
class ISensor {
    virtual SensorType type() const = 0;
    virtual SensorReading scan(
        const SensorPose& pose,
        const warehouser_msgs::msg::WorldState& world) const = 0;
};
```

**Implemented Sensors:**

#### LidarSimulator (`lidar_simulator.hpp:33-115`, `lidar_simulator.cpp:1-170`)
- **Purpose:** 2D raycast-based lidar simulation
- **Configuration:** 60 rays, 180° FOV, 10m max range
- **Output:** `sensor_msgs/LaserScan` (standard ROS2 message)
- **Domain Randomization:** Gaussian noise + dropout via `NoiseModel`
  - Range noise: σ = 2cm (configurable)
  - Dropout: 1% probability (configurable)
  - Max range on dropout
- **Raycast Implementation:** Simple 5cm step resolution against walls/bounds

**Strength:** Publishes standard `sensor_msgs/LaserScan`, making it swap-compatible with real lidars (Hokuyo, SICK, Velodyne).

#### OdometrySimulator (`odometry_simulator.hpp:17-64`, `odometry_simulator.cpp:1-94`)
- **Purpose:** Dead reckoning from pose changes
- **Noise Model:** Proportional to motion (1% linear, 2% angular)
- **Output:** OdometryReading with covariance
- **Reset:** Tracks previous pose, resets on episode start

**Strength:** Motion-proportional noise matches real odometry drift characteristics.

### 2.2 Observation Builder (`observation_builder.cpp:12-45`)

**Versioned Observation System:**
- **V1_Position:** [robot_x, robot_y, θ, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying] (8 dims)
- **V2_Lidar:** [60 lidar ranges + bearing + dist + carrying] (63 dims) — planned but not fully implemented
- **V3_MultiRobot:** V1 + relative positions of other robots (8 + 3*N dims)

**Current Training:** Uses V1_Position (privileged information)

**Gap for Sim-to-Real:**
- V1 uses ground-truth positions not available on real robots
- V2 (sensor-based) is the path to real deployment but incomplete
- Need to train with V2 before real hardware deployment

### 2.3 Noise Model (`noise_model.hpp:22-61`, `noise_model.cpp`)

**Implementation:**
- Gaussian noise with configurable mean/stddev
- Dropout with configurable probability
- Per-sensor noise configurations (LidarNoiseConfig, OdomNoiseConfig)
- Seeded RNG for reproducibility

**Strengths:**
- Well-structured domain randomization
- Configuration-driven noise parameters
- Applied at sensor level (good abstraction)

**Gap:**
- Noise only on sensors, not on physics or actuators
- No action delay randomization
- No systematic bias modeling (e.g., odometry drift accumulation)

## 3. Action Space and Control

### 3.1 Current Action Interface

**RL Action Space:** 4-dimensional continuous
```python
# training/envs/ros_env.py:40-43
action_space = Box(low=-1.0, high=1.0, shape=(4,), dtype=float32)
# [linear_vel, angular_vel, pick, place]
```

**Action Flow:**
```
Python RL Agent → RLStep service → RLBridgeNode → /cmd_vel topic → SimulationNode → Robot.setCommand()
```

**Critical Issue:** No controller layer between action and robot

**Real Robot Path (Missing):**
```
Should be: Policy → /cmd_vel → DiffDriveController (ros2_control) → HardwareInterface → Motor Drivers
```

### 3.2 RL Bridge (`rl_bridge_node.cpp:1-200`)

**Services:**
- `/rl/step` (RLStep.srv) — Apply action, get obs/reward/done
- `/rl/reset` (RLReset.srv) — Episode reset with seed

**Action Handling (Lines 191-200):**
```cpp
void RLBridgeNode::sendAction(size_t robot_id, float linear, float angular,
                               float pick, float place) {
    if (robot_id == 0) {
        geometry_msgs::msg::Twist cmd;
        cmd.linear.x = linear;
        cmd.angular.z = angular;
        cmd_pub_->publish(cmd);  // Direct velocity command
    }
    // Pick/place via /sim/pick, /sim/unpick topics
}
```

**Issue:** Direct velocity publishing, no abstraction for hardware

**Multi-Robot Support:** Partial implementation (reward calculators per robot, but single /cmd_vel topic)

### 3.3 Reward Calculation (`reward_calculator.cpp`)

**Reward Components:**
- Progress reward (distance reduction to goal)
- Collision penalty
- Success bonus (task completion)
- Pickup bonus
- Time penalty (per step)
- Smoothness penalty (penalizes action changes)

**Exploration Reward:** Coverage tracking via occupancy grid (`exploration_reward.cpp`, `occupancy_tracker.cpp`)

**Strength:** Modular, configurable reward components

**Gap:** No reward shaping for real-world constraints (energy efficiency, motor wear)

## 4. Hardware Abstraction Analysis

### 4.1 ros2_control Integration

**Current Status:** **NONE**

**Search Results:**
```bash
grep -r "ros2_control\|hardware_interface\|controller" ros_ws/
# Only found in safety package comments, not implemented
```

**Critical Gap:**
Warehouser has **no ros2_control hardware interface layer**. This is the standard ROS2 pattern for hardware abstraction.

**What's Missing:**

1. **Hardware Component Plugin:**
   ```cpp
   // Should exist but doesn't:
   class WarehouseHardwareInterface : public hardware_interface::SystemInterface {
       // on_init, on_configure, on_activate
       // read() — read encoder/sensor data from hardware
       // write() — send commands to motor drivers
   };
   ```

2. **URDF with ros2_control Tags:**
   ```xml
   <!-- Should exist but doesn't -->
   <ros2_control name="warehouse_robot" type="system">
       <hardware>
           <plugin>warehouser_control/WarehouseSimHardware</plugin>
       </hardware>
       <joint name="left_wheel_joint">
           <command_interface name="velocity"/>
           <state_interface name="position"/>
       </joint>
   </ros2_control>
   ```

3. **Controller Manager:** Not configured
4. **DiffDriveController:** Not used (ros2_controllers package)

**Impact:** Deploying to real hardware requires complete architectural refactoring, not just parameter changes.

### 4.2 Launch Configuration System

**Current Structure:**
```
warehouser_bringup/launch/
├── full_system.launch.py    # Simulation + inference + task + frontend
├── simulation.launch.py     # Simulation only
└── training.launch.py       # Simulation + RL bridge
```

**Configuration Files:**
```
warehouser_bringup/config/
└── world.yaml               # World entity definitions
```

**Analysis:**
- Single launch file for simulation
- No environment-specific configurations (sim vs real)
- No `use_sim_time` / `use_sim` toggle pattern
- Parameters hard-coded in launch files

**Standard Pattern (Missing):**
```python
DeclareLaunchArgument('use_sim', default_value='true'),
DeclareLaunchArgument('params_file',
    default_value=PathJoinSubstitution([
        FindPackageShare('warehouser_bringup'),
        'config', 'sim_params.yaml' if use_sim else 'real_params.yaml'
    ])
)
```

### 4.3 Sensor Interface Compatibility

**Strength:** Excellent sensor abstraction

**Lidar:**
- Outputs `sensor_msgs/LaserScan` (standard)
- Compatible with Nav2, SLAM Toolbox, any lidar-consuming node
- Can swap simulated lidar for real Hokuyo/SICK without code changes

**Odometry:**
- Outputs `OdometryReading` (custom struct)
- **Gap:** Should output `nav_msgs/Odometry` for Nav2 compatibility

**Missing Sensors for Real Robots:**
- IMU (accelerometer, gyroscope)
- Bumpers / contact sensors
- Battery state
- Emergency stop status

## 5. Message Definitions

### 5.1 Custom Messages

**Location:** `ros_ws/src/warehouser_msgs/`

**msg/:**
- `Entity.msg` — Generic entity representation
- `WorldState.msg` — Full world state (all entities)
- `Observation.msg` — Policy observation (float32[] + version)
- `LidarDebug.msg` — Debug lidar visualization
- `Goal.msg` — Task goal definition
- `TaskStatus.msg` — Task state machine status
- `Action.msg` — Policy action (linear, angular, pick, place)

**srv/:**
- `RLStep.srv` — RL environment step (request: action, response: obs/reward/done)
- `RLReset.srv` — RL environment reset (request: seed, response: initial obs)
- `SimStep.srv` — Synchronous simulation stepping
- `GetObservation.srv` — Request current observation
- `SetGoal.srv` — Set robot task goal
- `LoadModel.srv` — Load ONNX policy model

**Analysis:**

**Strengths:**
- Well-documented service definitions
- Multi-robot support in RLStep/RLReset (robot_id field)
- JSON info strings for extensibility

**Gaps:**
- Custom messages instead of standard ROS2 messages where possible
- `Observation.msg` should use `sensor_msgs` types
- No `nav_msgs/Odometry` usage
- No TF (transform) frame usage

**Sim-to-Real Impact:**
- Custom messages work equally in sim and real (good)
- But using standard messages improves interoperability with Nav2, SLAM, etc.

## 6. Training Pipeline

### 6.1 Python Environment Wrapper

**Location:** `training/training/envs/ros_env.py`

**Architecture:** Gymnasium environment wrapping ROS2 services

```python
class ROSGymEnv(gym.Env):
    def reset() → obs, info:
        # Calls /rl/reset service

    def step(action) → obs, reward, terminated, truncated, info:
        # Calls /rl/step service
```

**Communication:** ROS2 services (blocking calls)

**Observation Space:** Configurable via `EnvConfig`
- `obs_dim`: 8 (for V1_Position)
- Can be changed to 63 for V2_Lidar

**Action Space:** Box(low=-1, high=1, shape=(4,))

**Training Framework:** Stable-Baselines3 PPO

**ONNX Export:** `scripts/export_onnx.py` converts trained policy to ONNX for C++ inference

**Strength:** Clean separation of training (Python) and inference (C++ ONNX Runtime)

**Gap:** Training uses V1 (privileged ground-truth positions) — need V2 training before real deployment

### 6.2 Domain Randomization Status

**Currently Implemented:**
- Sensor noise (lidar, odometry) ✓
- Noise configuration via YAML ✓
- Seeded randomization for reproducibility ✓

**Missing for Sim-to-Real:**
- Physics randomization (mass, friction, inertia)
- Actuator randomization (motor torque, response time)
- Action delay randomization (50-100ms typical for real robots)
- Environmental randomization (floor texture, lighting if using vision)
- Automatic Domain Randomization (ADR)

**Recommendation from S.md:** Expand domain randomization to physics and dynamics, not just sensors.

## 7. Inference and Deployment

### 7.1 ONNX Inference (`ros_ws/src/warehouser_inference/`)

**InferenceNode:**
- Loads ONNX model via ONNX Runtime
- Subscribes to `/observations` topic
- Publishes to `/cmd_vel` (actions)
- Service: `/inference/load_model` for hot-reloading policies

**Strength:** C++ ONNX inference enables deployment on embedded platforms (Jetson, etc.)

**Issue:** Still publishes directly to /cmd_vel, bypassing controller layer

### 7.2 Task Manager (`ros_ws/src/warehouser_task/`)

**State Machine:** Pick-and-place task coordination

**Responsibilities:**
- Goal selection (closest object of target color)
- Task status tracking
- Goal publishing to `/task/goal`

**Gap:** Simulation-specific (uses ground-truth object positions from WorldState)

### 7.3 Safety Controller (`ros_ws/src/warehouser_safety/`)

**Purpose:** Collision avoidance override

**Implementation:** Monitors robot state, can override velocity commands

**Gap:** Needs lidar-based obstacle detection for real robot (currently uses ground-truth entity positions)

## 8. Code Organization

**Total Files:**
- C++ ROS2: 74 files (.cpp/.hpp)
- Python Training: ~40 files
- Configuration: 8 YAML files

**Package Structure:**
```
ros_ws/src/
├── warehouser_msgs/          # Message definitions ✓
├── warehouser_simulation/    # Custom simulation engine (no ros2_control)
├── warehouser_observations/  # Sensor abstraction ✓ (excellent)
├── warehouser_rl_bridge/     # RL training interface ✓
├── warehouser_inference/     # ONNX policy execution ✓
├── warehouser_task/          # Task state machine ✓
├── warehouser_command/       # JSON command parsing ✓
├── warehouser_safety/        # Safety override ✓
└── warehouser_bringup/       # Launch files (needs sim/real split)
```

**Missing Packages:**
- `warehouser_control/` — ros2_control hardware interface
- `warehouser_description/` — URDF/XACRO robot model
- `warehouser_randomization/` — Domain randomization configuration

## 9. Gap Analysis for Real Hardware Deployment

### 9.1 Critical Blockers (Must Fix Before Deployment)

| Issue | Current State | Required for Real Robot |
|-------|---------------|------------------------|
| **Hardware Abstraction** | Direct /cmd_vel, no ros2_control | ros2_control SystemInterface plugin |
| **Robot Description** | No URDF/XACRO | URDF with joints, links, sensors |
| **Motor Drivers** | Instant velocity changes | Acceleration limits, PID control |
| **Sensor Interface** | Custom WorldState | Standard sensor_msgs topics |
| **Controller Layer** | None | DiffDriveController, JointStateController |
| **Launch System** | Simulation-only | Sim/real toggle with environment configs |

### 9.2 High Priority (Sim-to-Real Transfer)

| Issue | Impact | Solution |
|-------|--------|----------|
| **Observation V1 Dependence** | Ground-truth positions unavailable | Train with V2_Lidar (sensor-based) |
| **Physics Randomization** | Reality gap in dynamics | Add mass/friction randomization |
| **Action Delays** | Real robots have 50-100ms lag | Randomize action delays in simulation |
| **Odometry Drift** | Real odometry accumulates error | Model systematic bias accumulation |
| **Collision Softness** | Sim collisions soft, real hard | Increase collision penalties, harder boundaries |

### 9.3 Medium Priority (Robustness)

| Issue | Impact | Solution |
|-------|--------|----------|
| **Motor Saturation** | Real motors have torque limits | Model motor torque curves |
| **Wheel Slip** | Floors vary (concrete, tile, carpet) | Add slip model with randomized friction |
| **Battery Degradation** | Performance degrades over time | Model voltage drop effects on motors |
| **Sensor Dropouts** | Real sensors occasionally fail | Increase dropout probability in training |
| **Emergency Stop** | Safety requirement | Hardware E-stop with software state machine |

## 10. Specific File-Level Findings

### Positive Implementations

1. **`sensor_interface.hpp:52-69`** — Excellent ISensor abstraction enabling sensor polymorphism
2. **`noise_model.hpp:22-61`** — Well-structured noise configuration with reproducibility
3. **`observation_builder.cpp:14-25`** — Versioned observation system for easy switching
4. **`rl_bridge_node.cpp:86-130`** — Multi-robot-aware RL stepping with per-robot rewards
5. **`lidar_simulator.cpp:63-92`** — Outputs standard sensor_msgs/LaserScan (Nav2 compatible)

### Issues Requiring Attention

1. **`simulation_node.cpp:104-106`** — Direct cmd_vel subscription
   ```cpp
   cmd_sub_ = create_subscription<Twist>("/cmd_vel", 10,
       [this](const Twist::SharedPtr msg) {
           world_.robot()->setCommand(msg->linear.x, msg->angular.z);
       });
   ```
   **Issue:** Bypasses controller layer

2. **`robot.hpp:43-48`** — Instantaneous velocity changes
   ```cpp
   void update(float dt) {
       x += v * cos(theta) * dt;  // No acceleration
       y += v * sin(theta) * dt;
       theta = normalizeAngle(theta + omega * dt);
   }
   ```
   **Issue:** Real motors cannot achieve instant velocity changes

3. **`rl_bridge_node.cpp:194-200`** — TODO comment acknowledges multi-robot action routing gap
   ```cpp
   // TODO: For multi-robot, need per-robot cmd_vel topics or action message
   // For now, use robot_id 0 for backward compatibility
   ```

4. **`world_manager.cpp`** — No physics parameter randomization (checked via grep)

5. **`observation_builder.cpp:17-20`** — V2 falls back to V1
   ```cpp
   case ObservationVersion::V2_Lidar:
       // V2 would be implemented similarly with lidar data
       // For now, fall back to V1
       return buildV1(world, goal, robot_index);
   ```
   **Issue:** Sensor-based observation (V2) not fully implemented

## 11. Architecture Assessment

### Strengths

1. **Modularity:** Clear separation between simulation, observation, RL, inference
2. **Sensor Abstraction:** ISensor interface enables sensor swap-ability
3. **Observation Versioning:** Easy to switch between ground-truth and sensor-based
4. **Multi-Robot Support:** Architecture supports N robots (partial implementation)
5. **ONNX Export:** Enables embedded deployment
6. **Domain Randomization:** Sensor noise models implemented
7. **Standard Messages:** Uses sensor_msgs/LaserScan for lidar

### Weaknesses

1. **No Hardware Abstraction Layer:** Biggest blocker for real deployment
2. **Missing ros2_control:** Industry-standard controller framework not used
3. **No URDF:** Robot description missing (needed for TF, visualization, physics)
4. **Direct Velocity Control:** No motor/controller modeling
5. **Limited Domain Randomization:** Only sensors, not physics/dynamics
6. **V1 Observation Dependence:** Training uses privileged information
7. **Launch System:** No sim/real environment switching

## 12. Recommendations

### Immediate (Current Sprint)

1. **Implement ros2_control Plugin:**
   - Create `warehouser_control` package
   - Implement `WarehouseSimHardware` (simulation backend)
   - Add URDF with ros2_control tags
   - Use `diff_drive_controller` from ros2_controllers

2. **Complete V2_Lidar Observation:**
   - Implement `buildV2()` in observation_builder
   - Train policy with V2 (sensor-based) observations
   - Benchmark against V1 performance

3. **Expand Domain Randomization:**
   - Add physics parameters to world config (mass, friction)
   - Randomize in world_manager during reset
   - Implement action delay buffer (0-100ms)

### Short-Term (Next 2 Sprints)

4. **Create Launch Configuration System:**
   - `sim_params.yaml` and `real_params.yaml`
   - `use_sim` argument in launch files
   - Conditional plugin loading (sim vs real hardware)

5. **Add URDF Robot Description:**
   - Define wheel joints, base_link, sensor frames
   - Integrate with robot_state_publisher
   - Enable TF tree for Nav2 integration

6. **Implement Standard Odometry:**
   - Output `nav_msgs/Odometry` from odometry simulator
   - Publish to `/odom` topic
   - Integrate with diff_drive_controller

### Medium-Term (Future Development)

7. **Real Hardware Interface Plugin:**
   - `WarehouseRealHardware` — communicates with motor drivers
   - Read encoder data, send velocity commands
   - Handle emergency stop signals

8. **Physics-Based Simulation:**
   - Add motor dynamics (torque curves, inertia)
   - Wheel slip modeling
   - Floor friction variations

9. **HIL Testing Setup:**
   - Run inference on target hardware (Jetson)
   - Connect to simulation via network
   - Validate control loop timing

10. **System Identification Pipeline:**
    - Safe data collection procedures
    - Parameter estimation from real trajectories
    - Automated simulation update workflow

## Conclusion

Warehouser has a **well-architected simulation and training system** with excellent sensor abstraction and modular design. However, it **lacks the critical hardware abstraction layer (ros2_control)** necessary for real robot deployment. The direct velocity command pattern and absence of motor/controller modeling create a significant reality gap.

**Primary Bottleneck:** No path from `/cmd_vel` topic to real motor drivers. Requires complete architectural refactoring to insert ros2_control layer.

**Secondary Issues:**
- Training on ground-truth observations (V1) instead of sensor-based (V2)
- Limited domain randomization (sensors only)
- No launch system for environment switching

**Recommendation:** Prioritize ros2_control integration and V2 observation training before considering real hardware deployment. Current architecture would require 40-60 hours of refactoring to support real robots.

**Estimated Effort for Real Deployment:**
- ros2_control integration: 20-30 hours
- URDF creation: 8-12 hours
- V2 observation training: 10-15 hours
- Launch system refactor: 8-10 hours
- Real hardware plugin: 20-30 hours
- Testing and validation: 40-60 hours

**Total:** 106-157 hours (3-4 work months)

Despite gaps, the codebase demonstrates strong software engineering practices and thoughtful design. With focused effort on hardware abstraction, Warehouser can become a robust sim-to-real platform.
