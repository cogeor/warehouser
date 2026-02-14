# Template Analysis: Nav2 Integration Patterns

Created: 2026-02-12

## Source

**Primary Sources:**
- Navigation2 GitHub Repository (https://github.com/ros-planning/navigation2)
- Nav2 Official Documentation (https://docs.nav2.org/)
- Nav2 Plugin Tutorials (https://docs.nav2.org/plugin_tutorials/)
- Reference Implementations: nav2_straightline_planner, nav2_dwb_controller

**Note:** `.delegate/templates/` is currently empty. This analysis is based on Nav2's official documentation and reference implementations available in the navigation2 repository.

## Pattern Analysis

### 1. Plugin Architecture Pattern

Nav2 uses a consistent plugin-based architecture across all components (planners, controllers, behavior trees). This enables runtime algorithm swapping without core code changes.

**Interface Hierarchy:**
```
nav2_core::GlobalPlanner    (Planners)
nav2_core::Controller       (Controllers)
nav2_core::CostmapLayer     (Costmap layers)
BtActionNode<ActionT>       (Behavior tree nodes)
```

**Key Insight:** RL policies can be integrated as Nav2 plugins by implementing these interfaces, maintaining compatibility with the broader Nav2 ecosystem.

### 2. Behavior Tree Coordination Pattern

Nav2 uses BehaviorTree.CPP for hierarchical task coordination, replacing traditional state machines.

**Minimal Functional Example:**
```xml
<root main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <PipelineSequence name="NavigateWithReplanning">
      <DistanceController distance="1.0">
        <ComputePathToPose goal="{goal}" path="{path}"/>
      </DistanceController>
      <FollowPath path="{path}"/>
    </PipelineSequence>
  </BehaviorTree>
</root>
```

**Pattern Elements:**
- Blackboard variables (`{goal}`, `{path}`) for inter-node data sharing
- Control nodes (PipelineSequence, Recovery, RoundRobin)
- Action nodes (ComputePathToPose, FollowPath)
- Decorator nodes (DistanceController for replanning triggers)

**Application to Warehouser:**
Could replace or augment state machine in task_manager with BT-based coordination:
```xml
<BehaviorTree ID="PickAndPlace">
  <Sequence>
    <NavigateToPose goal="{pickup_pose}"/>    <!-- Classical Nav2 -->
    <PickObject object_id="{target_id}"/>      <!-- Custom RL action -->
    <NavigateToPose goal="{dropoff_pose}"/>    <!-- Classical Nav2 -->
    <DropObject/>                               <!-- Custom action -->
  </Sequence>
</BehaviorTree>
```

### 3. Costmap Layer System Pattern

Nav2 uses a layered costmap architecture for sensor fusion and obstacle representation.

**Standard Layer Stack:**
1. **StaticLayer** - Pre-loaded map (walls, permanent obstacles)
2. **ObstacleLayer** - Real-time sensor data (lidar, depth cameras)
3. **InflationLayer** - Exponential decay cost function around obstacles
4. **VoxelLayer** - 3D obstacle tracking with clearing
5. **RangeSensorLayer** - Sonar/IR integration

**Custom Layer Pattern:**
Derive from `nav2_costmap_2d::Layer`, implement:
- `onInitialize()` - Setup parameters
- `updateBounds()` - Define affected region
- `updateCosts()` - Modify costmap values

**RL Integration Pattern:**
Create custom costmap layer that integrates learned obstacle costs from RL policy:
```cpp
class RLCostmapLayer : public nav2_costmap_2d::Layer
{
  void updateCosts(
    nav2_costmap_2d::Costmap2D& master_grid,
    int min_i, int min_j, int max_i, int max_j) override
  {
    // Query RL model for learned costs based on observations
    // Overlay onto master costmap
  }
};
```

### 4. Global Planner Plugin Pattern

Global planners compute long-range paths from start to goal.

**Interface Definition (nav2_core::GlobalPlanner):**
```cpp
class MyPlanner : public nav2_core::GlobalPlanner
{
public:
  // Lifecycle methods
  void configure(
    const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
    std::string name,
    std::shared_ptr<tf2_ros::Buffer> tf,
    std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros) override;

  void cleanup() override;
  void activate() override;
  void deactivate() override;

  // Core planning method
  nav_msgs::msg::Path createPlan(
    const geometry_msgs::msg::PoseStamped& start,
    const geometry_msgs::msg::PoseStamped& goal) override;

private:
  std::shared_ptr<tf2_ros::Buffer> tf_;
  nav2_costmap_2d::Costmap2D* costmap_;
  rclcpp::Logger logger_{rclcpp::get_logger("MyPlanner")};
};
```

**Configuration Method Pattern:**
```cpp
void MyPlanner::configure(
  const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
  std::string name, std::shared_ptr<tf2_ros::Buffer> tf,
  std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros)
{
  auto node = parent.lock();
  logger_ = node->get_logger();
  tf_ = tf;
  name_ = name;
  costmap_ = costmap_ros->getCostmap();
  global_frame_ = costmap_ros->getGlobalFrameID();

  // Declare and retrieve plugin-specific parameters
  nav2_util::declare_parameter_if_not_declared(
    node, name_ + ".interpolation_resolution",
    rclcpp::ParameterValue(0.1));
  node->get_parameter(name_ + ".interpolation_resolution",
    interpolation_resolution_);
}
```

**Plugin Export Pattern:**
```cpp
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(my_namespace::MyPlanner, nav2_core::GlobalPlanner)
```

**XML Descriptor (global_planner_plugin.xml):**
```xml
<library path="my_planner_plugin">
  <class type="my_namespace::MyPlanner"
         base_class_type="nav2_core::GlobalPlanner">
    <description>Custom planner description</description>
  </class>
</library>
```

**CMakeLists.txt Export:**
```cmake
pluginlib_export_plugin_description_file(nav2_core global_planner_plugin.xml)
```

**YAML Configuration:**
```yaml
planner_server:
  ros__parameters:
    plugins: ["GridBased"]
    GridBased:
      plugin: "my_namespace::MyPlanner"
      interpolation_resolution: 0.1
```

**Application to Warehouser:**
Could create RL-based global planner plugin that uses learned path preferences from training:
- Input: Start pose, goal pose, costmap
- Output: Kinematically feasible path optimized for learned efficiency metrics
- Fallback: Classical Smac 2D A* if RL model fails

### 5. Local Controller Plugin Pattern

Controllers execute local trajectory control to follow planned paths.

**Interface Definition (nav2_core::Controller):**
```cpp
class MyController : public nav2_core::Controller
{
public:
  // Lifecycle methods
  void configure(
    const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
    std::string name,
    std::shared_ptr<tf2_ros::Buffer> tf,
    std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros) override;

  void cleanup() override;
  void activate() override;
  void deactivate() override;

  // Path handling
  void newPathReceived() override;

  // Core control method
  geometry_msgs::msg::TwistStamped computeVelocityCommands(
    const geometry_msgs::msg::PoseStamped& pose,
    const geometry_msgs::msg::Twist& velocity,
    nav2_core::GoalChecker* goal_checker,
    std::vector<geometry_msgs::msg::PoseStamped>& transformed_plan,
    const geometry_msgs::msg::PoseStamped& goal) override;

  // Optional methods
  void cancel() override;
  void setSpeedLimit(const double& speed_limit, const bool& percentage) override;

private:
  std::shared_ptr<tf2_ros::Buffer> tf_;
  nav2_costmap_2d::Costmap2D* costmap_;
};
```

**Velocity Command Computation Pattern:**
```cpp
geometry_msgs::msg::TwistStamped MyController::computeVelocityCommands(
  const geometry_msgs::msg::PoseStamped& pose,
  const geometry_msgs::msg::Twist& velocity,
  nav2_core::GoalChecker* goal_checker,
  std::vector<geometry_msgs::msg::PoseStamped>& transformed_plan,
  const geometry_msgs::msg::PoseStamped& goal)
{
  // Check if goal reached
  if (goal_checker->isGoalReached(pose.pose, goal.pose, velocity)) {
    return geometry_msgs::msg::TwistStamped(); // Zero velocity
  }

  // Compute velocity commands using your algorithm
  geometry_msgs::msg::TwistStamped cmd_vel;
  cmd_vel.header.frame_id = pose.header.frame_id;
  cmd_vel.header.stamp = node_->now();

  // Your control logic here
  cmd_vel.twist.linear.x = computedLinearVelocity;
  cmd_vel.twist.angular.z = computedAngularVelocity;

  return cmd_vel;
}
```

**RL Controller Integration Pattern:**
```cpp
class RLController : public nav2_core::Controller
{
  geometry_msgs::msg::TwistStamped computeVelocityCommands(...) override
  {
    // Extract observations from pose, velocity, costmap, path
    auto observations = buildObservations(pose, velocity, costmap_,
                                          transformed_plan);

    // Run RL policy inference (ONNX model)
    auto action = rl_model_->predict(observations);

    // Convert RL action to velocity command
    geometry_msgs::msg::TwistStamped cmd_vel;
    cmd_vel.twist.linear.x = action.linear_velocity;
    cmd_vel.twist.angular.z = action.angular_velocity;

    return cmd_vel;
  }
};
```

**YAML Configuration:**
```yaml
controller_server:
  ros__parameters:
    controller_plugins: ["FollowPath"]
    FollowPath:
      plugin: "my_namespace::RLController"
      model_path: "/path/to/rl_model.onnx"
      observation_range: 5.0
```

### 6. BT Navigator Configuration Pattern

The BT Navigator orchestrates navigation tasks using behavior trees.

**Complete Configuration Example:**
```yaml
bt_navigator:
  ros__parameters:
    # Available navigator plugins
    navigators: ['navigate_to_pose', 'navigate_through_poses']

    # Behavior tree XML files
    default_nav_to_pose_bt_xml: "$(find-pkg-share my_package)/behavior_trees/navigate_to_pose.xml"
    default_nav_through_poses_bt_xml: "$(find-pkg-share my_package)/behavior_trees/navigate_through_poses.xml"

    # Execution timing
    bt_loop_duration: 10        # milliseconds
    default_server_timeout: 20  # milliseconds
    default_cancel_timeout: 50  # milliseconds

    # Frame configuration
    global_frame: map
    robot_base_frame: base_link
    transform_tolerance: 0.1
    odom_topic: odom

    # Custom plugin libraries (only if using custom BT nodes)
    plugin_lib_names:
      - my_custom_bt_nodes

    # NavigateToPose configuration
    navigate_to_pose:
      plugin: "nav2_bt_navigator::NavigateToPoseNavigator"
      goal_blackboard_id: goal
      path_blackboard_id: path
      tracking_feedback_blackboard_id: tracking_feedback
      groot_server_port: 1667

    # NavigateThroughPoses configuration
    navigate_through_poses:
      plugin: "nav2_bt_navigator::NavigateThroughPosesNavigator"
      goals_blackboard_id: goals
      waypoint_statuses_blackboard_id: waypoint_statuses
      groot_server_port: 1669  # Different port to avoid conflicts
```

### 7. Costmap Configuration Pattern

Costmaps represent the environment for planning and control.

**Global Costmap Configuration:**
```yaml
global_costmap:
  global_costmap:
    ros__parameters:
      # Update rates
      update_frequency: 1.0
      publish_frequency: 1.0

      # Frame configuration
      global_frame: map
      robot_base_frame: base_link

      # Costmap properties
      width: 50
      height: 50
      resolution: 0.05
      robot_radius: 0.22

      # Layer plugins
      plugins: ["static_layer", "obstacle_layer", "inflation_layer"]

      # Static layer (pre-loaded map)
      static_layer:
        plugin: "nav2_costmap_2d::StaticLayer"
        enabled: true
        map_subscribe_transient_local: true

      # Obstacle layer (real-time sensors)
      obstacle_layer:
        plugin: "nav2_costmap_2d::ObstacleLayer"
        enabled: true
        observation_sources: scan
        scan:
          topic: /scan
          max_obstacle_height: 2.0
          clearing: true
          marking: true
          data_type: "LaserScan"
          raytrace_max_range: 10.0
          raytrace_min_range: 0.0
          obstacle_max_range: 9.5
          obstacle_min_range: 0.0

      # Inflation layer (safety buffer)
      inflation_layer:
        plugin: "nav2_costmap_2d::InflationLayer"
        enabled: true
        inflation_radius: 0.55
        cost_scaling_factor: 3.0
```

**Local Costmap Configuration (Rolling Window):**
```yaml
local_costmap:
  local_costmap:
    ros__parameters:
      # Update rates (higher frequency for local planning)
      update_frequency: 5.0
      publish_frequency: 2.0

      # Frame configuration
      global_frame: odom
      robot_base_frame: base_link

      # Rolling window
      rolling_window: true
      width: 5
      height: 5
      resolution: 0.05
      robot_radius: 0.22

      # Same plugin structure as global costmap
      plugins: ["obstacle_layer", "inflation_layer"]

      obstacle_layer:
        plugin: "nav2_costmap_2d::ObstacleLayer"
        enabled: true
        observation_sources: scan
        scan:
          topic: /scan
          max_obstacle_height: 2.0
          clearing: true
          marking: true
          data_type: "LaserScan"

      inflation_layer:
        plugin: "nav2_costmap_2d::InflationLayer"
        enabled: true
        inflation_radius: 0.55
        cost_scaling_factor: 3.0
```

**Custom RL Costmap Layer Configuration:**
```yaml
local_costmap:
  local_costmap:
    ros__parameters:
      plugins: ["obstacle_layer", "rl_layer", "inflation_layer"]

      # Custom RL layer
      rl_layer:
        plugin: "warehouser_navigation::RLCostmapLayer"
        enabled: true
        model_path: "/path/to/costmap_model.onnx"
        blend_alpha: 0.5  # Blend with classical costmap
```

### 8. Planner Server Configuration Pattern

The planner server manages global path planning.

**Complete Configuration:**
```yaml
planner_server:
  ros__parameters:
    # Action server configuration
    expected_planner_frequency: 20.0

    # Planner plugin selection
    plugins: ["GridBased"]

    # GridBased planner (Smac 2D A*)
    GridBased:
      plugin: "nav2_smac_planner/SmacPlanner2D"
      tolerance: 0.125                 # tolerance for planning if unable to reach exact pose
      downsample_costmap: false        # whether to downsample costmap to another resolution
      downsampling_factor: 1           # downsampling factor
      allow_unknown: true              # allow traveling in unknown space
      max_iterations: 1000000          # maximum total iterations to search
      max_on_approach_iterations: 1000 # maximum iterations after finding first solution
      max_planning_time: 5.0           # max time in s for planner to plan, smooth
      motion_model_for_search: "MOORE" # 2D Moore (8-connected), Von Neumann (4-connected)
      angle_quantization_bins: 72      # number of angle bins for search
      analytic_expansion_ratio: 3.5    # ratio to attempt analytic expansion
      analytic_expansion_max_length: 3.0 # max length for analytic expansion
      minimum_turning_radius: 0.40     # minimum turning radius for search
      reverse_penalty: 2.0             # penalty for reverse motion
      change_penalty: 0.0              # penalty for direction changes
      non_straight_penalty: 1.2        # penalty for non-straight motion
      cost_penalty: 2.0                # cost multiplier for high-cost areas
      retrospective_penalty: 0.015     # retrospective penalty
      lookup_table_size: 20.0          # size of collision checking lookup table
      cache_obstacle_heuristic: false  # whether to cache heuristic
      smooth_path: true                # whether to smooth path
```

**Hybrid Classical + RL Planner Configuration:**
```yaml
planner_server:
  ros__parameters:
    plugins: ["Classical", "RL"]

    # Smac 2D A* for open environments
    Classical:
      plugin: "nav2_smac_planner/SmacPlanner2D"
      # ... parameters as above

    # RL-based planner for complex scenarios
    RL:
      plugin: "warehouser_navigation::RLPlanner"
      model_path: "/path/to/planner_model.onnx"
      planning_horizon: 10.0
      fallback_to_classical: true
```

### 9. Controller Server Configuration Pattern

The controller server manages local trajectory execution.

**DWB Controller Configuration:**
```yaml
controller_server:
  ros__parameters:
    # Action server configuration
    controller_frequency: 20.0
    min_x_velocity_threshold: 0.001
    min_y_velocity_threshold: 0.5
    min_theta_velocity_threshold: 0.001
    failure_tolerance: 0.3
    progress_checker_plugins: ["progress_checker"]
    goal_checker_plugins: ["general_goal_checker"]

    # Controller plugins
    controller_plugins: ["FollowPath"]

    # Progress checker
    progress_checker:
      plugin: "nav2_controller::SimpleProgressChecker"
      required_movement_radius: 0.5
      movement_time_allowance: 10.0

    # Goal checker
    general_goal_checker:
      plugin: "nav2_controller::SimpleGoalChecker"
      xy_goal_tolerance: 0.25
      yaw_goal_tolerance: 0.25
      stateful: true

    # DWB controller
    FollowPath:
      plugin: "dwb_core::DWBLocalPlanner"
      debug_trajectory_details: true
      min_vel_x: 0.0
      min_vel_y: 0.0
      max_vel_x: 0.26
      max_vel_y: 0.0
      max_vel_theta: 1.0
      min_speed_xy: 0.0
      max_speed_xy: 0.26
      min_speed_theta: 0.0
      acc_lim_x: 2.5
      acc_lim_y: 0.0
      acc_lim_theta: 3.2
      decel_lim_x: -2.5
      decel_lim_y: 0.0
      decel_lim_theta: -3.2
      vx_samples: 20
      vy_samples: 5
      vtheta_samples: 20
      sim_time: 1.7
      linear_granularity: 0.05
      angular_granularity: 0.025
      transform_tolerance: 0.2
      xy_goal_tolerance: 0.25
      trans_stopped_velocity: 0.25
      short_circuit_trajectory_evaluation: true
      stateful: true

      # DWB critics (scoring functions)
      critics: ["RotateToGoal", "Oscillation", "BaseObstacle", "GoalAlign", "PathAlign", "PathDist", "GoalDist"]
      BaseObstacle.scale: 0.02
      PathAlign.scale: 32.0
      PathAlign.forward_point_distance: 0.1
      GoalAlign.scale: 24.0
      GoalAlign.forward_point_distance: 0.1
      PathDist.scale: 32.0
      GoalDist.scale: 24.0
      RotateToGoal.scale: 32.0
      RotateToGoal.slowing_factor: 5.0
      RotateToGoal.lookahead_time: -1.0
```

**RL Controller Configuration:**
```yaml
controller_server:
  ros__parameters:
    controller_plugins: ["RLFollowPath"]

    RLFollowPath:
      plugin: "warehouser_navigation::RLController"
      model_path: "/path/to/controller_model.onnx"
      observation_range: 5.0
      max_linear_velocity: 0.26
      max_angular_velocity: 1.0
      goal_tolerance_xy: 0.25
      goal_tolerance_theta: 0.25
```

### 10. Custom Behavior Tree Node Pattern

Create application-specific BT nodes for warehouse tasks.

**Custom Action Node Implementation:**
```cpp
#include "nav2_behavior_tree/bt_action_node.hpp"
#include "warehouser_msgs/action/pick_object.hpp"

namespace warehouser_bt
{

class PickObjectAction : public nav2_behavior_tree::BtActionNode<
  warehouser_msgs::action::PickObject>
{
public:
  using ActionT = warehouser_msgs::action::PickObject;

  PickObjectAction(
    const std::string& xml_tag_name,
    const std::string& action_name,
    const BT::NodeConfiguration& conf)
  : BtActionNode<ActionT>(xml_tag_name, action_name, conf)
  {}

  // Provide goal to action server
  void on_tick() override
  {
    // Read from blackboard
    std::string object_id;
    getInput("object_id", object_id);

    goal_.object_id = object_id;
  }

  // Handle result
  BT::NodeStatus on_success() override
  {
    RCLCPP_INFO(node_->get_logger(), "Successfully picked object");
    setOutput("success", true);
    return BT::NodeStatus::SUCCESS;
  }

  BT::NodeStatus on_aborted() override
  {
    RCLCPP_ERROR(node_->get_logger(), "Pick object aborted");
    return BT::NodeStatus::FAILURE;
  }

  BT::NodeStatus on_cancelled() override
  {
    RCLCPP_WARN(node_->get_logger(), "Pick object cancelled");
    return BT::NodeStatus::FAILURE;
  }

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("object_id", "ID of object to pick"),
      BT::OutputPort<bool>("success", "Whether pick succeeded")
    };
  }
};

} // namespace warehouser_bt

// Register plugin
#include "behaviortree_cpp_v3/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<warehouser_bt::PickObjectAction>("PickObject");
}
```

**Using Custom Node in XML:**
```xml
<BehaviorTree ID="WarehouseTask">
  <Sequence>
    <ComputePathToPose goal="{pickup_location}" path="{path}"/>
    <FollowPath path="{path}"/>
    <PickObject object_id="{target_object}" success="{picked}"/>
    <ComputePathToPose goal="{dropoff_location}" path="{path}"/>
    <FollowPath path="{path}"/>
    <DropObject success="{dropped}"/>
  </Sequence>
</BehaviorTree>
```

### 11. Multi-Robot Navigation Pattern

Nav2 supports multi-robot systems through namespacing and coordination.

**Launch File Pattern (Python):**
```python
from launch import LaunchDescription
from launch.actions import GroupAction, DeclareLaunchArgument
from launch_ros.actions import Node, PushRosNamespace
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    robots = ['robot1', 'robot2', 'robot3']

    launch_actions = []
    for robot_name in robots:
        group = GroupAction([
            PushRosNamespace(robot_name),

            # Nav2 stack for this robot
            Node(
                package='nav2_planner',
                executable='planner_server',
                name='planner_server',
                parameters=[get_params_file(robot_name)]
            ),
            Node(
                package='nav2_controller',
                executable='controller_server',
                name='controller_server',
                parameters=[get_params_file(robot_name)]
            ),
            Node(
                package='nav2_bt_navigator',
                executable='bt_navigator',
                name='bt_navigator',
                parameters=[get_params_file(robot_name)]
            ),
            # ... other Nav2 nodes
        ])
        launch_actions.append(group)

    return LaunchDescription(launch_actions)
```

**Multi-Robot Parameter Pattern:**
```yaml
# robot1_nav2_params.yaml
bt_navigator:
  ros__parameters:
    groot_server_port: 1667  # Unique port per robot
    global_frame: map
    robot_base_frame: robot1/base_link
    odom_topic: /robot1/odom

# robot2_nav2_params.yaml
bt_navigator:
  ros__parameters:
    groot_server_port: 1668  # Different port
    global_frame: map
    robot_base_frame: robot2/base_link
    odom_topic: /robot2/odom
```

**Shared Costmap Pattern (Fleet Coordination):**
```yaml
local_costmap:
  local_costmap:
    ros__parameters:
      plugins: ["obstacle_layer", "other_robots_layer", "inflation_layer"]

      # Other robots as dynamic obstacles
      other_robots_layer:
        plugin: "nav2_costmap_2d::ObstacleLayer"
        enabled: true
        observation_sources: fleet_positions
        fleet_positions:
          topic: /fleet/robot_positions  # Shared topic
          data_type: "PointCloud2"
          marking: true
          clearing: false
          obstacle_max_range: 5.0
```

### 12. Launch File Pattern

Nav2 uses hierarchical launch files for system bringup.

**Main Navigation Launch:**
```python
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    pkg_share = get_package_share_directory('my_navigation')
    params_file = os.path.join(pkg_share, 'config', 'nav2_params.yaml')

    return LaunchDescription([
        Node(
            package='nav2_planner',
            executable='planner_server',
            name='planner_server',
            output='screen',
            parameters=[params_file]
        ),
        Node(
            package='nav2_controller',
            executable='controller_server',
            name='controller_server',
            output='screen',
            parameters=[params_file]
        ),
        Node(
            package='nav2_behaviors',
            executable='behavior_server',
            name='behavior_server',
            output='screen',
            parameters=[params_file]
        ),
        Node(
            package='nav2_bt_navigator',
            executable='bt_navigator',
            name='bt_navigator',
            output='screen',
            parameters=[params_file]
        ),
        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager_navigation',
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'autostart': True,
                'node_names': [
                    'planner_server',
                    'controller_server',
                    'behavior_server',
                    'bt_navigator'
                ]
            }]
        ),
    ])
```

### 13. Hybrid Classical/RL Integration Pattern

Meta-planner switches between classical and RL algorithms based on scenario complexity.

**Architecture:**
```
Sensor Input → Scenario Classifier
                ↓
        [Simple] | [Complex]
           ↓            ↓
      Nav2 DWB    RL Controller
           ↓            ↓
        Unified Velocity Command
```

**Implementation Pattern:**
```cpp
class HybridController : public nav2_core::Controller
{
  geometry_msgs::msg::TwistStamped computeVelocityCommands(...) override
  {
    // Classify scenario complexity
    double obstacle_density = computeObstacleDensity(costmap_);
    double path_curvature = computePathCurvature(transformed_plan);

    bool use_rl = (obstacle_density > rl_threshold_) ||
                  (path_curvature > curvature_threshold_);

    if (use_rl) {
      RCLCPP_DEBUG(logger_, "Using RL controller for complex scenario");
      return rl_controller_->computeVelocityCommands(...);
    } else {
      RCLCPP_DEBUG(logger_, "Using DWB controller for simple scenario");
      return dwb_controller_->computeVelocityCommands(...);
    }
  }
};
```

**Configuration:**
```yaml
controller_server:
  ros__parameters:
    controller_plugins: ["HybridController"]

    HybridController:
      plugin: "warehouser_navigation::HybridController"

      # Switching thresholds
      obstacle_density_threshold: 0.3
      path_curvature_threshold: 0.5

      # Classical controller config
      dwb:
        max_vel_x: 0.26
        # ... DWB parameters

      # RL controller config
      rl:
        model_path: "/path/to/model.onnx"
        # ... RL parameters
```

## Application to Warehouser

### Phase 1: Interface Compatibility

**Goal:** Make Warehouser navigation Nav2-compatible without full integration.

**Actions:**
1. Define `ComputePathToPose` service interface matching Nav2 conventions
2. Define `FollowPath` action interface matching Nav2 conventions
3. Implement costmap representation in warehouser_observations

**Code Snippets:**
```cpp
// In ros_rl_bridge or new ros_navigation package
class NavigationInterface : public rclcpp::Node
{
  // Nav2-compatible service
  rclcpp::Service<nav2_msgs::srv::ComputePathToPose>::SharedPtr plan_service_;

  // Nav2-compatible action
  rclcpp_action::Server<nav2_msgs::action::FollowPath>::SharedPtr follow_action_;

  // Convert to internal format and call existing RL bridge
  void handleComputePath(
    const std::shared_ptr<nav2_msgs::srv::ComputePathToPose::Request> request,
    std::shared_ptr<nav2_msgs::srv::ComputePathToPose::Response> response)
  {
    // Convert to internal RLStep call
    // Return path in nav_msgs::msg::Path format
  }
};
```

### Phase 2: Behavior Tree Integration

**Goal:** Replace or augment task_manager state machine with BT coordination.

**Warehouse Pick-and-Place BT:**
```xml
<root main_tree_to_execute="PickAndPlace">
  <BehaviorTree ID="PickAndPlace">
    <Sequence>
      <!-- Navigate to pickup location -->
      <SubTree ID="NavigateToLocation" target="{pickup_pose}"/>

      <!-- Pick object using RL policy -->
      <PickObject object_id="{target_object}"/>

      <!-- Navigate to dropoff location -->
      <SubTree ID="NavigateToLocation" target="{dropoff_pose}"/>

      <!-- Drop object -->
      <DropObject/>

      <!-- Return to idle position -->
      <SubTree ID="NavigateToLocation" target="{idle_pose}"/>
    </Sequence>
  </BehaviorTree>

  <BehaviorTree ID="NavigateToLocation">
    <PipelineSequence name="NavigateWithReplanning">
      <DistanceController distance="1.0">
        <ComputePathToPose goal="{target}" path="{path}"/>
      </DistanceController>
      <FollowPath path="{path}"/>
    </PipelineSequence>
  </BehaviorTree>
</root>
```

### Phase 3: Hybrid Planner/Controller

**Goal:** Combine Nav2 classical algorithms with existing RL policies.

**Architecture Options:**

**Option A: RL Controller + Nav2 Planner**
```yaml
planner_server:
  ros__parameters:
    plugins: ["SmacPlanner"]
    SmacPlanner:
      plugin: "nav2_smac_planner/SmacPlanner2D"

controller_server:
  ros__parameters:
    controller_plugins: ["RLController"]
    RLController:
      plugin: "warehouser_navigation::RLController"
      model_path: "/path/to/trained_ppo_model.onnx"
```

**Option B: Nav2 Controller + RL Planner**
```yaml
planner_server:
  ros__parameters:
    plugins: ["RLPlanner"]
    RLPlanner:
      plugin: "warehouser_navigation::RLPlanner"
      model_path: "/path/to/planning_model.onnx"

controller_server:
  ros__parameters:
    controller_plugins: ["DWB"]
    DWB:
      plugin: "dwb_core::DWBLocalPlanner"
```

**Option C: Meta-Switching Hybrid**
```yaml
controller_server:
  ros__parameters:
    controller_plugins: ["HybridController"]
    HybridController:
      plugin: "warehouser_navigation::HybridController"
      switch_based_on: obstacle_density
      threshold: 0.3
```

### Phase 4: Full Nav2 Plugin Ecosystem

**Goal:** Implement RL policies as drop-in Nav2 plugins.

**Package Structure:**
```
warehouser_navigation/
├── include/warehouser_navigation/
│   ├── rl_planner.hpp
│   ├── rl_controller.hpp
│   └── rl_costmap_layer.hpp
├── src/
│   ├── rl_planner.cpp
│   ├── rl_controller.cpp
│   └── rl_costmap_layer.cpp
├── plugins/
│   └── plugin_description.xml
├── config/
│   └── nav2_params.yaml
└── CMakeLists.txt
```

**Plugin Description (plugin_description.xml):**
```xml
<library path="warehouser_navigation">
  <class type="warehouser_navigation::RLPlanner"
         base_class_type="nav2_core::GlobalPlanner">
    <description>RL-based global planner for warehouse navigation</description>
  </class>
  <class type="warehouser_navigation::RLController"
         base_class_type="nav2_core::Controller">
    <description>RL-based local controller for obstacle avoidance</description>
  </class>
  <class type="warehouser_navigation::RLCostmapLayer"
         base_class_type="nav2_costmap_2d::Layer">
    <description>Learned costmap layer from RL training</description>
  </class>
</library>
```

### Phase 5: Multi-Robot Coordination

**Goal:** Support multiple robots using Nav2's namespace pattern.

**Warehouser Multi-Robot Launch:**
```python
def generate_multi_robot_launch():
    robots = [
        {'name': 'robot_1', 'x': 0.0, 'y': 0.0},
        {'name': 'robot_2', 'x': 2.0, 'y': 0.0},
        {'name': 'robot_3', 'x': 4.0, 'y': 0.0},
    ]

    launch_actions = []
    for robot in robots:
        robot_namespace = robot['name']

        group = GroupAction([
            PushRosNamespace(robot_namespace),

            # Spawn robot in simulation
            Node(
                package='ros_simulation',
                executable='entity_spawner',
                parameters=[{
                    'robot_id': robot['name'],
                    'x': robot['x'],
                    'y': robot['y']
                }]
            ),

            # RL bridge for this robot
            Node(
                package='ros_rl_bridge',
                executable='rl_bridge_node',
                parameters=[{
                    'robot_id': robot['name'],
                }]
            ),

            # Nav2 stack (if integrated)
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(nav2_bringup_dir, 'bringup_launch.py')
                ),
                launch_arguments={
                    'namespace': robot_namespace,
                    'use_namespace': 'True',
                    'params_file': get_robot_params(robot['name'])
                }.items()
            ),
        ])
        launch_actions.append(group)

    return LaunchDescription(launch_actions)
```

## Copy-Paste Ready Code Snippets

### 1. Minimal Custom Planner Plugin

**File: my_planner.hpp**
```cpp
#pragma once
#include "nav2_core/global_planner.hpp"
#include "nav2_util/lifecycle_node.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"

namespace my_navigation
{

class MyPlanner : public nav2_core::GlobalPlanner
{
public:
  void configure(
    const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
    std::string name,
    std::shared_ptr<tf2_ros::Buffer> tf,
    std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros) override;

  void cleanup() override;
  void activate() override;
  void deactivate() override;

  nav_msgs::msg::Path createPlan(
    const geometry_msgs::msg::PoseStamped& start,
    const geometry_msgs::msg::PoseStamped& goal) override;

private:
  std::shared_ptr<tf2_ros::Buffer> tf_;
  rclcpp_lifecycle::LifecycleNode::WeakPtr node_;
  nav2_costmap_2d::Costmap2D* costmap_;
  std::string name_;
  rclcpp::Logger logger_{rclcpp::get_logger("MyPlanner")};
};

} // namespace my_navigation
```

**File: my_planner.cpp**
```cpp
#include "my_navigation/my_planner.hpp"
#include "nav2_util/node_utils.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace my_navigation
{

void MyPlanner::configure(
  const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
  std::string name,
  std::shared_ptr<tf2_ros::Buffer> tf,
  std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros)
{
  auto node = parent.lock();
  logger_ = node->get_logger();
  tf_ = tf;
  node_ = parent;
  name_ = name;
  costmap_ = costmap_ros->getCostmap();

  RCLCPP_INFO(logger_, "Configured MyPlanner");
}

void MyPlanner::cleanup()
{
  RCLCPP_INFO(logger_, "Cleaning up MyPlanner");
}

void MyPlanner::activate()
{
  RCLCPP_INFO(logger_, "Activating MyPlanner");
}

void MyPlanner::deactivate()
{
  RCLCPP_INFO(logger_, "Deactivating MyPlanner");
}

nav_msgs::msg::Path MyPlanner::createPlan(
  const geometry_msgs::msg::PoseStamped& start,
  const geometry_msgs::msg::PoseStamped& goal)
{
  nav_msgs::msg::Path path;
  path.header = start.header;

  // Your planning algorithm here
  // For example: straight line interpolation
  path.poses.push_back(start);
  path.poses.push_back(goal);

  return path;
}

} // namespace my_navigation

PLUGINLIB_EXPORT_CLASS(my_navigation::MyPlanner, nav2_core::GlobalPlanner)
```

### 2. Minimal Custom Controller Plugin

**File: my_controller.hpp**
```cpp
#pragma once
#include "nav2_core/controller.hpp"
#include "nav2_util/lifecycle_node.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"

namespace my_navigation
{

class MyController : public nav2_core::Controller
{
public:
  void configure(
    const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
    std::string name,
    std::shared_ptr<tf2_ros::Buffer> tf,
    std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros) override;

  void cleanup() override;
  void activate() override;
  void deactivate() override;

  geometry_msgs::msg::TwistStamped computeVelocityCommands(
    const geometry_msgs::msg::PoseStamped& pose,
    const geometry_msgs::msg::Twist& velocity,
    nav2_core::GoalChecker* goal_checker,
    std::vector<geometry_msgs::msg::PoseStamped>& transformed_plan,
    const geometry_msgs::msg::PoseStamped& goal) override;

  void newPathReceived() override {}

private:
  rclcpp_lifecycle::LifecycleNode::WeakPtr node_;
  std::shared_ptr<tf2_ros::Buffer> tf_;
  nav2_costmap_2d::Costmap2D* costmap_;
  rclcpp::Logger logger_{rclcpp::get_logger("MyController")};
  double max_linear_vel_{0.5};
  double max_angular_vel_{1.0};
};

} // namespace my_navigation
```

**File: my_controller.cpp**
```cpp
#include "my_navigation/my_controller.hpp"
#include "nav2_util/node_utils.hpp"
#include "pluginlib/class_list_macros.hpp"
#include <cmath>

namespace my_navigation
{

void MyController::configure(
  const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent,
  std::string name,
  std::shared_ptr<tf2_ros::Buffer> tf,
  std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros)
{
  auto node = parent.lock();
  logger_ = node->get_logger();
  node_ = parent;
  tf_ = tf;
  costmap_ = costmap_ros->getCostmap();

  nav2_util::declare_parameter_if_not_declared(
    node, name + ".max_linear_vel", rclcpp::ParameterValue(0.5));
  nav2_util::declare_parameter_if_not_declared(
    node, name + ".max_angular_vel", rclcpp::ParameterValue(1.0));

  node->get_parameter(name + ".max_linear_vel", max_linear_vel_);
  node->get_parameter(name + ".max_angular_vel", max_angular_vel_);

  RCLCPP_INFO(logger_, "Configured MyController");
}

void MyController::cleanup()
{
  RCLCPP_INFO(logger_, "Cleaning up MyController");
}

void MyController::activate()
{
  RCLCPP_INFO(logger_, "Activating MyController");
}

void MyController::deactivate()
{
  RCLCPP_INFO(logger_, "Deactivating MyController");
}

geometry_msgs::msg::TwistStamped MyController::computeVelocityCommands(
  const geometry_msgs::msg::PoseStamped& pose,
  const geometry_msgs::msg::Twist& velocity,
  nav2_core::GoalChecker* goal_checker,
  std::vector<geometry_msgs::msg::PoseStamped>& transformed_plan,
  const geometry_msgs::msg::PoseStamped& goal)
{
  geometry_msgs::msg::TwistStamped cmd_vel;
  cmd_vel.header = pose.header;

  // Check if goal reached
  if (goal_checker->isGoalReached(pose.pose, goal.pose, velocity)) {
    return cmd_vel; // Zero velocity
  }

  // Simple proportional controller toward goal
  double dx = goal.pose.position.x - pose.pose.position.x;
  double dy = goal.pose.position.y - pose.pose.position.y;
  double distance = std::sqrt(dx * dx + dy * dy);

  double target_yaw = std::atan2(dy, dx);

  // Extract current yaw from quaternion
  double siny_cosp = 2.0 * (pose.pose.orientation.w * pose.pose.orientation.z +
                            pose.pose.orientation.x * pose.pose.orientation.y);
  double cosy_cosp = 1.0 - 2.0 * (pose.pose.orientation.y * pose.pose.orientation.y +
                                  pose.pose.orientation.z * pose.pose.orientation.z);
  double current_yaw = std::atan2(siny_cosp, cosy_cosp);

  double angle_error = target_yaw - current_yaw;
  // Normalize to [-pi, pi]
  while (angle_error > M_PI) angle_error -= 2.0 * M_PI;
  while (angle_error < -M_PI) angle_error += 2.0 * M_PI;

  // Proportional control
  cmd_vel.twist.linear.x = std::min(distance * 0.5, max_linear_vel_);
  cmd_vel.twist.angular.z = std::clamp(
    angle_error * 2.0, -max_angular_vel_, max_angular_vel_);

  return cmd_vel;
}

} // namespace my_navigation

PLUGINLIB_EXPORT_CLASS(my_navigation::MyController, nav2_core::Controller)
```

### 3. Custom BT Action Node

**File: custom_bt_node.hpp**
```cpp
#pragma once
#include "nav2_behavior_tree/bt_action_node.hpp"
#include "warehouser_msgs/action/warehouse_task.hpp"

namespace warehouser_bt
{

class WarehouseTaskAction : public nav2_behavior_tree::BtActionNode<
  warehouser_msgs::action::WarehouseTask>
{
public:
  using ActionT = warehouser_msgs::action::WarehouseTask;

  WarehouseTaskAction(
    const std::string& xml_tag_name,
    const std::string& action_name,
    const BT::NodeConfiguration& conf)
  : BtActionNode<ActionT>(xml_tag_name, action_name, conf)
  {}

  void on_tick() override
  {
    getInput("task_id", goal_.task_id);
    getInput("object_id", goal_.object_id);
  }

  BT::NodeStatus on_success() override
  {
    setOutput("result", result_.result->success);
    return BT::NodeStatus::SUCCESS;
  }

  BT::NodeStatus on_aborted() override
  {
    return BT::NodeStatus::FAILURE;
  }

  BT::NodeStatus on_cancelled() override
  {
    return BT::NodeStatus::FAILURE;
  }

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("task_id"),
      BT::InputPort<std::string>("object_id"),
      BT::OutputPort<bool>("result")
    };
  }
};

} // namespace warehouser_bt
```

### 4. Complete nav2_params.yaml Template

```yaml
bt_navigator:
  ros__parameters:
    navigators: ['navigate_to_pose', 'navigate_through_poses']
    navigate_to_pose:
      plugin: "nav2_bt_navigator::NavigateToPoseNavigator"
    navigate_through_poses:
      plugin: "nav2_bt_navigator::NavigateThroughPosesNavigator"

    global_frame: map
    robot_base_frame: base_link
    odom_topic: odom
    bt_loop_duration: 10
    default_server_timeout: 20

    default_nav_to_pose_bt_xml: "$(find-pkg-share my_nav)/behavior_trees/navigate_to_pose.xml"
    default_nav_through_poses_bt_xml: "$(find-pkg-share my_nav)/behavior_trees/navigate_through_poses.xml"

planner_server:
  ros__parameters:
    expected_planner_frequency: 20.0
    plugins: ["GridBased"]
    GridBased:
      plugin: "nav2_smac_planner/SmacPlanner2D"
      tolerance: 0.125
      downsample_costmap: false
      allow_unknown: true
      max_iterations: 1000000
      max_planning_time: 5.0

controller_server:
  ros__parameters:
    controller_frequency: 20.0
    min_x_velocity_threshold: 0.001
    min_theta_velocity_threshold: 0.001

    progress_checker_plugins: ["progress_checker"]
    goal_checker_plugins: ["goal_checker"]
    controller_plugins: ["FollowPath"]

    progress_checker:
      plugin: "nav2_controller::SimpleProgressChecker"
      required_movement_radius: 0.5
      movement_time_allowance: 10.0

    goal_checker:
      plugin: "nav2_controller::SimpleGoalChecker"
      xy_goal_tolerance: 0.25
      yaw_goal_tolerance: 0.25

    FollowPath:
      plugin: "dwb_core::DWBLocalPlanner"
      max_vel_x: 0.26
      max_vel_theta: 1.0
      min_speed_xy: 0.0
      acc_lim_x: 2.5
      acc_lim_theta: 3.2

global_costmap:
  global_costmap:
    ros__parameters:
      update_frequency: 1.0
      publish_frequency: 1.0
      global_frame: map
      robot_base_frame: base_link
      resolution: 0.05
      robot_radius: 0.22
      plugins: ["static_layer", "obstacle_layer", "inflation_layer"]

      static_layer:
        plugin: "nav2_costmap_2d::StaticLayer"
        enabled: true
        map_subscribe_transient_local: true

      obstacle_layer:
        plugin: "nav2_costmap_2d::ObstacleLayer"
        enabled: true
        observation_sources: scan
        scan:
          topic: /scan
          data_type: "LaserScan"

      inflation_layer:
        plugin: "nav2_costmap_2d::InflationLayer"
        enabled: true
        inflation_radius: 0.55
        cost_scaling_factor: 3.0

local_costmap:
  local_costmap:
    ros__parameters:
      update_frequency: 5.0
      publish_frequency: 2.0
      global_frame: odom
      robot_base_frame: base_link
      rolling_window: true
      width: 5
      height: 5
      resolution: 0.05
      robot_radius: 0.22
      plugins: ["obstacle_layer", "inflation_layer"]

      obstacle_layer:
        plugin: "nav2_costmap_2d::ObstacleLayer"
        enabled: true
        observation_sources: scan
        scan:
          topic: /scan
          data_type: "LaserScan"

      inflation_layer:
        plugin: "nav2_costmap_2d::InflationLayer"
        enabled: true
        inflation_radius: 0.55
        cost_scaling_factor: 3.0
```

## Recommendation: Clone Reference Repositories

Since `.delegate/templates/` is empty, consider using the [S] study phase to clone these reference repositories for deeper analysis:

1. **navigation2** - Main Nav2 repository
   - URL: https://github.com/ros-navigation/navigation2
   - Focus: Plugin interfaces, example implementations

2. **navigation2_tutorials** - Official tutorials with reference code
   - URL: https://github.com/ros-planning/navigation2_tutorials
   - Focus: nav2_straightline_planner, custom plugin examples

3. **nav2_minimal_turtlebot_simulation** - Complete working example
   - URL: https://github.com/cyberbotics/nav2_minimal_turtlebot_simulation
   - Focus: Integration patterns, launch files

4. **free_fleet** - Multi-robot fleet management
   - URL: https://github.com/open-rmf/free_fleet
   - Focus: Multi-robot coordination, OpenRMF integration

These repositories contain production-ready code that can serve as templates for Warehouser integration.

## Sources

- [Nav2 BT Navigator README](https://github.com/ros-planning/navigation2/blob/main/nav2_bt_navigator/README.md)
- [Nav2 Behavior Tree README](https://github.com/ros-planning/navigation2/blob/main/nav2_behavior_tree/README.md)
- [Nav2 Behavior Trees Documentation](https://docs.nav2.org/behavior_trees/index.html)
- [Configuring BT Navigator](https://docs.nav2.org/configuration/packages/configuring-bt-navigator.html)
- [Writing a New Planner Plugin](https://docs.nav2.org/plugin_tutorials/docs/writing_new_nav2planner_plugin.html)
- [Writing a New Controller Plugin](https://docs.nav2.org/plugin_tutorials/docs/writing_new_nav2controller_plugin.html)
- [DWB Controller Documentation](https://docs.nav2.org/configuration/packages/configuring-dwb-controller.html)
- [Costmap 2D Configuration](https://docs.nav2.org/configuration/packages/configuring-costmaps.html)
- [Obstacle Layer Parameters](https://docs.nav2.org/configuration/packages/costmap-plugins/obstacle.html)
- [Inflation Layer Parameters](https://docs.nav2.org/configuration/packages/costmap-plugins/inflation.html)
- [Multi-robot Navigation with Nav2](https://medium.com/@arshad.mehmood/a-guide-to-multi-robot-navigation-utilizing-turtlebot3-and-nav2-cd24f96d19c6)
- [Free Fleet Adapter - Multi-Robot Programming](https://osrf.github.io/ros2multirobotbook/integration_free_fleet_adapter.html)
- [OpenRMF Deep Dive](https://ekumenlabs.com/blog/posts/deep-dive-into-openrmf/)
