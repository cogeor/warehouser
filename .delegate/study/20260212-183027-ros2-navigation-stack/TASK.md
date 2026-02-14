# TASK: Integrate Nav2 Architecture Patterns for Hybrid RL-Classical Navigation

Created: 2026-02-12 18:45:00
Build: NOT VERIFIED (workspace not built)
Tests: FAIL (missing dependencies)

## Summary

Integrate ROS2 Navigation2 (Nav2) architectural patterns into Warehouser to create a hybrid RL-classical navigation system. This involves implementing Nav2-compatible interfaces, transitioning from state machine to behavior tree coordination, and wrapping the existing RL policy as a Nav2 controller plugin while adding classical global planning capabilities.

## Context

### Current State: RL-First Navigation Architecture

Warehouser implements end-to-end learned navigation where a PPO policy directly outputs velocity commands from observations. Key characteristics:

- **Movement**: Differential drive with direct velocity control (no acceleration limits)
- **Observations**: V1 uses ground-truth position (8 dims), V2 planned with lidar (63 dims, not implemented)
- **Path Planning**: None - navigation is fully learned end-to-end
- **Task Coordination**: Simple finite state machine (IDLE → NAVIGATING_TO_PICK → PICKING → etc.)
- **Collision Handling**: Discrete rollback + reactive safety controller
- **World Model**: Continuous 2D space with axis-aligned rectangle walls
- **Localization**: Ground truth (unrealistic for real robots)
- **Recovery Behaviors**: None
- **Multi-Robot**: Native support via V3 observations

**Strengths:**
- Clean modular architecture with composable reward strategies
- Multi-robot foundation better than Nav2
- Sensor compatibility (LaserScan format)
- Safety controller provides runtime safety layer

**Weaknesses:**
- Perfect localization assumption breaks sim-to-real transfer
- No explicit path planning or map representation
- No recovery behaviors for stuck situations
- Discrete collision handling causes oscillation
- State machine less flexible than behavior trees

### Target Architecture: Hybrid RL + Nav2

Based on 2025 research and Nav2 best practices, integrate classical navigation components while preserving RL strengths:

**Architecture Flow:**
```
Sensors (Lidar) → Costmap2D (Static + Obstacle + Inflation Layers)
                     ↓
           Smac 2D A* Global Planner (warehouse-scale paths)
                     ↓
           BehaviorTree.CPP Navigator (task coordination)
                     ↓
           RL Controller Plugin (PPO policy for local control)
                     ↓
           Safety Layer (existing) → Robot
```

**Key Design Decisions:**
1. **Global Planning**: Use Nav2 Smac 2D A* for efficient warehouse-scale path planning
2. **Local Control**: Wrap existing RL policy as Nav2 controller plugin
3. **Task Coordination**: Replace FSM with BehaviorTree.CPP for flexible task composition
4. **Localization**: Add AMCL or integrate localization noise for sim-to-real readiness
5. **Recovery**: Add Nav2 recovery behaviors (spin, backup, wait) for stuck situations
6. **Costmaps**: Implement costmap representation from lidar + static map

**Benefits:**
- Proven Nav2 components for planning, localization, recovery
- RL policy focuses on optimal local control (its strength)
- Behavior trees enable complex warehouse task sequences
- Standard Nav2 interfaces for interoperability
- Maintains multi-robot advantages

## Sources

Findings consolidated from three study phases:

### [S] Search Findings - Nav2 Architecture Research
- Nav2 is production-grade framework used by 100+ companies
- Plugin architecture allows hybrid classical/RL approaches
- Behavior trees provide flexible coordination without FSM complexity
- Smac 2D A* recommended for differential drive warehouse robots
- 2025 research shows RL local planners outperform DWB in complex scenarios
- Active research on meta-planners that switch between classical and RL

**Key Nav2 Components:**
1. Planner Server - Global path planning (NavFn, Smac, Theta*)
2. Controller Server - Local trajectory control (DWB, TEB, MPPI)
3. Behavior Server - Recovery behaviors (spin, backup, wait)
4. BT Navigator - Behavior tree task coordination
5. AMCL - Particle filter localization
6. Costmap 2D - Layered obstacle representation

### [I] Introspection Findings - Current Warehouser Analysis
- Well-architected RL-first system with clean separation of concerns
- Lidar simulator outputs Nav2-compatible sensor_msgs/LaserScan
- Ground truth localization in V1 observations (sim-to-real gap)
- No costmap, no occupancy grid, continuous 2D space only
- Task state machine is supervisory only (no path computation)
- Safety controller is reactive (no predictive collision checking)
- Multi-robot infrastructure exists but not fully integrated

**Integration Opportunities Identified:**
- Easy: Publish LaserScan on /scan, add TF tree, generate static map
- Medium: Wrap policy as Nav2 controller, add recovery behaviors, costmap generation
- Large: Behavior tree integration, SLAM, hybrid planner/controller

### [T] Template Findings - Nav2 Integration Patterns
- Complete plugin interface definitions for planners and controllers
- BehaviorTree.CPP patterns for warehouse pick-and-place tasks
- Costmap configuration with layered architecture
- Custom BT node implementation templates
- Multi-robot namespace patterns
- Hybrid classical/RL meta-controller patterns

**Plugin Interfaces:**
- nav2_core::GlobalPlanner - createPlan() returns nav_msgs::Path
- nav2_core::Controller - computeVelocityCommands() returns TwistStamped
- nav2_costmap_2d::Layer - updateCosts() for custom costmap layers
- BtActionNode<ActionT> - for custom behavior tree nodes

## Objective

Create a hybrid navigation architecture that:
1. Implements Nav2-compatible interfaces for interoperability
2. Uses classical global planning (Smac 2D A*) for warehouse-scale paths
3. Wraps existing RL PPO policy as Nav2 controller plugin for local control
4. Replaces state machine with behavior tree coordination
5. Adds AMCL localization or noise injection for sim-to-real readiness
6. Implements recovery behaviors for stuck situations
7. Generates costmaps from lidar data and static map
8. Maintains multi-robot capabilities

## Scope

### New Packages to Create

| Package | Purpose | Key Interfaces |
|---------|---------|----------------|
| `warehouser_navigation` | Nav2 plugin implementations | RLController, RLCostmapLayer, navigation services |
| `warehouser_bt` | Custom behavior tree nodes | PickObject, DropObject, WarehouseTask actions |
| `warehouser_localization` | Localization integration | AMCL config or noise injection |

### Files to Create

#### Phase 1: Nav2 Interface Compatibility

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_navigation/include/warehouser_navigation/rl_controller.hpp` | Nav2 controller plugin wrapping RL policy |
| `ros_ws/src/warehouser_navigation/src/rl_controller.cpp` | Controller implementation using ONNX inference |
| `ros_ws/src/warehouser_navigation/include/warehouser_navigation/navigation_interface.hpp` | Nav2-compatible service/action interfaces |
| `ros_ws/src/warehouser_navigation/src/navigation_interface.cpp` | Bridge between Nav2 and existing RL bridge |
| `ros_ws/src/warehouser_navigation/include/warehouser_navigation/costmap_builder.hpp` | Convert world to costmap representation |
| `ros_ws/src/warehouser_navigation/src/costmap_builder.cpp` | Costmap generation from walls + lidar |
| `ros_ws/src/warehouser_navigation/plugins/plugin_description.xml` | Plugin exports for Nav2 |
| `ros_ws/src/warehouser_navigation/config/nav2_params.yaml` | Nav2 configuration for Warehouser |
| `ros_ws/src/warehouser_navigation/CMakeLists.txt` | Build configuration with pluginlib exports |
| `ros_ws/src/warehouser_navigation/package.xml` | Package dependencies (nav2_core, nav2_costmap_2d) |

#### Phase 2: Behavior Tree Integration

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_bt/include/warehouser_bt/pick_object_action.hpp` | BT node for picking objects |
| `ros_ws/src/warehouser_bt/src/pick_object_action.cpp` | Pick action implementation |
| `ros_ws/src/warehouser_bt/include/warehouser_bt/drop_object_action.hpp` | BT node for dropping objects |
| `ros_ws/src/warehouser_bt/src/drop_object_action.cpp` | Drop action implementation |
| `ros_ws/src/warehouser_bt/behavior_trees/pick_and_place.xml` | BT definition for warehouse task |
| `ros_ws/src/warehouser_bt/behavior_trees/navigate_to_pose.xml` | BT definition for navigation |
| `ros_ws/src/warehouser_bt/CMakeLists.txt` | Build configuration |
| `ros_ws/src/warehouser_bt/package.xml` | Package dependencies (nav2_behavior_tree) |

#### Phase 3: Localization & Recovery

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_localization/config/amcl_params.yaml` | AMCL configuration |
| `ros_ws/src/warehouser_localization/launch/localization.launch.py` | Localization launch file |
| `ros_ws/src/warehouser_navigation/include/warehouser_navigation/recovery_behaviors.hpp` | Spin/backup recovery behaviors |
| `ros_ws/src/warehouser_navigation/src/recovery_behaviors.cpp` | Recovery implementation |

#### Phase 4: Launch & Configuration

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_bringup/launch/nav2_navigation.launch.py` | Launch Nav2 stack with Warehouser |
| `ros_ws/src/warehouser_bringup/launch/hybrid_navigation.launch.py` | Launch hybrid RL + classical navigation |
| `ros_ws/src/warehouser_bringup/config/map.yaml` | Static map metadata |
| `ros_ws/src/warehouser_bringup/maps/warehouse.pgm` | Generated occupancy grid map |

### Files to Modify

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_observations/src/observations_node.cpp` | Publish LaserScan on /scan topic, add TF broadcaster |
| `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp` | Add map generation method |
| `ros_ws/src/warehouser_simulation/src/world_manager.cpp` | Implement occupancy grid generation from walls |
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Add localization noise option for V1 observations |
| `ros_ws/src/warehouser_task/src/task_state_machine.cpp` | Add BT integration option (parallel implementation) |
| `ros_ws/src/warehouser_inference/src/inference_node.cpp` | Expose as service for Nav2 controller plugin |

## Implementation Plan

### Phase 1: Nav2 Interface Compatibility (Week 1)

**Objective:** Make Warehouser navigation Nav2-compatible without full integration.

#### Tasks:
- [ ] Create `warehouser_navigation` package with Nav2 dependencies
- [ ] Implement `RLController` class deriving from `nav2_core::Controller`
- [ ] Integrate ONNX inference into controller plugin
- [ ] Create `NavigationInterface` node with Nav2-compatible services
- [ ] Implement costmap generation from walls (static layer)
- [ ] Publish LaserScan on `/scan` topic
- [ ] Add TF tree broadcaster (map → odom → base_link)
- [ ] Generate static map in PGM format from world config
- [ ] Create `nav2_params.yaml` configuration file
- [ ] Write plugin_description.xml and export via CMakeLists

**Acceptance Criteria:**
- RLController compiles and loads as Nav2 plugin
- LaserScan published on /scan topic visible in RViz
- TF tree published correctly
- Static map loads in nav2_map_server
- Nav2 planner can compute paths on generated costmap

### Phase 2: Behavior Tree Integration (Week 2)

**Objective:** Replace FSM with behavior tree coordination for flexible task composition.

#### Tasks:
- [ ] Create `warehouser_bt` package with BehaviorTree.CPP dependencies
- [ ] Implement `PickObjectAction` BT node using `BtActionNode<PickObject>`
- [ ] Implement `DropObjectAction` BT node
- [ ] Create pick_and_place.xml behavior tree definition
- [ ] Create navigate_to_pose.xml behavior tree (Nav2 pattern)
- [ ] Register custom BT nodes with factory
- [ ] Configure BT Navigator to load custom plugins
- [ ] Test behavior tree execution with RViz BT visualization

**Acceptance Criteria:**
- Custom BT nodes compile and register successfully
- Behavior trees load without errors
- BT Navigator can execute pick-and-place sequence
- Groot can visualize behavior tree structure

### Phase 3: Hybrid Planner Implementation (Week 3)

**Objective:** Integrate Nav2 global planner with RL local controller.

#### Tasks:
- [ ] Configure Smac 2D A* planner for warehouse environment
- [ ] Tune planner parameters (tolerance, max_iterations, etc.)
- [ ] Connect RLController to receive paths from planner
- [ ] Implement path following in RL controller
- [ ] Test hybrid navigation: Smac planning + RL control
- [ ] Benchmark against RL-only navigation
- [ ] Add obstacle layer to costmap from lidar scans
- [ ] Add inflation layer for safety margins

**Acceptance Criteria:**
- Smac planner generates kinematically feasible paths
- RL controller successfully follows planned paths
- Hybrid approach shows improved success rate vs RL-only
- Costmap updates with lidar observations

### Phase 4: Localization Integration (Week 4)

**Objective:** Remove ground-truth dependency, add realistic localization.

#### Tasks:
- [ ] Option A: Integrate AMCL for particle filter localization
- [ ] Option B: Add configurable noise to ground-truth observations
- [ ] Modify observation_builder to use AMCL pose or noisy pose
- [ ] Retrain RL policy with noisy observations
- [ ] Configure AMCL parameters (particle count, noise models)
- [ ] Test localization accuracy with lidar data
- [ ] Validate policy performance with localization uncertainty

**Acceptance Criteria:**
- Robot no longer depends on ground-truth position
- AMCL converges to correct pose within 0.5m
- RL policy performs adequately with localization noise
- Sim-to-real gap reduced

### Phase 5: Recovery Behaviors (Week 5)

**Objective:** Add stuck detection and recovery actions.

#### Tasks:
- [ ] Implement stuck detection (low velocity despite high command)
- [ ] Create spin recovery behavior (rotate to find free space)
- [ ] Create backup recovery behavior (reverse from obstacle)
- [ ] Create wait recovery behavior (pause for dynamic obstacles)
- [ ] Integrate recovery behaviors into BT (Recovery decorator)
- [ ] Configure recovery behavior parameters
- [ ] Test recovery in corner/narrow passage scenarios

**Acceptance Criteria:**
- Stuck situations detected within 5 seconds
- Recovery behaviors successfully unstick robot
- BT Navigator triggers recoveries appropriately
- No infinite recovery loops

### Phase 6: Multi-Robot Coordination (Week 6)

**Objective:** Extend Nav2 integration to multi-robot scenarios.

#### Tasks:
- [ ] Create multi-robot launch file with namespaces
- [ ] Configure per-robot Nav2 parameter files
- [ ] Add shared costmap layer for other robots
- [ ] Test multi-robot navigation without collisions
- [ ] Implement fleet coordination patterns (if needed)
- [ ] Benchmark multi-robot throughput

**Acceptance Criteria:**
- Multiple robots navigate independently
- Robots avoid each other via costmap
- No inter-robot collisions
- Task allocation works correctly

## Interface Definitions

### 1. RLController Plugin Interface

```cpp
namespace warehouser_navigation
{

class RLController : public nav2_core::Controller
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
  void setPlan(const nav_msgs::msg::Path& path) override;

  // Core control method
  geometry_msgs::msg::TwistStamped computeVelocityCommands(
    const geometry_msgs::msg::PoseStamped& pose,
    const geometry_msgs::msg::Twist& velocity,
    nav2_core::GoalChecker* goal_checker) override;

  // Optional methods
  void setSpeedLimit(const double& speed_limit, const bool& percentage) override;

private:
  // ONNX model inference
  std::unique_ptr<PolicyInference> rl_model_;

  // Observation building
  std::vector<float> buildObservations(
    const geometry_msgs::msg::PoseStamped& pose,
    const geometry_msgs::msg::Twist& velocity,
    const nav_msgs::msg::Path& path,
    nav2_costmap_2d::Costmap2D* costmap);

  // Configuration
  std::string model_path_;
  double observation_range_;
  double max_linear_velocity_;
  double max_angular_velocity_;

  // State
  nav_msgs::msg::Path current_path_;
  rclcpp_lifecycle::LifecycleNode::WeakPtr node_;
  std::shared_ptr<tf2_ros::Buffer> tf_;
  nav2_costmap_2d::Costmap2D* costmap_;
  rclcpp::Logger logger_{rclcpp::get_logger("RLController")};
};

} // namespace warehouser_navigation
```

### 2. NavigationInterface Service Definitions

```cpp
namespace warehouser_navigation
{

class NavigationInterface : public rclcpp::Node
{
public:
  NavigationInterface();

private:
  // Nav2-compatible service handlers
  void handleComputePathToPose(
    const std::shared_ptr<nav2_msgs::srv::ComputePathToPose::Request> request,
    std::shared_ptr<nav2_msgs::srv::ComputePathToPose::Response> response);

  void handleFollowPath(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<nav2_msgs::action::FollowPath>> goal_handle);

  // Convert to existing RL bridge format
  warehouser_msgs::srv::RLStep::Request convertToRLStep(
    const geometry_msgs::msg::PoseStamped& pose,
    const nav_msgs::msg::Path& path);

  // Services and actions
  rclcpp::Service<nav2_msgs::srv::ComputePathToPose>::SharedPtr plan_service_;
  rclcpp_action::Server<nav2_msgs::action::FollowPath>::SharedPtr follow_action_;

  // RL bridge client
  rclcpp::Client<warehouser_msgs::srv::RLStep>::SharedPtr rl_step_client_;
};

} // namespace warehouser_navigation
```

### 3. Custom Behavior Tree Nodes

```cpp
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
    const BT::NodeConfiguration& conf);

  void on_tick() override;
  BT::NodeStatus on_success() override;
  BT::NodeStatus on_aborted() override;
  BT::NodeStatus on_cancelled() override;

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("object_id", "ID of object to pick"),
      BT::OutputPort<bool>("success", "Whether pick succeeded")
    };
  }
};

class DropObjectAction : public nav2_behavior_tree::BtActionNode<
  warehouser_msgs::action::DropObject>
{
public:
  using ActionT = warehouser_msgs::action::DropObject;

  DropObjectAction(
    const std::string& xml_tag_name,
    const std::string& action_name,
    const BT::NodeConfiguration& conf);

  void on_tick() override;
  BT::NodeStatus on_success() override;
  BT::NodeStatus on_aborted() override;
  BT::NodeStatus on_cancelled() override;

  static BT::PortsList providedPorts()
  {
    return {
      BT::OutputPort<bool>("success", "Whether drop succeeded")
    };
  }
};

} // namespace warehouser_bt
```

### 4. Costmap Builder Interface

```cpp
namespace warehouser_navigation
{

class CostmapBuilder
{
public:
  // Generate occupancy grid from world walls
  nav_msgs::msg::OccupancyGrid generateStaticMap(
    const std::vector<Wall>& walls,
    float resolution,
    float width,
    float height);

  // Convert to Nav2 Costmap2D format
  void updateCostmapFromWorld(
    nav2_costmap_2d::Costmap2D& costmap,
    const std::vector<Wall>& walls);

  // Convert lidar scan to obstacle layer
  void updateCostmapFromLidar(
    nav2_costmap_2d::Costmap2D& costmap,
    const sensor_msgs::msg::LaserScan& scan,
    const geometry_msgs::msg::PoseStamped& robot_pose);

private:
  float resolution_;
  uint8_t occupied_threshold_{100};
  uint8_t free_threshold_{0};
};

} // namespace warehouser_navigation
```

### 5. Behavior Tree XML Structure

```xml
<root main_tree_to_execute="PickAndPlace">
  <BehaviorTree ID="PickAndPlace">
    <RecoveryNode number_of_retries="3" name="NavigateRecovery">
      <PipelineSequence name="NavigateWithReplanning">
        <Sequence>
          <!-- Navigate to pickup location -->
          <DistanceController distance="1.0">
            <ComputePathToPose goal="{pickup_pose}" path="{path}"/>
          </DistanceController>
          <FollowPath path="{path}" controller_id="RLController"/>

          <!-- Pick object using warehouse action -->
          <PickObject object_id="{target_object}" success="{picked}"/>

          <!-- Navigate to dropoff location -->
          <DistanceController distance="1.0">
            <ComputePathToPose goal="{dropoff_pose}" path="{path}"/>
          </DistanceController>
          <FollowPath path="{path}" controller_id="RLController"/>

          <!-- Drop object -->
          <DropObject success="{dropped}"/>
        </Sequence>
      </PipelineSequence>

      <!-- Recovery behaviors if navigation fails -->
      <SequenceStar name="RecoveryActions">
        <ClearCostmap name="ClearGlobalCostmap" service_name="global_costmap/clear_costmap"/>
        <ClearCostmap name="ClearLocalCostmap" service_name="local_costmap/clear_costmap"/>
        <Spin spin_dist="1.57"/>
        <Wait wait_duration="5"/>
        <BackUp backup_dist="0.3" backup_speed="0.1"/>
      </SequenceStar>
    </RecoveryNode>
  </BehaviorTree>
</root>
```

## Architecture Notes

### Hybrid RL-Classical Design Rationale

**Why Global Classical + Local RL:**
1. Classical planners (Smac A*) are provably complete and efficient for warehouse-scale planning
2. RL policies excel at reactive obstacle avoidance and learning optimal local behaviors
3. Hybrid approach provides structure (via global plan) while keeping learned flexibility
4. Easier to debug: global plan is interpretable, RL policy focused on local control

**Why Behavior Trees over FSM:**
1. Composability: reuse subtrees across different warehouse tasks
2. Hierarchy: natural representation of task decomposition
3. XML configuration: change task logic without recompilation
4. Groot visualization: real-time debugging of task execution
5. Recovery patterns: built-in retry and fallback mechanisms

**Why AMCL or Noise Injection:**
1. Ground-truth localization creates sim-to-real gap
2. Real robots use particle filters (AMCL) or EKF-based localization
3. Training with noise improves policy robustness
4. AMCL provides uncertainty estimates for decision-making

**Why Keep Multi-Robot Foundation:**
1. Warehouser's native multi-robot support is better than Nav2's namespace approach
2. Decentralized observations (V3) enable multi-agent RL
3. Shared costmap pattern from Nav2 can augment existing approach
4. Fleet coordination is critical for warehouse throughput

### Configuration Strategy

**Parameter Files:**
- `nav2_params.yaml` - Nav2 stack configuration (planner, controller, costmap)
- `amcl_params.yaml` - Localization parameters
- `rl_controller_params.yaml` - RL controller specific parameters
- `world.yaml` - World definition (existing, extended with map metadata)

**Runtime Parameter Updates:**
- RL controller model path (swap between trained policies)
- Planner/controller selection (classical vs RL vs hybrid)
- Recovery behavior thresholds
- Costmap layer enabling/disabling

### Sim-to-Real Considerations

**Training Pipeline:**
1. Train PPO policy in simulation with localization noise
2. Export to ONNX format
3. Load in RLController plugin
4. Test with Nav2 global planner in simulation
5. Domain randomization: sensor noise, dynamics variation
6. Deploy to real robots via same ROS2 interfaces

**Key Sim-to-Real Improvements:**
- Replace ground-truth localization with AMCL
- Add sensor noise models (already implemented)
- Train with localization uncertainty
- Use Nav2 recovery behaviors for edge cases
- Costmap-based obstacle representation (more realistic)

## Verification

### Phase 1: Interface Compatibility
- [ ] `ros2 run warehouser_navigation rl_controller_node` starts without errors
- [ ] `ros2 topic echo /scan` shows lidar data
- [ ] `ros2 run tf2_ros tf2_echo map base_link` shows transform
- [ ] RViz displays static map loaded from map_server
- [ ] Nav2 planner computes path on costmap

### Phase 2: Behavior Trees
- [ ] `ros2 launch warehouser_bringup bt_navigation.launch.py` launches BT Navigator
- [ ] BT Navigator loads custom plugin library
- [ ] Groot connects and visualizes behavior tree
- [ ] Pick-and-place sequence executes successfully
- [ ] BT handles action failures gracefully

### Phase 3: Hybrid Navigation
- [ ] Robot follows Smac-planned path using RL controller
- [ ] Success rate ≥ RL-only baseline
- [ ] Path efficiency improved (shorter, smoother)
- [ ] Planning time < 200ms for warehouse-scale distances
- [ ] Costmap updates with obstacles from lidar

### Phase 4: Localization
- [ ] AMCL converges within 10 seconds
- [ ] Localization error < 0.5m RMS
- [ ] Policy succeeds with noisy observations
- [ ] Retraining converges with localization noise

### Phase 5: Recovery
- [ ] Stuck detection triggers within 5 seconds
- [ ] Recovery behaviors unstick robot ≥ 90% of time
- [ ] No infinite recovery loops
- [ ] BT Navigator retries after recovery

### Phase 6: Multi-Robot
- [ ] 3+ robots navigate without collisions
- [ ] Shared costmap shows other robots
- [ ] Task allocation distributes work
- [ ] Throughput scales with robot count

### Integration Testing
- [ ] Full warehouse scenario: spawn → navigate → pick → navigate → place
- [ ] 100 episode test: success rate ≥ 95%
- [ ] Multi-robot test: 5 robots, 10 tasks, no collisions
- [ ] Recovery test: robot escapes all test corner/narrow scenarios
- [ ] Performance test: task completion time comparable to RL-only

### Documentation
- [ ] README.md updated with Nav2 integration instructions
- [ ] Configuration guide for nav2_params.yaml
- [ ] Behavior tree authoring guide
- [ ] Troubleshooting guide for common issues
- [ ] Architecture diagram showing hybrid RL-Nav2 flow

## Risk Mitigation

**Risk: RL policy trained on ground-truth doesn't work with AMCL**
- Mitigation: Retrain with noisy observations, gradually increase noise

**Risk: Nav2 dependencies conflict with existing ROS2 packages**
- Mitigation: Test in isolated workspace, use compatible ROS2 version (Humble/Iron)

**Risk: Behavior tree complexity harder to debug than FSM**
- Mitigation: Use Groot visualization, start with simple trees, add logging

**Risk: Hybrid approach slower than RL-only**
- Mitigation: Benchmark planning time, use local costmap for RL controller, async planning

**Risk: Multi-robot coordination breaks with Nav2 namespace pattern**
- Mitigation: Keep existing multi-robot infrastructure, add Nav2 per-robot, test incrementally

## Success Criteria

This task is complete when:
1. RL controller successfully wraps PPO policy as Nav2 plugin
2. Smac 2D A* planner generates warehouse paths
3. Behavior tree coordinates pick-and-place tasks
4. Localization no longer uses ground truth
5. Recovery behaviors handle stuck situations
6. Multi-robot navigation works with Nav2 integration
7. All verification tests pass
8. Documentation complete
9. Performance ≥ baseline RL-only approach

**Estimated Effort:** 6 weeks (1 week per phase)
**Priority:** High (foundational for real-world deployment)
**Dependencies:** ONNX inference working, ROS2 workspace building, trained PPO model available
