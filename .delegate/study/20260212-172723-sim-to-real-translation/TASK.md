# TASK: Implement Hardware Abstraction Layer for Sim-to-Real Transfer

Created: 2026-02-12T17:45:00
Build: PARTIAL (dependencies missing - not critical for task)
Tests: 24/28 Python tests pass (ROS integration tests require colcon)

## Summary

Implement a comprehensive hardware abstraction layer using ros2_control to enable seamless switching between simulation and real robot hardware. This includes creating URDF robot descriptions, hardware interface plugins, controller configurations, domain randomization enhancements, and launch file infrastructure that supports environment-specific parameter loading. The goal is to prepare the Warehouser codebase for eventual real robot deployment while maintaining the current simulation training workflow.

## Current State

### Strengths
- Excellent sensor abstraction via ISensor interface (lidar, odometry simulators)
- Domain randomization for sensor observations (Gaussian noise, dropout)
- Multi-robot architecture with per-robot observations and rewards
- Standard ROS2 messages (sensor_msgs/LaserScan for lidar)
- Clean separation: simulation, observation, RL bridge, inference
- ONNX export for embedded deployment
- Versioned observation system (V1_Position, V2_Lidar, V3_MultiRobot)

### Critical Gaps
- **No ros2_control hardware abstraction** - Direct /cmd_vel subscription bypasses controller layer
- **No URDF/XACRO robot description** - Missing TF tree, joint definitions
- **Instantaneous velocity changes** - No motor dynamics, acceleration limits, or lag modeling
- **Limited domain randomization** - Sensors only, no physics (mass, friction, inertia)
- **No launch configuration system** - Cannot switch between sim and real environments
- **V1 observation dependence** - Training uses ground-truth positions unavailable on real robots
- **No action delay modeling** - Real robots have 50-100ms latency

### File-Level Findings

**Positive Implementations:**
- `sensor_interface.hpp:52-69` - ISensor abstraction enabling polymorphism
- `noise_model.hpp:22-61` - Well-structured noise configuration
- `observation_builder.cpp:14-25` - Versioned observation system
- `lidar_simulator.cpp:63-92` - Standard sensor_msgs/LaserScan output

**Issues Requiring Attention:**
- `simulation_node.cpp:104-106` - Direct cmd_vel subscription (no controller)
- `robot.hpp:43-48` - Instant velocity updates (x += v*cos(θ)*dt with no acceleration)
- `observation_builder.cpp:17-20` - V2_Lidar falls back to V1 (sensor-based observation incomplete)
- `world_manager.cpp` - No physics parameter randomization

## Target State

### Architecture with ros2_control

```
┌─────────────────────────────────────────────┐
│        RL Policy (ONNX Runtime)             │
│  - InferenceNode loads .onnx model          │
│  - Publishes /cmd_vel (geometry_msgs/Twist) │
└────────────────┬────────────────────────────┘
                 │ cmd_vel topic
                 ▼
┌─────────────────────────────────────────────┐
│     DiffDriveController (ros2_control)      │
│  - Converts cmd_vel to wheel velocities     │
│  - Applies acceleration/velocity limits     │
│  - Publishes /odom (nav_msgs/Odometry)      │
└────────────────┬────────────────────────────┘
                 │ wheel velocity commands
                 ▼
┌─────────────────────────────────────────────┐
│      Hardware Interface Plugin              │
│  EITHER:                                    │
│  - WarehouseSimHardware (kinematic sim)     │
│  OR:                                        │
│  - WarehouseRealHardware (motor drivers)    │
└─────────────────────────────────────────────┘
```

**Key Benefits:**
- Same control stack in simulation and reality
- Plug-and-play hardware switching via XACRO parameter
- Standard ROS2 patterns (TF tree, joint_states, odometry)
- Acceleration limits prevent unrealistic commands
- Ready for Nav2 integration

## Implementation Plan

### Phase 1: Hardware Abstraction Foundation (20-30 hours)

#### Create warehouser_control Package
- [ ] Package structure with CMakeLists.txt, package.xml, plugin XML
- [ ] WarehouseSimHardware class (hardware_interface::SystemInterface)
- [ ] WarehouseRealHardware class (stub implementation with TODOs)
- [ ] Plugin export via pluginlib
- [ ] Dependencies: hardware_interface, pluginlib, rclcpp, rclcpp_lifecycle

#### Implement Simulation Hardware Interface
- [ ] on_init() - Validate joint interfaces, read parameters (wheel_separation, wheel_radius)
- [ ] on_configure() - Reset state to zero
- [ ] on_activate() - Sync commands with state
- [ ] read() - Integrate wheel velocities to positions (hw_positions[i] += hw_velocities[i] * dt)
- [ ] write() - Apply velocity commands (simulation: instant, real: send to motors)
- [ ] Export command interfaces (left_wheel_joint/velocity, right_wheel_joint/velocity)
- [ ] Export state interfaces (left_wheel_joint/position, left_wheel_joint/velocity, etc.)

### Phase 2: Robot Description (8-12 hours)

#### Create warehouser_description Package
- [ ] Package structure with urdf/, rviz/, meshes/ directories
- [ ] warehouse_robot.urdf.xacro - Main robot description
  - [ ] base_link, left_wheel_link, right_wheel_link, lidar_link
  - [ ] Joint definitions (left_wheel_joint, right_wheel_joint as continuous)
  - [ ] Visual and collision geometries
  - [ ] Inertial properties (mass, inertia tensors)
- [ ] warehouse_robot.ros2_control.xacro - Hardware abstraction macro
  - [ ] Conditional plugin selection (use_sim parameter)
  - [ ] Joint command/state interface definitions
  - [ ] Hardware parameters (wheel_separation, wheel_radius, serial_port, baud_rate)
- [ ] rviz/warehouse_robot.rviz - Visualization configuration
- [ ] Integration with robot_state_publisher for TF tree

### Phase 3: Configuration System (8-10 hours)

#### Controller Configuration
- [ ] config/controllers.yaml - Base controller manager and diff_drive_controller settings
  - [ ] update_rate: 50 Hz
  - [ ] wheel_separation, wheel_radius
  - [ ] Velocity limits (linear: ±1.0 m/s, angular: ±1.0 rad/s)
  - [ ] Acceleration limits (linear: ±1.0 m/s², angular: ±1.0 rad/s²)
  - [ ] Odometry configuration (odom_frame_id, base_frame_id, covariance)
  - [ ] cmd_vel_timeout: 0.5s
- [ ] config/sim_controllers.yaml - Simulation-specific overrides
  - [ ] open_loop: true (no encoder feedback)
  - [ ] cmd_vel_timeout: 1.0s (more lenient)
- [ ] config/real_controllers.yaml - Real hardware overrides
  - [ ] open_loop: false (use encoder feedback)
  - [ ] Conservative velocity limits (0.5 m/s linear)
  - [ ] cmd_vel_timeout: 0.3s (stricter for safety)

#### Launch System Refactor
- [ ] launch/warehouse_robot.launch.py - Common robot bringup
  - [ ] DeclareLaunchArgument: use_sim, use_sim_time, gui
  - [ ] robot_description from XACRO with use_sim parameter
  - [ ] Conditional controller config loading (sim_controllers.yaml vs real_controllers.yaml)
  - [ ] ros2_control_node with robot_description and controller config
  - [ ] robot_state_publisher_node
  - [ ] Controller spawners (joint_state_broadcaster, diff_drive_controller)
  - [ ] RViz2 node (conditional on gui argument)
- [ ] launch/sim.launch.py - Simulation environment
  - [ ] Launch Gazebo (gz sim)
  - [ ] Spawn robot in Gazebo via ros_gz_sim
  - [ ] Include warehouse_robot.launch.py with use_sim=true
- [ ] launch/real.launch.py - Real hardware deployment
  - [ ] Include warehouse_robot.launch.py with use_sim=false
  - [ ] Additional hardware startup (motor driver initialization, emergency stop monitoring)

### Phase 4: Domain Randomization Enhancement (10-15 hours)

#### Expand Simulation Randomization
- [ ] config/domain_randomization.yaml - Comprehensive randomization config
  - [ ] Physics: robot_mass (±20%), wheel_friction (±30%), floor_friction (±50%), motor_torque_limit (±15%)
  - [ ] Dynamics: action_delay (0-100ms), control_noise (2% Gaussian), wheel_slip (0-10%)
  - [ ] Observations: lidar_noise (1cm stddev), lidar_dropout (5%), odometry_drift (2% per meter), goal_position_noise (5cm)
  - [ ] Environment: obstacle_variations (count 5-15, size 0.1-0.5m), package_mass (0.5-5.0kg)

#### Python Domain Randomizer
- [ ] training/training/envs/randomization.py - DomainRandomizer class
  - [ ] Load config from YAML
  - [ ] randomize_lidar() - Gaussian noise + dropout
  - [ ] apply_odometry_drift() - Cumulative error proportional to distance/angle
  - [ ] randomize_action() - Control noise (multiplicative)
  - [ ] get_action_delay() - Random delay in 0-100ms range
  - [ ] get_physics_randomization_params() - Return dict of randomized physics for sim reset

#### C++ World Manager Integration
- [ ] world_manager.hpp - Add physics parameter members (mass_factor, friction_coeffs, torque_limits)
- [ ] world_manager.cpp - Reset service accepts randomization parameters
- [ ] robot.hpp - Add acceleration limits, motor lag modeling
- [ ] robot.cpp - Update kinematic integration with realistic dynamics
  - [ ] Acceleration clamping (a_max = 1.0 m/s²)
  - [ ] Exponential velocity smoothing (v_actual = 0.9*v_actual + 0.1*v_commanded)
  - [ ] Friction-based deceleration when no command

#### Gym Environment Integration
- [ ] training/training/envs/warehouser_env.py modifications
  - [ ] Initialize DomainRandomizer with config path
  - [ ] reset(): Get physics params, send to ROS, clear action delay buffer
  - [ ] step(): Apply action randomization, implement delay buffer, randomize observations
  - [ ] Track cumulative distance/angle for odometry drift

### Phase 5: V2 Observation Completion (10-15 hours)

#### Complete Sensor-Based Observation
- [ ] observation_builder.cpp - Implement buildV2()
  - [ ] Observation space: [60 lidar ranges, goal_bearing, goal_distance, is_carrying] (63 dims)
  - [ ] No ground-truth position (only sensor-observable data)
  - [ ] Normalize lidar ranges to [0, 1] (range / max_range)
  - [ ] Goal bearing/distance computed from odometry (not ground truth)
- [ ] Training pipeline update
  - [ ] env_config.yaml - Set observation_version: V2_Lidar, obs_dim: 63
  - [ ] Retrain PPO policy with V2 observations
  - [ ] Benchmark performance vs V1 (expect initial degradation)
  - [ ] Export ONNX model after convergence
- [ ] Observation node update
  - [ ] observations_node.cpp - Switch to V2 for inference
  - [ ] Ensure inference node receives 63-dim observations

### Phase 6: Testing and Validation (20-30 hours)

#### Unit Tests
- [ ] test_warehouse_sim_hardware.cpp - Test hardware interface lifecycle
  - [ ] Verify on_init validates joint interfaces correctly
  - [ ] Test read() integrates velocities properly
  - [ ] Test write() applies commands
- [ ] test_domain_randomizer.py - Test Python randomization
  - [ ] Verify noise distributions match config
  - [ ] Test action delay buffer logic
  - [ ] Verify physics param generation

#### Integration Tests
- [ ] test/test_hardware_switching.py - launch_testing
  - [ ] Launch robot with use_sim=true
  - [ ] Verify /joint_states topic publishes correct joints
  - [ ] Publish /cmd_vel, verify joint velocities change
  - [ ] Check /odom topic publishes nav_msgs/Odometry
- [ ] test/test_controller_limits.py
  - [ ] Send velocity exceeding limits
  - [ ] Verify controller clamps to max_velocity
  - [ ] Send acceleration exceeding limits
  - [ ] Verify smooth ramping to commanded velocity

#### Validation Scripts
- [ ] scripts/validate_sim_to_real.py
  - [ ] Load policy, run 100 episodes in simulation
  - [ ] Measure: success rate, collision rate, average episode length
  - [ ] Compare V1 vs V2 observation performance
  - [ ] Generate performance report

## Interface Definitions

### Hardware Interface (C++)

```cpp
// warehouser_control/include/warehouser_control/warehouse_sim_hardware.hpp
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace warehouser_control {

class WarehouseSimHardware : public hardware_interface::SystemInterface {
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(WarehouseSimHardware)

  // Lifecycle callbacks
  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;
  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  // Read/Write hardware
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // Joint state storage (automatically exported by framework)
  std::vector<double> hw_positions_;   // Wheel positions (radians)
  std::vector<double> hw_velocities_;  // Wheel velocities (rad/s)
  std::vector<double> hw_commands_;    // Commanded wheel velocities

  // Robot parameters from URDF
  double wheel_separation_;  // Distance between wheels (m)
  double wheel_radius_;      // Wheel radius (m)
};

}  // namespace warehouser_control
```

### URDF/XACRO Macro

```xml
<!-- warehouser_description/urdf/warehouse_robot.ros2_control.xacro -->
<?xml version="1.0"?>
<robot xmlns:xacro="http://www.ros.org/wiki/xacro">
  <xacro:macro name="warehouse_robot_ros2_control" params="
    name
    use_sim:=true
    wheel_separation:=0.3
    wheel_radius:=0.075">

    <ros2_control name="${name}" type="system">
      <!-- Conditional hardware plugin selection -->
      <xacro:if value="${use_sim}">
        <hardware>
          <plugin>warehouser_control/WarehouseSimHardware</plugin>
          <param name="wheel_separation">${wheel_separation}</param>
          <param name="wheel_radius">${wheel_radius}</param>
        </hardware>
      </xacro:if>

      <xacro:unless value="${use_sim}">
        <hardware>
          <plugin>warehouser_control/WarehouseRealHardware</plugin>
          <param name="wheel_separation">${wheel_separation}</param>
          <param name="wheel_radius">${wheel_radius}</param>
          <param name="serial_port">/dev/ttyUSB0</param>
          <param name="baud_rate">115200</param>
        </hardware>
      </xacro:unless>

      <!-- Left wheel joint -->
      <joint name="left_wheel_joint">
        <command_interface name="velocity">
          <param name="min">-2.0</param>
          <param name="max">2.0</param>
        </command_interface>
        <state_interface name="position"/>
        <state_interface name="velocity"/>
      </joint>

      <!-- Right wheel joint -->
      <joint name="right_wheel_joint">
        <command_interface name="velocity">
          <param name="min">-2.0</param>
          <param name="max">2.0</param>
        </command_interface>
        <state_interface name="position"/>
        <state_interface name="velocity"/>
      </joint>
    </ros2_control>
  </xacro:macro>
</robot>
```

### Controller Configuration (YAML)

```yaml
# warehouser_control/config/controllers.yaml
controller_manager:
  ros__parameters:
    update_rate: 50  # Hz

    diff_drive_controller:
      type: diff_drive_controller/DiffDriveController
    joint_state_broadcaster:
      type: joint_state_broadcaster/JointStateBroadcaster

diff_drive_controller:
  ros__parameters:
    left_wheel_names: ["left_wheel_joint"]
    right_wheel_names: ["right_wheel_joint"]

    wheel_separation: 0.3
    wheel_radius: 0.075

    odom_frame_id: odom
    base_frame_id: base_link
    pose_covariance_diagonal: [0.001, 0.001, 0.001, 0.001, 0.001, 0.01]
    twist_covariance_diagonal: [0.001, 0.001, 0.001, 0.001, 0.001, 0.01]

    publish_rate: 50.0
    enable_odom_tf: true

    # Velocity limits
    linear:
      x:
        has_velocity_limits: true
        max_velocity: 1.0
        min_velocity: -1.0
        has_acceleration_limits: true
        max_acceleration: 1.0
        min_acceleration: -1.0

    angular:
      z:
        has_velocity_limits: true
        max_velocity: 1.0
        min_velocity: -1.0
        has_acceleration_limits: true
        max_acceleration: 1.0
        min_acceleration: -1.0

    cmd_vel_timeout: 0.5
    open_loop: false
```

### Domain Randomization Configuration (YAML)

```yaml
# warehouser_control/config/domain_randomization.yaml
domain_randomization:
  ros__parameters:
    enabled: true

    physics:
      robot_mass:
        enabled: true
        distribution: uniform
        min_factor: 0.8   # 80% of nominal mass
        max_factor: 1.2   # 120% of nominal mass

      wheel_friction:
        enabled: true
        distribution: uniform
        min_value: 0.5
        max_value: 1.5

      floor_friction:
        enabled: true
        distribution: uniform
        min_value: 0.3
        max_value: 1.0

      motor_torque_limit:
        enabled: true
        distribution: uniform
        min_factor: 0.85
        max_factor: 1.0

    dynamics:
      action_delay:
        enabled: true
        distribution: uniform
        min_ms: 0
        max_ms: 100

      control_noise:
        enabled: true
        distribution: gaussian
        mean: 0.0
        stddev: 0.02  # 2% of command

      wheel_slip:
        enabled: true
        distribution: uniform
        min_coefficient: 0.0
        max_coefficient: 0.1

    observations:
      lidar_noise:
        enabled: true
        distribution: gaussian
        mean: 0.0
        stddev: 0.01  # 1cm

      lidar_dropout:
        enabled: true
        dropout_probability: 0.05  # 5% of beams

      odometry_drift:
        enabled: true
        linear_drift_per_meter: 0.02   # 2% per meter
        angular_drift_per_radian: 0.05 # 5% per radian

      goal_position_noise:
        enabled: true
        distribution: gaussian
        mean: 0.0
        stddev: 0.05  # 5cm
```

### Python Domain Randomizer

```python
# training/training/envs/randomization.py
import numpy as np
from typing import Dict, Any
import yaml

class DomainRandomizer:
    """Applies domain randomization to observations and environment."""

    def __init__(self, config_path: str):
        with open(config_path, 'r') as f:
            config = yaml.safe_load(f)
        self.config = config['domain_randomization']['ros__parameters']
        self.enabled = self.config['enabled']

    def randomize_lidar(self, scan: np.ndarray,
                        min_range: float, max_range: float) -> np.ndarray:
        """Apply Gaussian noise + dropout to lidar."""
        if not self.enabled:
            return scan

        randomized = scan.copy()

        # Gaussian noise
        cfg = self.config['observations']['lidar_noise']
        if cfg['enabled']:
            noise = np.random.normal(cfg['mean'], cfg['stddev'], scan.shape)
            randomized += noise

        # Dropout
        cfg = self.config['observations']['lidar_dropout']
        if cfg['enabled']:
            mask = np.random.random(scan.shape) < cfg['dropout_probability']
            randomized[mask] = max_range

        return np.clip(randomized, min_range, max_range).astype(np.float32)

    def apply_odometry_drift(self, position: np.ndarray,
                            distance_traveled: float,
                            angle_turned: float) -> np.ndarray:
        """Apply cumulative drift to odometry."""
        if not self.enabled:
            return position

        cfg = self.config['observations']['odometry_drift']
        if not cfg['enabled']:
            return position

        linear_drift = distance_traveled * cfg['linear_drift_per_meter']
        angular_drift = angle_turned * cfg['angular_drift_per_radian']

        drifted = position.copy()
        drifted[:2] += np.random.randn(2) * linear_drift
        drifted[2] += np.random.randn() * angular_drift

        return drifted.astype(np.float32)

    def get_action_delay(self) -> float:
        """Get randomized action delay in seconds."""
        if not self.enabled:
            return 0.0

        cfg = self.config['dynamics']['action_delay']
        if not cfg['enabled']:
            return 0.0

        delay_ms = np.random.uniform(cfg['min_ms'], cfg['max_ms'])
        return delay_ms / 1000.0

    def get_physics_randomization_params(self) -> Dict[str, Any]:
        """Get randomized physics parameters for sim reset."""
        if not self.enabled:
            return {}

        params = {}
        physics = self.config['physics']

        if physics['robot_mass']['enabled']:
            cfg = physics['robot_mass']
            params['robot_mass_factor'] = np.random.uniform(
                cfg['min_factor'], cfg['max_factor'])

        if physics['wheel_friction']['enabled']:
            cfg = physics['wheel_friction']
            params['wheel_friction'] = np.random.uniform(
                cfg['min_value'], cfg['max_value'])

        if physics['floor_friction']['enabled']:
            cfg = physics['floor_friction']
            params['floor_friction'] = np.random.uniform(
                cfg['min_value'], cfg['max_value'])

        if physics['motor_torque_limit']['enabled']:
            cfg = physics['motor_torque_limit']
            params['motor_torque_factor'] = np.random.uniform(
                cfg['min_factor'], cfg['max_factor'])

        return params
```

## New Packages to Create

| Package | Purpose | Key Exports |
|---------|---------|-------------|
| warehouser_control | Hardware abstraction layer | WarehouseSimHardware, WarehouseRealHardware plugins, controller configs |
| warehouser_description | Robot URDF/XACRO definitions | warehouse_robot.urdf.xacro, ros2_control macro, RViz configs |

## Files to Create

| File | Purpose |
|------|---------|
| ros_ws/src/warehouser_control/include/warehouser_control/warehouse_sim_hardware.hpp | Simulation hardware interface header |
| ros_ws/src/warehouser_control/src/warehouse_sim_hardware.cpp | Simulation hardware implementation (kinematic integration) |
| ros_ws/src/warehouser_control/include/warehouser_control/warehouse_real_hardware.hpp | Real hardware interface header |
| ros_ws/src/warehouser_control/src/warehouse_real_hardware.cpp | Real hardware stub (motor driver communication) |
| ros_ws/src/warehouser_control/warehouser_control_plugin.xml | Pluginlib export for hardware interfaces |
| ros_ws/src/warehouser_control/config/controllers.yaml | Base controller configuration |
| ros_ws/src/warehouser_control/config/sim_controllers.yaml | Simulation-specific controller overrides |
| ros_ws/src/warehouser_control/config/real_controllers.yaml | Real hardware controller overrides |
| ros_ws/src/warehouser_control/config/domain_randomization.yaml | Comprehensive randomization configuration |
| ros_ws/src/warehouser_description/urdf/warehouse_robot.urdf.xacro | Main robot URDF with links, joints, inertias |
| ros_ws/src/warehouser_description/urdf/warehouse_robot.ros2_control.xacro | ros2_control configuration macro |
| ros_ws/src/warehouser_description/rviz/warehouse_robot.rviz | RViz visualization configuration |
| ros_ws/src/warehouser_bringup/launch/warehouse_robot.launch.py | Common robot bringup with conditional hardware |
| ros_ws/src/warehouser_bringup/launch/sim.launch.py | Simulation environment launcher (Gazebo + robot) |
| ros_ws/src/warehouser_bringup/launch/real.launch.py | Real hardware launcher |
| training/training/envs/randomization.py | Python domain randomization module |
| test/test_warehouse_sim_hardware.cpp | Unit tests for hardware interface |
| test/test_hardware_switching.py | Integration tests with launch_testing |
| scripts/validate_sim_to_real.py | Performance validation script |

## Files to Modify

| File | Change |
|------|--------|
| ros_ws/src/warehouser_simulation/src/simulation_node.cpp | Remove direct cmd_vel subscription, rely on ros2_control for velocity commands |
| ros_ws/src/warehouser_simulation/include/warehouser_simulation/robot.hpp | Add acceleration limits, motor lag model, friction-based deceleration |
| ros_ws/src/warehouser_simulation/src/robot.cpp | Replace instant velocity updates with realistic dynamics (exponential smoothing, acceleration clamping) |
| ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp | Add physics randomization parameters (mass_factor, friction_coeffs, torque_limits) |
| ros_ws/src/warehouser_simulation/src/world_manager.cpp | Accept randomization params in reset service, apply to robot/world |
| ros_ws/src/warehouser_observations/src/observation_builder.cpp | Implement buildV2() for sensor-based observations (63-dim: lidar + goal bearing/dist + carrying) |
| ros_ws/src/warehouser_bringup/config/world.yaml | Add physics parameter fields (mass, friction, inertia) |
| training/training/envs/ros_env.py | Integrate DomainRandomizer, implement action delay buffer, randomize observations |
| training/config/env_config.yaml | Switch observation_version to V2_Lidar, set obs_dim to 63 |
| ros_ws/src/warehouser_observations/src/observations_node.cpp | Use ObservationVersion::V2_Lidar for inference |

## Architecture Notes

### Key Design Decisions

1. **ros2_control as Abstraction Boundary**
   - Industry-standard pattern used across ROS2 ecosystem
   - Clean separation: controllers (generic) vs hardware (sim/real specific)
   - Same DiffDriveController works in both environments
   - Enables Nav2 integration without code changes

2. **XACRO Conditional Plugin Selection**
   - Single URDF source file with use_sim parameter
   - Avoids dual URDF maintenance (sim vs real)
   - Hardware plugin chosen at launch time, not compile time
   - Supports multiple robot variants (different wheel sizes, sensors)

3. **YAML-Driven Domain Randomization**
   - Configuration separate from code for easy tuning
   - Enables A/B testing of randomization strategies
   - Version-controllable randomization settings
   - Easy to disable/enable specific randomizations

4. **Sensor-Based Observations (V2)**
   - Critical for sim-to-real transfer (V1 uses unavailable ground truth)
   - Lidar + odometry only (realistic sensor suite)
   - No privileged information (object positions, exact distances)
   - Forces policy to handle sensor noise and uncertainty

5. **Modular Launch System**
   - Common launch file (warehouse_robot.launch.py) included by sim/real
   - Environment-specific configs loaded conditionally
   - Easy to add new environments (HIL, staged deployment)
   - Follows Nav2 launch architecture pattern

6. **Action Delay Buffer**
   - Models real robot communication latency (50-100ms)
   - Prevents policies from assuming instant response
   - Implemented in Python gym environment (training time)
   - Real robot will naturally have this delay (no code needed)

### Modularity and Extensibility

**Adding New Sensors:**
1. Extend URDF with new sensor link/joint
2. Add sensor plugin in XACRO (Gazebo for sim, driver for real)
3. Implement ISensor subclass in ros_observations
4. Update observation builder to include sensor data
5. Retrain with expanded observation space

**Supporting New Robot Platforms:**
1. Create new URDF with platform-specific geometry
2. Implement platform-specific WarehouseRealHardware plugin
3. Add platform config YAML (wheel params, sensor offsets)
4. Launch with platform-specific parameters

**Staged Deployment Strategy:**
1. Pure Simulation (current) - Train, validate in sim
2. Hardware-in-Loop (future) - Real controller, simulated environment
3. Constrained Real Environment (future) - Safe physical space, reduced limits
4. Operational Deployment (future) - Full warehouse environment

## Verification

### Build Verification
- [ ] colcon build --packages-select warehouser_control succeeds
- [ ] colcon build --packages-select warehouser_description succeeds
- [ ] All package dependencies resolved (hardware_interface, controller_manager, diff_drive_controller)

### Functional Verification
- [ ] Launch sim.launch.py, verify robot spawns in Gazebo
- [ ] ros2 topic echo /joint_states shows left_wheel_joint, right_wheel_joint
- [ ] ros2 topic echo /odom publishes nav_msgs/Odometry at 50Hz
- [ ] ros2 topic pub /cmd_vel geometry_msgs/Twist moves robot in RViz
- [ ] RViz shows TF tree: odom -> base_link -> lidar_link

### Controller Verification
- [ ] Send cmd_vel with velocity exceeding limits, verify clamping to max_velocity
- [ ] Send rapid velocity changes, verify smooth acceleration (not instant)
- [ ] Stop cmd_vel publishing, verify robot stops within cmd_vel_timeout (0.5s)
- [ ] Monitor /diff_drive_controller/transition_event for lifecycle state changes

### Domain Randomization Verification
- [ ] Train with randomization enabled, log physics params per episode
- [ ] Verify mass, friction, delay vary across episodes
- [ ] Verify lidar scans show noise and dropouts
- [ ] Compare policy trained with/without randomization on transfer metrics

### Observation V2 Verification
- [ ] Switch to V2_Lidar, verify observation shape is (63,)
- [ ] Verify observation contains: 60 lidar ranges + bearing + distance + carrying flag
- [ ] Train policy with V2, verify convergence (may be slower than V1)
- [ ] Export ONNX, verify inference node loads and executes with V2 observations

### Integration Test Verification
- [ ] launch_testing test_hardware_switching.py passes
- [ ] test_controller_limits.py verifies acceleration/velocity clamping
- [ ] Simulation runs 100 episodes without crashes
- [ ] Success rate, collision rate within expected ranges

## Success Criteria

1. Can launch robot with use_sim=true and use_sim=false (real hardware stub)
2. DiffDriveController publishes /odom with realistic covariance
3. Velocity/acceleration limits enforced by controller, not policy
4. Domain randomization covers physics, dynamics, and observations
5. V2 observation training achieves 80%+ of V1 performance
6. Integration tests pass in CI/CD
7. Code follows Warehouser standards (C++23, float precision, REP 103 coordinates)

## Estimated Effort

- Phase 1 (Hardware Abstraction): 20-30 hours
- Phase 2 (Robot Description): 8-12 hours
- Phase 3 (Configuration System): 8-10 hours
- Phase 4 (Domain Randomization): 10-15 hours
- Phase 5 (V2 Observation): 10-15 hours
- Phase 6 (Testing/Validation): 20-30 hours

**Total: 76-112 hours (2-3 months with focused effort)**

## References

- ros2_control documentation: https://control.ros.org/rolling/
- ros2_control_demos (DiffBot): https://github.com/ros-controls/ros2_control_demos
- Domain randomization research (OpenAI Dactyl): https://openai.com/index/learning-dexterity/
- Nav2 launch patterns: https://github.com/ros-planning/navigation2
- REP 103 (Standard Units/Coordinates): https://www.ros.org/reps/rep-0103.html

## Sources

This task consolidates findings from:
- **[S] Search Findings**: Sim-to-real transfer patterns, ros2_control architecture, OpenAI Dactyl case study, warehouse robotics deployments, domain randomization techniques
- **[I] Introspection Findings**: Current codebase gaps (no ros2_control, direct cmd_vel, instant velocity changes, V2 observation incomplete, limited randomization)
- **[T] Template Findings**: Complete reference implementations for hardware interfaces, URDF macros, controller configs, launch files, domain randomization integration
