# TASK: Complete Multi-Robot Coordination Infrastructure

Created: 2026-02-12T18:30:00Z
Build: Python tests passing (84/103 passed)
Tests: ROS2 build not tested on Windows platform

## Summary

Warehouser has a strong foundation for multi-robot reinforcement learning with PettingZoo integration, per-robot observations (V3_MultiRobot), and service interfaces supporting robot_id parameters. However, critical bugs prevent actual multi-robot operation: actions for robot_id > 0 are silently ignored, robots can overlap without collision detection, and the simulation doesn't accept robot_count during reset. This task addresses these P0 bugs first, then builds out traffic management, path planning, and advanced MARL coordination features to enable scalable warehouse fleet coordination.

## Current State

### Strong Foundation
- **PettingZoo ParallelEnv**: `training/training/envs/pettingzoo_env.py` implements multi-agent RL API
- **V3_MultiRobot Observations**: `ros_ws/src/warehouser_observations/` provides ego-centric observations with other robot positions
- **Multi-Robot Services**: `warehouser_msgs/srv/RLStep.srv` and `RLReset.srv` have robot_id/robot_count fields
- **Per-Robot Rewards**: `warehouser_rl_bridge` maintains vector of RewardCalculator instances
- **WorldManager Support**: `warehouser_simulation/world_manager.hpp` has `addRobot()`, `robot(index)`, `robotCount()` methods

### Critical Bugs (Phase 0 - BLOCKING)

**Bug 1: Action Routing to Robot 0 Only**
- Location: `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:194-216`
- Problem: Actions for `robot_id > 0` are silently discarded with a warning
- Impact: Only robot 0 can be controlled; multi-robot training doesn't actually train multiple robots
- Evidence:
  ```cpp
  // TODO: For multi-robot, need per-robot cmd_vel topics or action message
  if (robot_id == 0) {
      cmd_pub_->publish(cmd);
  } else {
      RCLCPP_WARN_ONCE(get_logger(),
          "Multi-robot actions not yet routed to per-robot topics");
  }
  ```

**Bug 2: No Robot-Robot Collision Detection**
- Location: `ros_ws/src/warehouser_simulation/src/world_manager.cpp:206-213`
- Problem: `checkCollision()` only checks walls, not other robots
- Impact: Robots can occupy the same position without penalty, breaking physics realism
- Evidence:
  ```cpp
  bool WorldManager::checkCollision(float px, float py) const {
      for (const auto& wall : walls_) {
          if (wall->contains(px, py)) {
              return true;
          }
      }
      return false;  // No robot-robot collision check
  }
  ```

**Bug 3: Simulation Doesn't Accept robot_count**
- Location: `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:242-244`
- Problem: Reset service receives `robot_count` but doesn't pass it to simulation
- Impact: Simulation may not spawn correct number of robots
- Evidence:
  ```cpp
  // TODO: Pass robot_count to simulation reset if multi-robot sim is supported
  (void)robot_count;  // Suppress unused warning
  ```

### Coordination Gaps (Phase 1+)

**Missing Features:**
1. No path planning layer (no MAPF/CBS integration)
2. No inter-robot communication channels
3. No task allocation (single shared goal for all robots)
4. No traffic management (zone control, deadlock prevention)
5. No fleet protocols (VDA5050, Open-RMF)
6. Sequential stepping instead of truly parallel actions (`pettingzoo_env.py:239-299`)

## Critical Bug Fixes (Phase 0)

These must be fixed before any multi-robot training will work properly.

### Fix 1: Implement Per-Robot Action Routing

**File**: `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp`

**Changes Required**:
1. Replace single publishers with vectors indexed by robot_id:
   ```cpp
   // In rl_bridge_node.hpp
   std::vector<rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr> cmd_vel_pubs_;
   std::vector<rclcpp::Client<warehouser_msgs::srv::Pick>::SharedPtr> pick_clients_;
   std::vector<rclcpp::Client<warehouser_msgs::srv::Unpick>::SharedPtr> unpick_clients_;
   ```

2. Initialize publishers in constructor based on robot_count (or dynamically on first use):
   ```cpp
   // Create publishers for each robot namespace
   for (size_t i = 0; i < robot_count_; ++i) {
       std::string robot_ns = "/robot" + std::to_string(i);
       cmd_vel_pubs_.push_back(
           create_publisher<geometry_msgs::msg::Twist>(
               robot_ns + "/cmd_vel", 10
           )
       );
       // Similar for pick/unpick clients
   }
   ```

3. Route actions in `executeAction()`:
   ```cpp
   auto RLBridgeNode::executeAction(/* ... */) -> void {
       if (robot_id >= cmd_vel_pubs_.size()) {
           RCLCPP_ERROR(get_logger(), "robot_id %u exceeds configured robot count", robot_id);
           return;
       }
       cmd_vel_pubs_[robot_id]->publish(cmd);
       // Similar for pick/unpick
   }
   ```

### Fix 2: Add Robot-Robot Collision Detection

**File**: `ros_ws/src/warehouser_simulation/src/world_manager.cpp`

**Changes Required**:
1. Add `checkRobotCollision(size_t robot_index)` method:
   ```cpp
   auto WorldManager::checkRobotCollision(size_t robot_index) const -> bool {
       const auto& robot_a = robots_[robot_index];
       float ax = robot_a->x();
       float ay = robot_a->y();
       float radius = 0.3f;  // Robot::kRadius

       for (size_t i = 0; i < robots_.size(); ++i) {
           if (i == robot_index) continue;

           const auto& robot_b = robots_[i];
           float bx = robot_b->x();
           float by = robot_b->y();

           float dist_sq = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
           float collision_dist = 2.0f * radius;

           if (dist_sq < collision_dist * collision_dist) {
               return true;
           }
       }
       return false;
   }
   ```

2. Check robot-robot collisions in `step()`:
   ```cpp
   auto WorldManager::step(float dt) -> void {
       // Store previous positions
       std::vector<std::pair<float, float>> prev_positions;
       for (const auto& robot : robots_) {
           prev_positions.emplace_back(robot->x(), robot->y());
       }

       // Update all robots
       for (auto& robot : robots_) {
           robot->step(dt);
       }

       // Check collisions and rollback if needed
       for (size_t i = 0; i < robots_.size(); ++i) {
           if (checkRobotCollision(i) || checkCollision(robots_[i]->x(), robots_[i]->y())) {
               robots_[i]->setPosition(prev_positions[i].first, prev_positions[i].second);
               // Set collision flag in entity for reward calculation
           }
       }
   }
   ```

3. Add collision flag to Entity message (or use existing state fields)

### Fix 3: Add Robot-Robot Collision Penalty

**File**: `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/reward_strategy.hpp`

**Changes Required**:
1. Extend `RewardContext` with `robot_robot_collision` boolean
2. Add `robot_collision_penalty` to `RewardConfig` (e.g., -50.0)
3. Update reward strategies to check collision flag
4. Apply penalty in `BaseRewardStrategy::calculate()`

### Fix 4: Pass robot_count to Simulation Reset

**File**: `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp`

**Changes Required**:
1. Update simulation reset service to accept robot_count parameter
2. WorldManager should spawn robots based on robot_count
3. Initialize per-robot publishers based on actual robot count

## Target Architecture

Based on research (S.md) and current state (I.md), the target multi-robot coordination architecture follows a layered approach:

```
┌─────────────────────────────────────────────────────────────┐
│                Fleet Manager (Phase 4 - Optional)            │
│            VDA5050/Open-RMF compatibility layer              │
│              (Task allocation, high-level planning)          │
└─────────────────────────────┬───────────────────────────────┘
                              │
      ┌───────────────────────┴───────────────────────┐
      │                                                │
┌─────▼──────────┐                          ┌─────────▼────────┐
│ Traffic Manager│                          │  MARL Policy     │
│ Zone Control   │◄────────────────────────►│  (MAPPO/IPPO)    │
│ Deadlock Prev  │                          │  Global Critic   │
└─────┬──────────┘                          └─────────┬────────┘
      │                                                │
┌─────▼────────────────────────────────────────────────▼───────┐
│         Multi-Agent Path Planning (PM-CBS)                   │
│       (Local planning with traffic-aware replanning)         │
└─────┬────────────────────────────────────────────────────────┘
      │
┌─────▼────────────────────────────────────────────────────────┐
│              ROS2 Communication Layer                         │
│   (Fast DDS, Discovery Server, Per-Robot Namespaces)         │
└─────┬────────────────────────────────────────────────────────┘
      │
┌─────▼────────────────────────────────────────────────────────┐
│           Individual Robot Controllers                       │
│         (/robot0, /robot1, ... /robotN)                      │
│   (Local navigation, collision avoidance, actuators)         │
└──────────────────────────────────────────────────────────────┘
```

## Implementation Plan

### Phase 0: Critical Bug Fixes (IMMEDIATE - REQUIRED FOR FUNCTIONALITY)

- [ ] Fix action routing to per-robot namespaced topics
  - Modify `rl_bridge_node.cpp` to use vector of publishers
  - Route actions based on robot_id
  - Test with 2-3 robots in simulation

- [ ] Implement robot-robot collision detection
  - Add `checkRobotCollision()` to WorldManager
  - Store previous positions, rollback on collision
  - Set collision flag in entity state

- [ ] Add robot-robot collision penalty to rewards
  - Extend RewardContext with `robot_robot_collision`
  - Add penalty to RewardConfig
  - Apply penalty in reward calculation

- [ ] Pass robot_count to simulation reset
  - Update reset service interface
  - WorldManager spawns correct number of robots
  - Initialize publishers based on robot_count

**Verification**: Train 2-3 robots, verify all receive commands and avoid each other

### Phase 1: Traffic Management Foundation (P1)

- [ ] Create `warehouser_traffic` package
  - CMake package with ROS2 dependencies
  - ZoneManager class with zone definition, reservation, occupancy tracking
  - Deadlock detection using wait-for graph

- [ ] Implement zone-based control
  - Define zones in warehouse config (aisles, intersections, staging areas)
  - Zone capacity constraints
  - Mark intersections as "nonstop areas"
  - Zone reservation service

- [ ] Add zone occupancy to observations
  - Extend V3_MultiRobot with zone information
  - Track which zone each robot occupies
  - Add zone occupancy counts to global state

- [ ] Integrate with reward system
  - Penalty for entering full zones
  - Penalty for stopping in nonstop areas
  - Reward for efficient zone usage

**Verification**: Simulate 5-10 robots, verify no deadlocks in narrow passages

### Phase 2: Path Planning (P2)

- [ ] Create `warehouser_planning` package
  - Research existing CBS/MAPF libraries
  - Implement PM-CBS or adapt existing implementation
  - C++23 with std::expected error handling

- [ ] Implement Multi-Agent Path Finding
  - CBS high-level constraint tree search
  - A* low-level single-agent planning
  - Conflict detection (vertex and edge conflicts)
  - Integrate with warehouse grid from simulation

- [ ] Create PlanMultiAgentPath service
  - Accept starts/goals for all robots
  - Return collision-free paths
  - Replanning on dynamic obstacles

- [ ] Integrate with traffic manager
  - Paths respect zone reservations
  - Request zone reservations for planned paths
  - Replan if reservation fails

**Verification**: Generate optimal paths for 10 robots, validate collision-free execution

### Phase 3: Advanced MARL Coordination (P2)

- [ ] Implement MAPPO algorithm
  - Create `training/training/algorithms/mappo.py`
  - Actor: decentralized, uses local observations
  - Critic: centralized, uses global state
  - GAE for advantage estimation
  - PPO clipped objective

- [ ] Construct global state for centralized critic
  - All robot positions, velocities, carrying states
  - All task locations and statuses
  - Zone occupancy counts
  - Static warehouse map (cached)

- [ ] Add parameter sharing option
  - Single actor shared across all agents
  - Agent ID as one-hot input
  - Compare with independent actors

- [ ] Curriculum learning
  - Start with 2 robots
  - Gradually increase to 5, 10, 20+ robots
  - Transfer policies across team sizes

- [ ] Baseline comparison
  - Train IPPO (independent PPO) first
  - Train MAPPO with same hyperparameters
  - Compare: fleet throughput, collisions, deadlocks, task completion time

**Verification**: MAPPO outperforms IPPO on fleet-level metrics

### Phase 4: Communication & Namespacing (P1)

- [ ] Configure ROS2 Discovery Server
  - Create `fastdds_discovery_server.xml` config
  - Set up server/client discovery protocol
  - Reduces network overhead for >10 robots

- [ ] Create multi-robot launch file
  - `warehouser_bringup/launch/multi_robot.launch.py`
  - Use GroupAction + PushRosNamespace per robot
  - Launch robot-specific nodes under `/robot{id}/`
  - Global nodes (world_manager, traffic_manager) in root namespace

- [ ] Update existing nodes for namespace awareness
  - Topics use relative paths
  - Services include robot_id parameter
  - World manager subscribes to all robot namespaces

- [ ] Test multi-robot communication
  - Verify topic isolation between robots
  - Measure network overhead with 10+ robots
  - Validate discovery performance

**Verification**: 10 robots communicate efficiently without cross-talk

### Phase 5: Extended Observations & Cooperation (P3)

- [ ] Extend V3 observations with velocity and state
  - Per other robot: `[rel_x, rel_y, rel_theta, v, omega, is_carrying]` (6 dims)
  - Update max_other_robots config
  - Enables prediction of other robots' intentions

- [ ] Implement weighted team rewards
  - Add `team_reward_weight` to MultiAgentConfig
  - Combine: `(1-w) * individual + w * team_avg`
  - Coordination bonuses for formation/cooperation

- [ ] Add per-robot goal assignment
  - Change `current_goal_` to vector of goals
  - Modify `setRandomGoal()` to assign different goals
  - Enable parallel task execution

- [ ] Add communication actions (optional)
  - Extend action space: `[linear, angular, pick, place, message]`
  - Broadcast messages in WorldState
  - Add `other_robot_messages` to observations
  - Train policies to use learned communication

**Verification**: Robots coordinate on multi-goal tasks, demonstrate emergent cooperation

### Phase 6: Fleet-Level Coordination (P4 - Production/Optional)

- [ ] Integrate VDA5050 protocol (optional)
  - Create `warehouser_vda5050` package
  - MQTT client for VDA5050 messages
  - Translate VDA5050 orders → ROS2 tasks
  - Publish VDA5050 state messages

- [ ] Evaluate Open-RMF integration (optional)
  - Fleet adapter for Warehouser robots
  - Building infrastructure integration
  - Task auctioning for multi-fleet optimization

**Verification**: Compatible with commercial fleet management systems

## Interface Definitions

### ROS2 Services (Phase 0-1)

**Per-Robot Topic Namespacing**:
```
/robot0/cmd_vel          geometry_msgs/Twist
/robot1/cmd_vel          geometry_msgs/Twist
/robot{N}/cmd_vel        geometry_msgs/Twist

/robot0/sim/pick         warehouser_msgs/srv/Pick
/robot1/sim/pick         warehouser_msgs/srv/Pick
```

**Zone Reservation Service** (Phase 1):
```cpp
// warehouser_msgs/srv/ReserveZone.srv
uint32 robot_id
uint32 zone_id
float32 entry_time
float32 exit_time
uint32 priority
---
bool success
string message
```

**Multi-Agent Path Planning Service** (Phase 2):
```cpp
// warehouser_msgs/srv/PlanMultiAgentPath.srv
uint32[] robot_ids
geometry_msgs/Point[] starts
geometry_msgs/Point[] goals
---
bool success
nav_msgs/Path[] paths
string message
```

### Python Interfaces (Phase 3)

**MAPPO Configuration**:
```python
from pydantic import BaseModel

class MAPPOConfig(BaseModel):
    n_agents: int
    obs_dim: int
    global_state_dim: int
    action_dim: int
    hidden_dim: int = 256
    lr_actor: float = 3e-4
    lr_critic: float = 1e-3
    gamma: float = 0.99
    gae_lambda: float = 0.95
    clip_ratio: float = 0.2
    value_clip: float = 0.2
    entropy_coef: float = 0.01
    max_grad_norm: float = 0.5
    parameter_sharing: bool = True
```

**Global State Construction**:
```python
def get_global_state(self) -> np.ndarray:
    """
    Construct global state for centralized critic.

    Returns:
        np.ndarray: [robot_positions, robot_velocities, task_locations,
                     task_statuses, zone_occupancy, warehouse_map]
    """
    # Implementation in training/training/envs/ros_env.py
```

### C++ Interfaces (Phase 1-2)

**ZoneManager** (Phase 1):
```cpp
namespace warehouser_traffic {

struct Zone {
    uint32_t id;
    std::string name;
    std::vector<std::pair<float, float>> boundary_points;
    uint32_t capacity;
    bool is_nonstop_area;
    std::unordered_set<uint32_t> current_robots;
};

class ZoneManager {
public:
    auto add_zone(uint32_t zone_id, std::string name,
                  std::vector<std::pair<float, float>> boundary,
                  uint32_t capacity, bool is_nonstop) -> void;

    [[nodiscard]] auto get_zone_at_position(float x, float y) const
        -> std::expected<uint32_t, std::string>;

    [[nodiscard]] auto request_reservation(ZoneReservation reservation)
        -> std::expected<bool, std::string>;

    [[nodiscard]] auto detect_deadlock() const -> std::vector<uint32_t>;
};

} // namespace warehouser_traffic
```

**CBS Planner** (Phase 2):
```cpp
namespace warehouser_planning {

struct Constraint {
    uint32_t agent_id;
    std::pair<int, int> position;
    uint32_t timestep;
};

struct Conflict {
    uint32_t agent1;
    uint32_t agent2;
    std::pair<int, int> position;
    uint32_t timestep;
    std::string conflict_type;  // "vertex" or "edge"
};

class CBS {
public:
    CBS(const Grid& map, const std::unordered_map<uint32_t, Position>& starts,
        const std::unordered_map<uint32_t, Position>& goals);

    [[nodiscard]] auto find_paths()
        -> std::expected<std::unordered_map<uint32_t, Path>, std::string>;

private:
    [[nodiscard]] auto low_level_search(uint32_t agent_id,
                                        const std::vector<Constraint>& constraints)
        -> std::expected<Path, std::string>;

    [[nodiscard]] auto find_first_conflict(const std::unordered_map<uint32_t, Path>& paths)
        -> std::optional<Conflict>;
};

} // namespace warehouser_planning
```

## New Modules to Create

| Module | Purpose | Key Interfaces |
|--------|---------|----------------|
| `warehouser_traffic` | Zone-based traffic management and deadlock prevention | `ZoneManager`, `ReserveZone.srv`, deadlock detection |
| `warehouser_planning` | Multi-agent path finding using CBS/PM-CBS | `CBS`, `PlanMultiAgentPath.srv`, conflict resolution |
| `warehouser_bringup` | Multi-robot launch configurations | `multi_robot.launch.py`, Discovery Server config |
| `training/algorithms/mappo.py` | MAPPO algorithm with centralized critic | `MAPPO`, `Actor`, `CentralizedCritic`, `MAPPOConfig` |
| `warehouser_vda5050` (optional) | VDA5050 protocol adapter for fleet interoperability | `VDA5050Adapter`, MQTT message translation |

## Files to Modify

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp` | Replace single publishers with vectors, route actions by robot_id |
| `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp` | Add publisher/client vectors indexed by robot_id |
| `ros_ws/src/warehouser_simulation/src/world_manager.cpp` | Add robot-robot collision detection, rollback on collision |
| `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp` | Add `checkRobotCollision()` method |
| `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/reward_strategy.hpp` | Extend RewardContext with `robot_robot_collision` flag |
| `ros_ws/src/warehouser_msgs/msg/Entity.msg` | Add collision state flag (or use existing fields) |
| `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp` | Extend V3 with velocity/state info (6 dims per robot) |
| `training/training/envs/pettingzoo_env.py` | Add parallel action execution (vs. sequential) |
| `training/training/envs/ros_env.py` | Add `get_global_state()` method for centralized critic |
| `training/training/models/config.py` | Add `team_reward_weight` to MultiAgentConfig |

## Architecture Notes

### Modularity Principles

1. **Layered Architecture**: Each layer has clear responsibilities and well-defined interfaces
   - Communication layer handles ROS2 namespacing and discovery
   - Traffic layer handles zone control and deadlock prevention
   - Planning layer handles optimal path generation
   - Learning layer handles policy optimization
   - Fleet layer (optional) handles interoperability

2. **Service-Oriented**: Major functions exposed as ROS2 services
   - Zone reservation as service
   - Path planning as service
   - Enables testing, debugging, and third-party integration

3. **Incremental Deployment**: Each phase adds value independently
   - Phase 0: Fixes bugs, enables basic multi-robot operation
   - Phase 1: Adds safety (traffic management)
   - Phase 2: Adds efficiency (optimal path planning)
   - Phase 3: Adds intelligence (MARL coordination)
   - Phase 4+: Adds scale and interoperability

4. **Separation of Concerns**:
   - Simulation doesn't know about RL (WorldManager is RL-agnostic)
   - Traffic manager doesn't know about learning (rule-based)
   - Path planner doesn't know about rewards (optimization-based)
   - Learning agent doesn't know about physics (observation-based)

5. **Scalability**:
   - Per-robot namespaces enable clean isolation
   - Discovery Server reduces network overhead
   - Zone-based control prevents congestion
   - Parameter sharing enables variable team sizes
   - CBS guarantees optimal paths at scale

### Key Architectural Decisions

**Decision 1: ROS2 with Fast DDS Discovery Server**
- Rationale: Already using ROS2, Fast DDS well-supported, Discovery Server reduces overhead
- Alternative: VDA5050 adds complexity without immediate benefit

**Decision 2: PM-CBS for Path Planning**
- Rationale: Optimal guarantees, recent innovation (2025), topometric map efficiency
- Alternative: Prioritized planning is simpler but not optimal

**Decision 3: Hierarchical Traffic with Zone Control**
- Rationale: Simple, proven, integrates with CBS, prevents deadlocks
- Alternative: Fully learned traffic is research-heavy

**Decision 4: MAPPO with Parameter Sharing**
- Rationale: CTDE paradigm matches deployment, parameter sharing improves efficiency
- Alternative: IPPO is simpler but doesn't leverage coordination

**Decision 5: Incremental Phases**
- Rationale: Each phase delivers value, validates approach, enables testing
- Alternative: Big-bang implementation is risky

## Verification

### Phase 0 Success Criteria
- [ ] Actions for robot_id 0, 1, 2 all execute correctly
- [ ] Robots cannot occupy same position
- [ ] Collision penalty applied to both robots
- [ ] Simulation spawns correct number of robots on reset

### Phase 1 Success Criteria
- [ ] Zones defined in warehouse config
- [ ] Zone capacity enforced
- [ ] No deadlocks in 10-robot scenarios
- [ ] Nonstop areas prevent stopping

### Phase 2 Success Criteria
- [ ] CBS generates collision-free paths for 10 robots
- [ ] Paths respect zone reservations
- [ ] Replanning works on conflicts
- [ ] Path optimality verified

### Phase 3 Success Criteria
- [ ] MAPPO training converges
- [ ] MAPPO outperforms IPPO on fleet throughput
- [ ] Policies transfer across robot counts (2→5→10)
- [ ] Emergent coordination behaviors observed

### Phase 4 Success Criteria
- [ ] Discovery Server reduces network traffic
- [ ] 10 robots communicate without cross-talk
- [ ] Per-robot namespaces isolate topics
- [ ] Launch file spawns multi-robot system

### Phase 5 Success Criteria
- [ ] Extended observations improve coordination
- [ ] Team rewards balance individual/collective performance
- [ ] Per-robot goals enable parallel tasks
- [ ] Communication actions learned (if implemented)

### Phase 6 Success Criteria (Optional)
- [ ] VDA5050 adapter translates orders/state
- [ ] Compatible with commercial fleet systems
- [ ] Open-RMF integration functional (if pursued)

## Performance Metrics

Track across all phases:

**Throughput**:
- Tasks completed per hour
- Items moved per robot per hour
- Fleet-level efficiency

**Path Quality**:
- Average path length
- Path optimality vs. optimal baseline
- Replanning frequency

**Safety**:
- Near-collision events (proximity < 0.5m)
- Actual collisions
- Deadlock occurrences
- Deadlock resolution time

**Coordination**:
- Average wait time per robot
- Zone utilization percentage
- Load balancing variance across fleet

**Learning**:
- Training convergence rate
- Sample efficiency (timesteps to convergence)
- Transfer success across fleet sizes
- Generalization to new layouts

## Critical Code Locations Reference

### Multi-Robot Training Entry Points
- `training/training/envs/pettingzoo_env.py:25-331` — PettingZoo ParallelEnv
- `training/training/models/config.py:96-114` — MultiAgentConfig

### ROS2 Service Interfaces
- `ros_ws/src/warehouser_msgs/srv/RLStep.srv` — Per-robot step service
- `ros_ws/src/warehouser_msgs/srv/RLReset.srv` — Multi-robot reset service
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:86-189` — Service handlers

### Critical TODOs (Phase 0 - BLOCKING)
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:194` — Action routing bug
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:242` — Simulation reset bug
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:206-213` — Collision detection bug

### Observation System
- `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp:22-26` — V3_MultiRobot
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp:84-171` — V3 implementation

### Simulation Core
- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp:70-83` — Multi-robot accessors
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:114-136` — Parallel robot update loop

### Reward Architecture
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/reward_calculator.hpp:48-60` — Multi-robot rewards
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/exploration_reward.hpp` — Shared coverage tracking

## Sources

This task consolidates findings from:

- **[S]** `S.md` — Multi-robot coordination research (MAPF, CBS, Open-RMF, VDA5050, MARL, ROS2 communication, traffic management)
- **[I]** `I.md` — Current codebase introspection (PettingZoo implementation, observation system, simulation layer, critical bugs)
- **[T]** `T.md` — Implementation templates (ROS2 namespacing, MAPPO, zone management, CBS, VDA5050, parameter sharing)

## Next Steps

1. **Immediate**: Fix Phase 0 bugs in `rl_bridge_node.cpp` and `world_manager.cpp`
2. **Week 1**: Create `warehouser_traffic` package, implement ZoneManager
3. **Week 2**: Create `warehouser_planning` package, implement CBS
4. **Week 3**: Implement MAPPO algorithm, train with 2-5 robots
5. **Week 4**: Configure ROS2 namespacing, test with 10 robots
6. **Month 2**: Extended observations, team rewards, curriculum learning
7. **Month 3+**: VDA5050 integration (if needed), production deployment

The multi-robot coordination architecture enables Warehouser to scale from single-robot RL experiments to realistic warehouse fleet simulations with 10-100 coordinated robots. By addressing critical bugs first and building incrementally, each phase adds value while maintaining system stability.
