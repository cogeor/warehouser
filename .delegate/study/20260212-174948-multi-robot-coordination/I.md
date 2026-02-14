# Introspect: Multi-Robot Coordination Architecture Analysis

Created: 2026-02-12T18:15:00Z

## Focus

Deep analysis of Warehouser's multi-robot coordination capabilities, examining the current implementation across training, ROS2 services, simulation, and observation layers to identify architectural strengths and gaps for fleet coordination features.

## Findings

### 1. Multi-Agent Training Layer (PettingZoo Implementation)

**Strong Foundation:**
- `training/training/envs/pettingzoo_env.py:25-331`: Implements PettingZoo ParallelEnv API for multi-agent RL
- Agent IDs follow `robot_{i}` pattern (line 50), supporting 1-10 agents (configurable)
- Per-agent observation/action spaces properly structured (lines 54-72)
- Shared reward option available via `MultiAgentConfig.shared_reward` (line 286-289)
- Sequential stepping through agents with service calls (lines 239-299)

**Critical Gap - Truly Parallel Actions:**
- `training/training/envs/pettingzoo_env.py:239-299`: Current implementation steps robots sequentially via individual RLStep service calls
- Each robot gets its own `RLStep.Request()` with `robot_id`, but calls are sequential, not simultaneous
- This creates coordination artifacts: robots act in turn-based fashion rather than true parallel execution
- For MARL algorithms (MAPPO, QMIX), this breaks the simultaneity assumption of ParallelEnv

**Observation Independence:**
- `training/training/envs/pettingzoo_env.py:179-193`: Reset returns per-robot observations from `response.observations` array
- Each robot receives its own observation slice from the RL bridge

### 2. ROS2 Multi-Robot Service Interface

**Per-Robot Identification:**
- `ros_ws/src/warehouser_msgs/srv/RLStep.srv:5`: `robot_id` field (default 0 for backward compatibility)
- `ros_ws/src/warehouser_msgs/srv/RLReset.srv:6`: `robot_count` field configures number of robots
- `ros_ws/src/warehouser_msgs/srv/RLReset.srv:12`: Returns array `observations[]` for all robots

**RL Bridge Multi-Robot Support:**
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp:52-56`: Maintains per-robot state vectors
  - `robot_count_` tracks number of active robots
  - `prev_world_states_` vector indexed by robot_id
  - `reward_calculators_` vector - one per robot for independent rewards
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:140-157`: Reset dynamically resizes per-robot state based on `robot_count`
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:86-130`: Step validates robot_id and calculates per-robot rewards

**Critical Action Routing Gap:**
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:194-216`: TODO comment exposes fundamental limitation:
  ```cpp
  // TODO: For multi-robot, need per-robot cmd_vel topics or action message
  // For now, use robot_id 0 for backward compatibility
  if (robot_id == 0) {
      cmd_pub_->publish(cmd);
  } else {
      RCLCPP_WARN_ONCE(get_logger(),
          "Multi-robot actions not yet routed to per-robot topics");
  }
  ```
- Actions for `robot_id > 0` are silently ignored with a warning
- No per-robot topic namespace (`/robot_0/cmd_vel`, `/robot_1/cmd_vel`, etc.)
- Single publishers for `/cmd_vel`, `/sim/pick`, `/sim/unpick` - all control robot 0

**Simulation Reset Gap:**
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:242-244`: TODO comment indicates simulation doesn't accept robot_count
  ```cpp
  // TODO: Pass robot_count to simulation reset if multi-robot sim is supported
  (void)robot_count;  // Suppress unused warning
  ```
- Reset service doesn't inform simulation layer of desired robot count

**Observation Service Gap:**
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:259-261`: GetObservation service lacks robot_id parameter
  ```cpp
  // TODO: Add robot_id to GetObservation.srv when per-robot obs service is available
  (void)robot_id;
  ```

### 3. Observation System Multi-Robot Architecture

**V3_MultiRobot Observation Version:**
- `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp:22-26`: Dedicated multi-robot observation format
  - 8 dims: ego state (position, goal, carrying status)
  - 3 * max_other_robots dims: relative positions `[rel_x, rel_y, rel_theta]` for each other robot
  - Default `max_other_robots = 3` (line 32)
  - Total dimension: 8 + 3*3 = 17 (matches `MultiAgentConfig.obs_dim = 17`)

**Ego-Centric Observations:**
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp:84-139`: `buildV3` implementation
  - Lines 92-115: First 8 dims same as V1 (ego position, goal vector, carrying flag)
  - Lines 118-136: Transforms other robots into ego's reference frame using rotation matrix
  - Lines 124-134: Zero-pads if fewer than `max_other_robots` present
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp:141-154`: `findRobotByIndex` iterates entities to find robot by index
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp:156-170`: `findOtherRobots` collects all robots except ego

**Observation Isolation:**
- Each robot receives fully independent observation centered on its own pose
- No shared/global state in observations
- Other robots appear as movable obstacles with relative positions only
- No explicit communication channel in observations

### 4. Simulation Layer Multi-Robot Foundation

**WorldManager Multi-Robot Support:**
- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp:70-83`: Multi-robot accessor methods
  - `robot(size_t index)` returns pointer to specific robot (backward compatible with default index=0)
  - `robotCount()` returns number of robots in world
  - `addRobot(config)` dynamically adds robots
- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp:17-33`: Configuration supports both legacy and multi-robot spawn
  - `robot_spawn` array for single robot (backward compat)
  - `robot_spawns` vector for multiple robots
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:11-26`: Constructor handles both spawn modes
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:28-33`: `addRobot` creates robot and stores initial config for reset

**Robot Entity Structure:**
- `ros_ws/src/warehouser_msgs/msg/Entity.msg:5-7`: Single entity type enum covers robots, objects, walls, zones
- All robots share entity type `TYPE_ROBOT = 0`
- No explicit robot-to-robot relationship tracking in entity structure
- `ros_ws/src/warehouser_msgs/msg/WorldState.msg:4`: Flat array of all entities (robots mixed with objects/walls/zones)

**Collision Detection Gap:**
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:206-213`: `checkCollision` only checks walls
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
- No robot-robot collision detection
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:114-136`: Step loop updates each robot independently
- Robots can occupy same position without collision penalty

**Movement Synchronization:**
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:114-136`: All robots updated in single `step(dt)` call
- Truly parallel physics update (not sequential)
- No coordination or conflict resolution between robot movements

### 5. Reward Architecture for Multi-Robot

**Per-Robot Independent Rewards:**
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp:56`: Vector of `RewardCalculator` instances - one per robot
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:115-118`: Each robot gets individual reward based on its own state transition
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/reward_calculator.hpp:48-60`: Multi-robot calculate overload accepts `robot_index`

**Exploration Reward with Coverage Tracking:**
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/exploration_reward.hpp:10-17`: ExplorationConfig includes coverage tracking
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/occupancy_tracker.hpp:19-69`: Shared occupancy grid for coverage
- Coverage tracker is **mutable** in ExplorationRewardStrategy (line 56) - implies shared state across robots
- No distinction between cells visited by different robots - global coverage metric

**No Team Reward Options:**
- `training/training/models/config.py:103`: `shared_reward` boolean in MultiAgentConfig
- `training/training/envs/pettingzoo_env.py:286-289`: Implements shared reward by averaging all rewards
- No sophisticated team reward shaping (e.g., coordination bonuses, formation rewards)
- No penalty for interference or blocking other robots

### 6. Message Definitions Multi-Robot Patterns

**Robot Identification:**
- `ros_ws/src/warehouser_msgs/msg/Entity.msg:10`: String `id` field for unique identification
- No standardized robot ID format beyond Python-side `robot_{i}` naming
- No robot-specific metadata (e.g., capabilities, roles, team assignment)

**WorldState Broadcasting:**
- `ros_ws/src/warehouser_msgs/msg/WorldState.msg:4`: Single flat entity array
- No spatial indexing or proximity information
- No explicit robot-robot relationships

**Goal Representation:**
- `ros_ws/src/warehouser_msgs/msg/Goal.msg:1-9`: Single goal message (not per-robot)
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:273-301`: `setRandomGoal` sets one shared goal for all robots
- No multi-goal assignment or task allocation

### 7. Coordination Gaps Analysis

#### 7.1 Path Planning & Traffic Management
**Status: Not Implemented**
- No path planning layer - robots use reactive RL policies only
- No trajectory prediction for other robots
- No multi-agent path finding (MAPF) integration
- No priority-based motion planning
- No reservation system for workspace regions

**Evidence:**
- Grep search for "path.*plan|mapf|trajectory" in codebase returns no hits in core modules
- No CBS (Conflict-Based Search) or PBS (Priority-Based Search) implementation
- Movement is immediate reactive response to observations

#### 7.2 Inter-Robot Communication
**Status: Not Implemented**
- No explicit communication channel in observations or actions
- Robots perceive each other only through relative position observations
- No message passing protocol between robots
- No intention sharing or plan broadcasting

**Evidence:**
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp:118-136`: Other robots encoded as 3D position vectors only
- No communication action in action space (only: linear, angular, pick, place)
- No shared memory or blackboard architecture

#### 7.3 Task Allocation
**Status: Not Implemented**
- Single shared goal for all robots (lines referenced above)
- No task queue or assignment mechanism
- No role differentiation (all robots identical)
- No dynamic task reallocation on failure

**Evidence:**
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:160-161,273-301`: Single `current_goal_` and `setRandomGoal()` function
- No task manager integration visible in RL bridge
- Task manager module exists in `.arch/task_manager/README.md` but not connected to multi-robot system

#### 7.4 Collision Avoidance
**Status: Partial - Observable but No Penalty**
- Robots can observe other robots' positions (V3 observation)
- No explicit robot-robot collision detection in physics
- No collision penalty in reward function
- Robots can overlap without consequence

**Evidence:**
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:206-213`: Wall collision only
- Reward strategies check wall collision but not robot-robot collision
- No radius-based proximity detection between robot entities

#### 7.5 Fleet Coordination Protocols
**Status: Not Implemented**
- No VDA 5050 support (warehouse fleet standard)
- No Open-RMF integration
- No centralized fleet manager
- No traffic control zones or virtual traffic lights

**Evidence:**
- Grep for "vda|rmf|fleet" returns only study documents, not code
- No protocol adapters in codebase

## Proposal: Incremental Multi-Robot Coordination Roadmap

### Phase 1: Complete Basic Multi-Robot Infrastructure (P0 - Required for Functionality)

#### 1.1 Fix Action Routing
**Location:** `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:191-216`

**Problem:** Actions for robot_id > 0 are ignored

**Solution:**
- Modify publishers to use per-robot namespaces: `/robot_{id}/cmd_vel`, `/robot_{id}/sim/pick`, `/robot_{id}/sim/unpick`
- Store publishers in vector indexed by robot_id
- Route actions based on request robot_id

**Impact:** Enables actual multi-robot control

#### 1.2 Implement Robot-Robot Collision Detection
**Location:** `ros_ws/src/warehouser_simulation/src/world_manager.cpp:114-136`

**Problem:** Robots can overlap without penalty

**Solution:**
- Add `checkRobotCollision(robot_index)` method to WorldManager
- In step loop, check each robot against all other robots using radius (Robot::kRadius = 0.3m)
- On collision, rollback both robots to previous positions
- Add collision flag to entity state for reward calculation

**Impact:** Enforces physical realism and enables collision-avoidance learning

#### 1.3 Add Robot-Robot Collision Penalty to Rewards
**Location:** `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/reward_strategy.hpp`

**Problem:** No penalty for robot interference

**Solution:**
- Extend RewardContext with `robot_robot_collision` flag
- Add `robot_collision_penalty` to RewardConfig (e.g., -50.0)
- Check collision state in reward calculation

**Impact:** Incentivizes spatial coordination and avoidance

### Phase 2: Communication & Cooperation (P1 - Enables Learning)

#### 2.1 Extend Observations with Robot State Information
**Location:** `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp:22-26`

**Problem:** Limited information about other robots (position only)

**Enhancement:**
- Add to V3 observation per other robot: `[rel_x, rel_y, rel_theta, v, omega, is_carrying]` (6 dims instead of 3)
- Update `max_other_robots` config to balance observation dimension
- Encode velocity and carrying state of other robots

**Impact:** Enables prediction of other robots' intentions

#### 2.2 Implement Shared Team Reward Components
**Location:** `training/training/models/config.py:96-114`

**Problem:** Only binary shared_reward flag (average all rewards)

**Enhancement:**
- Add `team_reward_weight` to MultiAgentConfig (0.0 = fully independent, 1.0 = fully shared)
- Implement weighted combination: `final_reward = (1 - w) * individual + w * team_avg`
- Add coordination bonus for robots in formation or working on same task

**Impact:** Balances individual performance with team cooperation

### Phase 3: Explicit Coordination Mechanisms (P2 - Advanced Features)

#### 3.1 Add Per-Robot Goal Assignment
**Location:** `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:273-301`

**Problem:** Single shared goal for all robots

**Solution:**
- Change `current_goal_` to `std::vector<warehouser_msgs::msg::Goal> robot_goals_`
- Modify `setRandomGoal()` to assign different goals to different robots
- Pass per-robot goal to observation builder
- Add goal reassignment on completion

**Impact:** Enables parallel task execution and load balancing

#### 3.2 Implement Simple Traffic Zones
**Location:** `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp`

**Problem:** No traffic management, potential for deadlocks in narrow passages

**Solution:**
- Extend Zone entity with `max_occupancy` field
- Track robot presence in zones
- Add zone occupancy to observations
- Penalize overcrowded zones in reward

**Impact:** Emergent traffic patterns, reduced congestion

#### 3.3 Add Communication Actions
**Location:** `ros_ws/src/warehouser_msgs/srv/RLStep.srv`

**Problem:** No explicit communication channel

**Solution:**
- Extend action space: `[linear, angular, pick, place, message]` where message is float signal
- Broadcast robot messages in WorldState or separate topic
- Add `other_robot_messages` to observations
- Train policies to use communication for coordination

**Impact:** Enables learned communication protocols (e.g., claiming tasks, requesting help)

### Phase 4: Fleet-Level Coordination (P3 - Production)

#### 4.1 Integrate Multi-Agent Path Finding (MAPF)
**Problem:** RL policies don't guarantee collision-free paths in dense scenarios

**Solution:**
- Implement CBS (Conflict-Based Search) or PBS planner
- Use RL policy as heuristic for path cost
- Plan trajectories for all robots jointly
- Execute via low-level controller

**Impact:** Provable collision-freedom, scalability to 10+ robots

#### 4.2 Add VDA 5050 Protocol Support
**Problem:** No interoperability with commercial fleet management systems

**Solution:**
- Implement VDA 5050 message adapter node
- Map internal states to VDA 5050 `state` messages
- Accept VDA 5050 `order` messages for task assignment
- Support `instantActions` for emergency stop

**Impact:** Integration with existing warehouse automation stacks

## Critical Code Locations Reference

### Multi-Robot Training Entry Points
- `training/training/envs/pettingzoo_env.py:25-331` — PettingZoo ParallelEnv implementation
- `training/training/models/config.py:96-114` — MultiAgentConfig

### ROS2 Service Interfaces
- `ros_ws/src/warehouser_msgs/srv/RLStep.srv:1-19` — Per-robot step service
- `ros_ws/src/warehouser_msgs/srv/RLReset.srv:1-15` — Multi-robot reset service
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:86-189` — Service handlers

### Critical TODOs Requiring Immediate Attention
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:194` — Action routing to per-robot topics
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:242` — Simulation reset with robot_count
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:259` — Per-robot observation service

### Observation System
- `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp:22-26` — V3_MultiRobot definition
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp:84-171` — V3 implementation with other robot tracking

### Simulation Core
- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp:70-83` — Multi-robot accessors
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:114-136` — Parallel robot update loop
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:206-213` — Collision detection (walls only, needs robot-robot)

### Reward Architecture
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/reward_calculator.hpp:48-60` — Multi-robot reward interface
- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/exploration_reward.hpp:10-62` — Exploration with shared coverage tracking

## Summary

Warehouser has established a **solid foundation for multi-robot coordination** with:
1. PettingZoo-compliant multi-agent training environment
2. Per-robot observations with V3_MultiRobot version encoding other robots
3. Service interfaces supporting robot_id and robot_count
4. Scalable simulation world manager with multi-robot support
5. Independent per-robot reward calculation

**Critical gaps preventing full multi-robot functionality:**
1. **Action routing** — Only robot_0 receives commands (lines 194-216 in rl_bridge_node.cpp)
2. **Robot-robot collisions** — No detection or penalty (world_manager.cpp:206-213)
3. **Simulation reset** — Doesn't accept robot_count parameter (rl_bridge_node.cpp:242)
4. **Sequential stepping** — PettingZoo env steps robots sequentially not parallel (pettingzoo_env.py:239-299)

**Coordination features requiring implementation:**
- Path planning / MAPF
- Inter-robot communication
- Task allocation
- Traffic management
- Fleet protocols (VDA 5050)

**Recommended approach:** Address Phase 1 gaps first (action routing, collision detection/penalty) to establish functional multi-robot system, then incrementally add coordination mechanisms in Phases 2-4 based on training performance and use case requirements.
