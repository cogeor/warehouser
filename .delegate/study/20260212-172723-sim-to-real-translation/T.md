# Template Analysis: Sim-to-Real Transfer Patterns

Created: 2026-02-12T17:30:00

## Sources

Primary reference implementations analyzed:
- **ros2_control_demos** - Example 2 (DiffBot) - Differential drive hardware abstraction
- **Navigation2** (nav2_bringup) - Launch configuration and parameter management
- **gz_ros2_control** - Gazebo simulation integration
- **OpenAI Gym** - Environment wrapper patterns
- **Isaac Gym Envs** - Domain randomization frameworks

## Patterns Discovered

### Pattern 1: Hardware Abstraction Layer (ros2_control)

The ros2_control framework provides the standard pattern for hardware abstraction, enabling seamless switching between simulation and real hardware.

**Architecture:**
```
RL Policy (ONNX)
    ↓ cmd_vel
DiffDriveController (ros2_control)
    ↓ wheel velocities
Hardware Interface Plugin
    ↓
[SimHardware OR RealHardware]
```

### Pattern 2: URDF/XACRO Configuration with Hardware Toggle

Use XACRO macros to conditionally load different hardware plugins based on a parameter.

### Pattern 3: Launch File Environment Switching

Use launch arguments and conditional logic to load environment-specific configurations.

### Pattern 4: Controller Configuration via YAML

Separate controller parameters into YAML files for easy tuning without code changes.

### Pattern 5: Domain Randomization Integration

Hydra/YAML-based configuration for physics and observation randomization ranges.

---

## Application to Warehouser

### 1. Create Hardware Abstraction Layer

**File Structure:**
```
warehouser_control/
├── CMakeLists.txt
├── package.xml
├── include/warehouser_control/
│   ├── warehouse_sim_hardware.hpp
│   └── warehouse_real_hardware.hpp
├── src/
│   ├── warehouse_sim_hardware.cpp
│   └── warehouse_real_hardware.cpp
└── warehouser_control_plugin.xml
```

**warehouse_sim_hardware.hpp:**
```cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace warehouser_control
{

class WarehouseSimHardware : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(WarehouseSimHardware)

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // Store joint positions and velocities (state)
  std::vector<double> hw_positions_;
  std::vector<double> hw_velocities_;
  // Store joint velocity commands
  std::vector<double> hw_commands_;

  // Robot parameters
  double wheel_separation_{0.0};
  double wheel_radius_{0.0};
};

}  // namespace warehouser_control
```

**warehouse_sim_hardware.cpp:**
```cpp
#include "warehouser_control/warehouse_sim_hardware.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

namespace warehouser_control
{

hardware_interface::CallbackReturn WarehouseSimHardware::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (
    hardware_interface::SystemInterface::on_init(info) !=
    hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  // Initialize state and command storage
  hw_positions_.resize(info_.joints.size(), 0.0);
  hw_velocities_.resize(info_.joints.size(), 0.0);
  hw_commands_.resize(info_.joints.size(), 0.0);

  // Validate joint interfaces
  for (const hardware_interface::ComponentInfo & joint : info_.joints)
  {
    // Expect exactly one command interface (velocity)
    if (joint.command_interfaces.size() != 1)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("WarehouseSimHardware"),
        "Joint '%s' has %zu command interfaces. Expected 1.",
        joint.name.c_str(), joint.command_interfaces.size());
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.command_interfaces[0].name != hardware_interface::HW_IF_VELOCITY)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("WarehouseSimHardware"),
        "Joint '%s' has '%s' command interface. Expected '%s'.",
        joint.name.c_str(), joint.command_interfaces[0].name.c_str(),
        hardware_interface::HW_IF_VELOCITY);
      return hardware_interface::CallbackReturn::ERROR;
    }

    // Expect two state interfaces (position and velocity)
    if (joint.state_interfaces.size() != 2)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("WarehouseSimHardware"),
        "Joint '%s' has %zu state interfaces. Expected 2.",
        joint.name.c_str(), joint.state_interfaces.size());
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.state_interfaces[0].name != hardware_interface::HW_IF_POSITION)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("WarehouseSimHardware"),
        "Joint '%s' first state interface is '%s'. Expected '%s'.",
        joint.name.c_str(), joint.state_interfaces[0].name.c_str(),
        hardware_interface::HW_IF_POSITION);
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.state_interfaces[1].name != hardware_interface::HW_IF_VELOCITY)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("WarehouseSimHardware"),
        "Joint '%s' second state interface is '%s'. Expected '%s'.",
        joint.name.c_str(), joint.state_interfaces[1].name.c_str(),
        hardware_interface::HW_IF_VELOCITY);
      return hardware_interface::CallbackReturn::ERROR;
    }
  }

  // Read hardware parameters
  wheel_separation_ = std::stod(info_.hardware_parameters["wheel_separation"]);
  wheel_radius_ = std::stod(info_.hardware_parameters["wheel_radius"]);

  RCLCPP_INFO(
    rclcpp::get_logger("WarehouseSimHardware"),
    "Initialized with wheel_separation=%.3f, wheel_radius=%.3f",
    wheel_separation_, wheel_radius_);

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn WarehouseSimHardware::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("WarehouseSimHardware"), "Configuring...");

  // Reset all values to zero
  for (size_t i = 0; i < hw_positions_.size(); i++)
  {
    hw_positions_[i] = 0.0;
    hw_velocities_[i] = 0.0;
    hw_commands_[i] = 0.0;
  }

  RCLCPP_INFO(rclcpp::get_logger("WarehouseSimHardware"), "Successfully configured!");

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn WarehouseSimHardware::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("WarehouseSimHardware"), "Activating...");

  // Synchronize commands with current state
  for (size_t i = 0; i < hw_commands_.size(); i++)
  {
    hw_commands_[i] = hw_velocities_[i];
  }

  RCLCPP_INFO(rclcpp::get_logger("WarehouseSimHardware"), "Successfully activated!");

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn WarehouseSimHardware::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("WarehouseSimHardware"), "Deactivating...");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type WarehouseSimHardware::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
  // Simulate wheel dynamics by integrating velocity commands
  for (size_t i = 0; i < hw_positions_.size(); i++)
  {
    // Update velocity from command (in real hardware, read from encoders)
    hw_velocities_[i] = hw_commands_[i];

    // Integrate position: position += velocity * dt
    hw_positions_[i] += hw_velocities_[i] * period.seconds();
  }

  return hardware_interface::return_type::OK;
}

hardware_interface::return_type WarehouseSimHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // In simulation, commands are directly applied
  // In real hardware, this would send commands to motor controllers

  // Commands are already stored in hw_commands_ by the controller manager
  // Nothing to do here for simulation

  return hardware_interface::return_type::OK;
}

}  // namespace warehouser_control

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  warehouser_control::WarehouseSimHardware, hardware_interface::SystemInterface)
```

**warehouse_real_hardware.cpp:**
```cpp
#include "warehouser_control/warehouse_real_hardware.hpp"

// Same structure as sim hardware, but different implementation:

hardware_interface::return_type WarehouseRealHardware::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // TODO: Read actual encoder values from hardware
  // Example pseudocode:
  // hw_positions_[0] = motor_driver_->getLeftWheelPosition();
  // hw_positions_[1] = motor_driver_->getRightWheelPosition();
  // hw_velocities_[0] = motor_driver_->getLeftWheelVelocity();
  // hw_velocities_[1] = motor_driver_->getRightWheelVelocity();

  return hardware_interface::return_type::OK;
}

hardware_interface::return_type WarehouseRealHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // TODO: Send velocity commands to actual motor controllers
  // Example pseudocode:
  // motor_driver_->setLeftWheelVelocity(hw_commands_[0]);
  // motor_driver_->setRightWheelVelocity(hw_commands_[1]);

  return hardware_interface::return_type::OK;
}
```

**warehouser_control_plugin.xml:**
```xml
<library path="warehouser_control">
  <class name="warehouser_control/WarehouseSimHardware"
         type="warehouser_control::WarehouseSimHardware"
         base_class_type="hardware_interface::SystemInterface">
    <description>
      Simulation hardware interface for Warehouser differential drive robot
    </description>
  </class>
  <class name="warehouser_control/WarehouseRealHardware"
         type="warehouser_control::WarehouseRealHardware"
         base_class_type="hardware_interface::SystemInterface">
    <description>
      Real hardware interface for Warehouser differential drive robot
    </description>
  </class>
</library>
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(warehouser_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)

# Create hardware interface library
add_library(
  warehouser_control
  SHARED
  src/warehouse_sim_hardware.cpp
  src/warehouse_real_hardware.cpp
)

target_compile_features(warehouser_control PUBLIC cxx_std_23)
target_include_directories(warehouser_control PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

ament_target_dependencies(
  warehouser_control
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)

# Export plugin description
pluginlib_export_plugin_description_file(hardware_interface warehouser_control_plugin.xml)

# Install
install(
  TARGETS warehouser_control
  DESTINATION lib
)
install(
  DIRECTORY include/
  DESTINATION include
)

ament_export_include_directories(include)
ament_export_libraries(warehouser_control)
ament_export_dependencies(
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)

ament_package()
```

**package.xml:**
```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>warehouser_control</name>
  <version>0.1.0</version>
  <description>Hardware abstraction layer for Warehouser robots</description>
  <maintainer email="your.email@example.com">Your Name</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>

  <depend>hardware_interface</depend>
  <depend>pluginlib</depend>
  <depend>rclcpp</depend>
  <depend>rclcpp_lifecycle</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

---

### 2. URDF/XACRO Configuration

**warehouse_robot.ros2_control.xacro:**
```xml
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
          <!-- Real hardware specific parameters -->
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

**warehouse_robot.urdf.xacro:**
```xml
<?xml version="1.0"?>
<robot xmlns:xacro="http://www.ros.org/wiki/xacro" name="warehouse_robot">

  <!-- Parameters -->
  <xacro:arg name="use_sim" default="true"/>
  <xacro:arg name="wheel_separation" default="0.3"/>
  <xacro:arg name="wheel_radius" default="0.075"/>

  <!-- Import ros2_control configuration -->
  <xacro:include filename="$(find warehouser_description)/urdf/warehouse_robot.ros2_control.xacro"/>

  <!-- Base link -->
  <link name="base_link">
    <visual>
      <geometry>
        <box size="0.4 0.3 0.1"/>
      </geometry>
      <material name="blue">
        <color rgba="0 0 0.8 1"/>
      </material>
    </visual>
    <collision>
      <geometry>
        <box size="0.4 0.3 0.1"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="10.0"/>
      <inertia ixx="0.1" ixy="0" ixz="0" iyy="0.1" iyz="0" izz="0.1"/>
    </inertial>
  </link>

  <!-- Left wheel -->
  <link name="left_wheel_link">
    <visual>
      <geometry>
        <cylinder radius="$(arg wheel_radius)" length="0.05"/>
      </geometry>
      <material name="black">
        <color rgba="0 0 0 1"/>
      </material>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="$(arg wheel_radius)" length="0.05"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.5"/>
      <inertia ixx="0.01" ixy="0" ixz="0" iyy="0.01" iyz="0" izz="0.01"/>
    </inertial>
  </link>

  <joint name="left_wheel_joint" type="continuous">
    <parent link="base_link"/>
    <child link="left_wheel_link"/>
    <origin xyz="0 ${$(arg wheel_separation)/2} 0" rpy="-1.5708 0 0"/>
    <axis xyz="0 0 1"/>
  </joint>

  <!-- Right wheel -->
  <link name="right_wheel_link">
    <visual>
      <geometry>
        <cylinder radius="$(arg wheel_radius)" length="0.05"/>
      </geometry>
      <material name="black">
        <color rgba="0 0 0 1"/>
      </material>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="$(arg wheel_radius)" length="0.05"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.5"/>
      <inertia ixx="0.01" ixy="0" ixz="0" iyy="0.01" iyz="0" izz="0.01"/>
    </inertial>
  </link>

  <joint name="right_wheel_joint" type="continuous">
    <parent link="base_link"/>
    <child link="right_wheel_link"/>
    <origin xyz="0 ${-$(arg wheel_separation)/2} 0" rpy="-1.5708 0 0"/>
    <axis xyz="0 0 1"/>
  </joint>

  <!-- Lidar sensor link -->
  <link name="lidar_link">
    <visual>
      <geometry>
        <cylinder radius="0.05" length="0.05"/>
      </geometry>
      <material name="red">
        <color rgba="0.8 0 0 1"/>
      </material>
    </visual>
    <collision>
      <geometry>
        <cylinder radius="0.05" length="0.05"/>
      </geometry>
    </collision>
    <inertial>
      <mass value="0.2"/>
      <inertia ixx="0.001" ixy="0" ixz="0" iyy="0.001" iyz="0" izz="0.001"/>
    </inertial>
  </link>

  <joint name="lidar_joint" type="fixed">
    <parent link="base_link"/>
    <child link="lidar_link"/>
    <origin xyz="0.15 0 0.1" rpy="0 0 0"/>
  </joint>

  <!-- Instantiate ros2_control -->
  <xacro:warehouse_robot_ros2_control
    name="warehouse_robot_control"
    use_sim="$(arg use_sim)"
    wheel_separation="$(arg wheel_separation)"
    wheel_radius="$(arg wheel_radius)"/>

  <!-- Gazebo plugin (only when using simulation) -->
  <xacro:if value="$(arg use_sim)">
    <gazebo>
      <plugin filename="gz_ros2_control-system"
              name="gz_ros2_control::GazeboSimROS2ControlPlugin">
        <parameters>$(find warehouser_control)/config/controllers.yaml</parameters>
      </plugin>
    </gazebo>
  </xacro:if>

  <!-- Gazebo lidar sensor -->
  <xacro:if value="$(arg use_sim)">
    <gazebo reference="lidar_link">
      <sensor name="lidar" type="gpu_lidar">
        <topic>scan</topic>
        <update_rate>10</update_rate>
        <lidar>
          <scan>
            <horizontal>
              <samples>360</samples>
              <resolution>1.0</resolution>
              <min_angle>0</min_angle>
              <max_angle>6.28</max_angle>
            </horizontal>
          </scan>
          <range>
            <min>0.1</min>
            <max>10.0</max>
            <resolution>0.01</resolution>
          </range>
          <noise>
            <type>gaussian</type>
            <mean>0.0</mean>
            <stddev>0.01</stddev>
          </noise>
        </lidar>
      </sensor>
    </gazebo>
  </xacro:if>

</robot>
```

---

### 3. Controller Configuration

**config/controllers.yaml:**
```yaml
controller_manager:
  ros__parameters:
    update_rate: 50  # Hz

    # List of controllers to load
    diff_drive_controller:
      type: diff_drive_controller/DiffDriveController

    joint_state_broadcaster:
      type: joint_state_broadcaster/JointStateBroadcaster

# DiffDriveController configuration
diff_drive_controller:
  ros__parameters:
    # Wheel joints
    left_wheel_names: ["left_wheel_joint"]
    right_wheel_names: ["right_wheel_joint"]

    # Robot geometry
    wheel_separation: 0.3
    wheel_radius: 0.075

    # Odometry
    odom_frame_id: odom
    base_frame_id: base_link
    pose_covariance_diagonal: [0.001, 0.001, 0.001, 0.001, 0.001, 0.01]
    twist_covariance_diagonal: [0.001, 0.001, 0.001, 0.001, 0.001, 0.01]

    # Publishing
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

    # Command timeout
    cmd_vel_timeout: 0.5

    # Open loop (no velocity feedback)
    open_loop: false  # Set to true for pure simulation
```

**config/sim_controllers.yaml:**
```yaml
# Extend base controllers with simulation-specific settings
<<: *controllers

diff_drive_controller:
  ros__parameters:
    <<: *diff_drive_params
    open_loop: true  # Simulation doesn't need feedback
    cmd_vel_timeout: 1.0  # More lenient in simulation
```

**config/real_controllers.yaml:**
```yaml
# Extend base controllers with real hardware settings
<<: *controllers

diff_drive_controller:
  ros__parameters:
    <<: *diff_drive_params
    open_loop: false  # Use encoder feedback
    cmd_vel_timeout: 0.3  # Stricter timeout for safety
    # Additional safety limits for real hardware
    linear:
      x:
        max_velocity: 0.5  # Conservative limit
```

---

### 4. Launch Files

**launch/warehouse_robot.launch.py:**
```python
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, Command, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Declare arguments
    use_sim_arg = DeclareLaunchArgument(
        'use_sim',
        default_value='true',
        description='Use simulation hardware if true, real hardware if false'
    )

    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation time if true'
    )

    gui_arg = DeclareLaunchArgument(
        'gui',
        default_value='true',
        description='Start RViz2 if true'
    )

    # Get configuration
    use_sim = LaunchConfiguration('use_sim')
    use_sim_time = LaunchConfiguration('use_sim_time')
    gui = LaunchConfiguration('gui')

    # Paths
    pkg_warehouser_description = get_package_share_directory('warehouser_description')
    pkg_warehouser_control = get_package_share_directory('warehouser_control')

    # URDF via xacro with conditional hardware
    robot_description_content = Command([
        'xacro ',
        PathJoinSubstitution([pkg_warehouser_description, 'urdf', 'warehouse_robot.urdf.xacro']),
        ' use_sim:=', use_sim,
    ])

    robot_description = {'robot_description': robot_description_content}

    # Controller configuration (conditional based on sim/real)
    controller_config = PathJoinSubstitution([
        pkg_warehouser_control,
        'config',
        ['sim_controllers.yaml' if use_sim else 'real_controllers.yaml']
    ])

    # Nodes
    control_node = Node(
        package='controller_manager',
        executable='ros2_control_node',
        parameters=[
            robot_description,
            controller_config,
            {'use_sim_time': use_sim_time}
        ],
        output='both',
    )

    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='both',
        parameters=[
            robot_description,
            {'use_sim_time': use_sim_time}
        ],
    )

    joint_state_broadcaster_spawner = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['joint_state_broadcaster', '--controller-manager', '/controller_manager'],
    )

    diff_drive_controller_spawner = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['diff_drive_controller', '--controller-manager', '/controller_manager'],
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', PathJoinSubstitution([pkg_warehouser_description, 'rviz', 'warehouse_robot.rviz'])],
        condition=IfCondition(gui),
        parameters=[{'use_sim_time': use_sim_time}],
    )

    return LaunchDescription([
        use_sim_arg,
        use_sim_time_arg,
        gui_arg,
        control_node,
        robot_state_publisher_node,
        joint_state_broadcaster_spawner,
        diff_drive_controller_spawner,
        rviz_node,
    ])
```

**launch/sim.launch.py:**
```python
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, ExecuteProcess
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Start Gazebo
    gazebo = ExecuteProcess(
        cmd=['gz', 'sim', '-r', 'empty.sdf'],
        output='screen',
    )

    # Spawn robot in Gazebo
    spawn_robot = ExecuteProcess(
        cmd=['ros2', 'run', 'ros_gz_sim', 'create',
             '-topic', '/robot_description',
             '-name', 'warehouse_robot',
             '-x', '0', '-y', '0', '-z', '0.1'],
        output='screen',
    )

    # Include common robot launch with simulation settings
    robot_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('warehouser_bringup'),
                'launch',
                'warehouse_robot.launch.py'
            ])
        ]),
        launch_arguments={
            'use_sim': 'true',
            'use_sim_time': 'true',
            'gui': 'true',
        }.items()
    )

    return LaunchDescription([
        gazebo,
        spawn_robot,
        robot_launch,
    ])
```

**launch/real.launch.py:**
```python
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Include common robot launch with real hardware settings
    robot_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('warehouser_bringup'),
                'launch',
                'warehouse_robot.launch.py'
            ])
        ]),
        launch_arguments={
            'use_sim': 'false',
            'use_sim_time': 'false',
            'gui': 'true',
        }.items()
    )

    return LaunchDescription([
        robot_launch,
    ])
```

---

### 5. Domain Randomization Integration

**config/domain_randomization.yaml:**
```yaml
domain_randomization:
  ros__parameters:
    # Enable/disable randomization
    enabled: true

    # Physics randomization
    physics:
      robot_mass:
        enabled: true
        distribution: uniform
        min_factor: 0.8  # 80% of nominal
        max_factor: 1.2  # 120% of nominal

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

    # Dynamics randomization
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

    # Observation randomization
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
        linear_drift_per_meter: 0.02  # 2% drift per meter
        angular_drift_per_radian: 0.05  # 5% drift per radian

      goal_position_noise:
        enabled: true
        distribution: gaussian
        mean: 0.0
        stddev: 0.05  # 5cm

    # Environmental randomization
    environment:
      obstacle_variations:
        enabled: true
        count_range: [5, 15]
        size_range: [0.1, 0.5]

      package_mass:
        enabled: true
        distribution: uniform
        min_kg: 0.5
        max_kg: 5.0

      lighting:
        enabled: false  # Not applicable for lidar-only
```

**Python implementation for observation randomization (extends existing code):**

```python
# training/training/envs/randomization.py

import numpy as np
from typing import Dict, Any
import yaml


class DomainRandomizer:
    """Applies domain randomization to observations and environment."""

    def __init__(self, config_path: str):
        """Initialize from YAML configuration."""
        with open(config_path, 'r') as f:
            config = yaml.safe_load(f)

        self.config = config['domain_randomization']['ros__parameters']
        self.enabled = self.config['enabled']

    def randomize_lidar(self, scan: np.ndarray,
                        min_range: float, max_range: float) -> np.ndarray:
        """Apply noise and dropout to lidar scan."""
        if not self.enabled:
            return scan

        randomized = scan.copy()

        # Gaussian noise
        lidar_config = self.config['observations']['lidar_noise']
        if lidar_config['enabled']:
            noise = np.random.normal(
                lidar_config['mean'],
                lidar_config['stddev'],
                scan.shape
            )
            randomized += noise

        # Dropout
        dropout_config = self.config['observations']['lidar_dropout']
        if dropout_config['enabled']:
            dropout_mask = np.random.random(scan.shape) < dropout_config['dropout_probability']
            randomized[dropout_mask] = max_range  # Set to max range (no detection)

        # Clip to valid range
        randomized = np.clip(randomized, min_range, max_range)

        return randomized.astype(np.float32)

    def apply_odometry_drift(self, position: np.ndarray,
                            distance_traveled: float,
                            angle_turned: float) -> np.ndarray:
        """Apply cumulative drift to odometry."""
        if not self.enabled:
            return position

        config = self.config['observations']['odometry_drift']
        if not config['enabled']:
            return position

        # Linear drift
        linear_drift = distance_traveled * config['linear_drift_per_meter']
        linear_noise = np.random.randn(2) * linear_drift

        # Angular drift
        angular_drift = angle_turned * config['angular_drift_per_radian']
        angular_noise = np.random.randn() * angular_drift

        drifted = position.copy()
        drifted[:2] += linear_noise
        drifted[2] += angular_noise

        return drifted.astype(np.float32)

    def randomize_action(self, action: np.ndarray) -> np.ndarray:
        """Apply action noise (control noise)."""
        if not self.enabled:
            return action

        config = self.config['dynamics']['control_noise']
        if not config['enabled']:
            return action

        noise = np.random.normal(
            config['mean'],
            config['stddev'],
            action.shape
        )

        noisy_action = action + noise * action  # Multiplicative noise
        return noisy_action.astype(np.float32)

    def get_action_delay(self) -> float:
        """Get randomized action delay in seconds."""
        if not self.enabled:
            return 0.0

        config = self.config['dynamics']['action_delay']
        if not config['enabled']:
            return 0.0

        delay_ms = np.random.uniform(
            config['min_ms'],
            config['max_ms']
        )

        return delay_ms / 1000.0  # Convert to seconds

    def get_physics_randomization_params(self) -> Dict[str, Any]:
        """Get randomized physics parameters for simulation reset."""
        if not self.enabled:
            return {}

        params = {}
        physics_config = self.config['physics']

        # Robot mass
        if physics_config['robot_mass']['enabled']:
            mass_config = physics_config['robot_mass']
            params['robot_mass_factor'] = np.random.uniform(
                mass_config['min_factor'],
                mass_config['max_factor']
            )

        # Wheel friction
        if physics_config['wheel_friction']['enabled']:
            friction_config = physics_config['wheel_friction']
            params['wheel_friction'] = np.random.uniform(
                friction_config['min_value'],
                friction_config['max_value']
            )

        # Floor friction
        if physics_config['floor_friction']['enabled']:
            friction_config = physics_config['floor_friction']
            params['floor_friction'] = np.random.uniform(
                friction_config['min_value'],
                friction_config['max_value']
            )

        # Motor torque
        if physics_config['motor_torque_limit']['enabled']:
            torque_config = physics_config['motor_torque_limit']
            params['motor_torque_factor'] = np.random.uniform(
                torque_config['min_factor'],
                torque_config['max_factor']
            )

        return params
```

**Integration with existing Gym environment:**

```python
# training/training/envs/warehouser_env.py

from training.envs.randomization import DomainRandomizer

class WarehouserEnv(gym.Env):
    def __init__(self, config_path: str, randomization_config_path: str):
        super().__init__()

        # Existing initialization...

        # Add domain randomization
        self.randomizer = DomainRandomizer(randomization_config_path)
        self.action_delay_buffer = []

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)

        # Get randomized physics parameters
        physics_params = self.randomizer.get_physics_randomization_params()

        # TODO: Send physics_params to ROS simulation to update SDF/world
        # This would require extending ros_simulation to accept runtime physics changes

        # Reset action delay buffer
        self.action_delay_buffer = []

        # Get initial observation
        obs = self._get_observation()

        return obs, {}

    def step(self, action):
        # Apply action randomization
        randomized_action = self.randomizer.randomize_action(action)

        # Apply action delay
        delay = self.randomizer.get_action_delay()
        self.action_delay_buffer.append((randomized_action, delay))

        # Process delayed actions (simple approach: constant delay)
        if len(self.action_delay_buffer) > int(delay / self.dt):
            action_to_execute = self.action_delay_buffer.pop(0)[0]
        else:
            action_to_execute = np.zeros_like(action)  # No action yet

        # Execute action via ROS
        self._execute_action(action_to_execute)

        # Get observation
        obs = self._get_observation()

        # Compute reward, done, etc.
        reward = self._compute_reward(obs)
        done = self._is_done(obs)

        return obs, reward, done, False, {}

    def _get_observation(self):
        # Get raw observation from ROS
        raw_obs = self._get_raw_observation()

        # Apply randomization
        raw_obs['lidar'] = self.randomizer.randomize_lidar(
            raw_obs['lidar'],
            min_range=0.1,
            max_range=10.0
        )

        raw_obs['robot_pose'] = self.randomizer.apply_odometry_drift(
            raw_obs['robot_pose'],
            distance_traveled=self.cumulative_distance,
            angle_turned=self.cumulative_angle
        )

        return self._format_observation(raw_obs)
```

---

### 6. Testing Infrastructure

**launch_testing example:**

```python
# test/test_hardware_switching.py

import unittest
import launch
import launch_testing
import launch_testing.actions
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution
import rclpy
from std_msgs.msg import Float64MultiArray
from sensor_msgs.msg import JointState


def generate_test_description():
    """Launch robot with simulation hardware."""
    robot_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('warehouser_bringup'),
                'launch',
                'warehouse_robot.launch.py'
            ])
        ]),
        launch_arguments={
            'use_sim': 'true',
            'use_sim_time': 'false',  # Use wall time for testing
            'gui': 'false',
        }.items()
    )

    return LaunchDescription([
        robot_launch,
        launch_testing.actions.ReadyToTest(),
    ])


class TestHardwareSwitching(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()

    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()

    def setUp(self):
        self.node = rclpy.create_node('test_hardware_node')

    def tearDown(self):
        self.node.destroy_node()

    def test_joint_state_publishing(self):
        """Verify joint states are published."""
        received_msg = False

        def callback(msg):
            nonlocal received_msg
            received_msg = True
            # Verify message contains our wheel joints
            self.assertIn('left_wheel_joint', msg.name)
            self.assertIn('right_wheel_joint', msg.name)

        sub = self.node.create_subscription(
            JointState,
            '/joint_states',
            callback,
            10
        )

        # Spin for up to 5 seconds waiting for message
        timeout = 5.0
        start_time = self.node.get_clock().now()
        while not received_msg:
            rclpy.spin_once(self.node, timeout_sec=0.1)
            elapsed = (self.node.get_clock().now() - start_time).nanoseconds / 1e9
            if elapsed > timeout:
                break

        self.assertTrue(received_msg, "Joint states not received within timeout")

    def test_velocity_command_execution(self):
        """Verify velocity commands affect joint states."""
        # TODO: Publish cmd_vel, verify joint velocities change
        pass
```

---

## Summary

These templates provide copy-paste-ready code for implementing sim-to-real transfer in Warehouser:

1. **Hardware Abstraction**: ros2_control plugins for simulation and real hardware
2. **URDF Configuration**: XACRO-based conditional hardware selection
3. **Controller Setup**: DiffDriveController with separate sim/real configs
4. **Launch System**: Modular launch files for different environments
5. **Domain Randomization**: YAML-configured randomization with Python implementation
6. **Testing**: launch_testing framework for validation

**Next Steps for Warehouser:**

1. Create `warehouser_control` package with hardware interfaces
2. Update URDF to use ros2_control XACRO patterns
3. Implement domain randomization in training environment
4. Add launch-based integration tests
5. Create sim/real launch configurations

This architecture enables:
- Zero code changes to switch between sim and real
- Systematic testing at each deployment stage
- Robust policies through comprehensive randomization
- Clear separation of concerns (control, hardware, configuration)
