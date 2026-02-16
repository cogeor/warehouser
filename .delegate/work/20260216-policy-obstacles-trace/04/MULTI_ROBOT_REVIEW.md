# Multi-Robot Support Review

## Executive Summary

The warehouser codebase has **partial multi-robot support** already implemented at the simulation and RL bridge layers, but significant gaps remain in the observations, inference, safety, task management, and frontend layers. This document identifies what works, what needs changes, and provides specific recommendations for full multi-robot support.

**Current Status:**
- WorldManager: Supports multiple robots
- SimReset/RLReset services: Support robot_count parameter
- RLStep service: Support robot_id parameter
- Entity message: Includes in_robot_collision flag
- ObservationBuilder: Has V3_MultiRobot version
- Frontend: Basic selectedRobotId support exists

**Gaps:**
- Topics use global namespaces (no per-robot namespacing)
- Observations node only tracks first robot
- Inference node operates on single robot
- Safety node has no multi-robot awareness
- Task manager tracks single robot state
- Lidar visualization only shows one robot's scan
- TF frames not namespaced per robot

---

## 1. ROS Topics Analysis

### 1.1 Current Topic Structure

| Topic | File | Direction | Multi-Robot Status |
|-------|------|-----------|-------------------|
| `/cmd_vel` | simulation_node.cpp:104 | Sub | BROKEN - global, only first robot receives |
| `/sim/move_entity` | simulation_node.cpp:108 | Sub | OK - uses entity ID |
| `/sim/pick` | simulation_node.cpp:113 | Sub | BROKEN - applies to first robot only |
| `/sim/unpick` | simulation_node.cpp:117 | Sub | BROKEN - applies to first robot only |
| `/world/state` | simulation_node.cpp:122 | Pub | OK - contains all robots in entities[] |
| `/clock` | simulation_node.cpp:124 | Pub | OK - global clock |
| `/observations` | observations_node.cpp:53 | Pub | BROKEN - single observation |
| `/observations/lidar_debug` | observations_node.cpp:55 | Pub | BROKEN - first robot only |
| `/scan` | observations_node.cpp:57 | Pub | BROKEN - first robot only |
| `/odom` | observations_node.cpp:58 | Pub | BROKEN - first robot only |
| `/task/goal` | task_manager_node.cpp:36 | Pub | BROKEN - global goal |
| `/task/status` | task_manager_node.cpp:37 | Pub | BROKEN - single task |
| `/inference/action` | inference_node.cpp:29 | Pub | BROKEN - single robot |
| `/cmd_vel_raw` | inference_node.cpp:28 | Pub | BROKEN - global |
| `/safety/status` | safety_node.cpp:33 | Pub | BROKEN - single status |

### 1.2 RLBridge Multi-Robot Publishers (Partial Implementation)

The RLBridge already implements per-robot topic namespacing in `rl_bridge_node.cpp:242-261`:

```cpp
void RLBridgeNode::initializeRobotPublishers(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        std::string prefix = "/robot" + std::to_string(i);

        cmd_vel_pubs_.push_back(
            create_publisher<geometry_msgs::msg::Twist>(prefix + "/cmd_vel", 10));
        pick_pubs_.push_back(
            create_publisher<std_msgs::msg::Empty>(prefix + "/sim/pick", 10));
        unpick_pubs_.push_back(
            create_publisher<std_msgs::msg::Empty>(prefix + "/sim/unpick", 10));
    }
}
```

**However**, the simulation_node.cpp still subscribes to global topics and only applies commands to the first robot.

### 1.3 Recommended Topic Namespacing Convention

Following ROS2 best practices (REP-105), use robot-prefixed namespaces:

```
/robot0/cmd_vel
/robot0/odom
/robot0/scan
/robot0/observations
/robot0/task/goal
/robot0/task/status
/robot0/inference/action
/robot0/safety/status

/robot1/cmd_vel
/robot1/odom
...

/world/state          (global - contains all entities)
/clock                (global - simulation clock)
/rl/step              (global - service accepts robot_id)
/rl/reset             (global - service accepts robot_count)
```

---

## 2. ROS Services Analysis

### 2.1 Current Service Definitions

| Service | File | Multi-Robot Status |
|---------|------|-------------------|
| `SimReset.srv` | warehouser_msgs/srv | OK - has robot_count field |
| `SimStep.srv` | warehouser_msgs/srv | OK - steps all robots together |
| `RLReset.srv` | warehouser_msgs/srv | OK - robot_count + observations[] |
| `RLStep.srv` | warehouser_msgs/srv | OK - robot_id field |
| `GetObservation.srv` | warehouser_msgs/srv | BROKEN - no robot_id field |
| `LoadModel.srv` | warehouser_msgs/srv | PARTIAL - single model path |
| `SetGoal.srv` | warehouser_msgs/srv | BROKEN - no robot_id field |

### 2.2 Service Changes Required

**GetObservation.srv** - Add robot_id:
```
# Request
int32 robot_id 0      # Robot index (default 0 for backward compat)
---
# Response
warehouser_msgs/Observation observation
```

**SetGoal.srv** - Add robot_id:
```
# Request
int32 robot_id 0
float32 x
float32 y
string target_color
---
# Response
bool success
string message
```

**LoadModel.srv** - Consider per-robot models or shared:
```
# Option 1: Per-robot model
int32 robot_id 0
string model_path
---
bool success
string message

# Option 2: Shared model (current) - probably sufficient
# All robots use same policy network
```

---

## 3. TF Frame Conventions

### 3.1 Current Frame Usage

| Frame | File | Context |
|-------|------|---------|
| `odom` | observations_node.cpp:147 | Odometry message header |
| `base_link` | observations_node.cpp:148 | Odometry child frame |
| `base_laser` | lidar_simulator.cpp:125 | LaserScan frame |

### 3.2 Multi-Robot TF Frame Convention (REP-105)

Per ROS2 conventions, each robot needs its own TF tree:

```
                    world
                      |
         +-----------++-----------+
         |            |            |
    robot0/odom  robot1/odom  robot2/odom
         |            |            |
  robot0/base_link  robot1/base_link  ...
         |            |
  robot0/base_laser robot1/base_laser
```

**Required Changes:**

1. **observations_node.cpp** - Parameterize frame names:
```cpp
// Current:
msg.header.frame_id = "odom";
msg.child_frame_id = "base_link";

// Multi-robot:
std::string robot_ns = "robot" + std::to_string(robot_index);
msg.header.frame_id = robot_ns + "/odom";
msg.child_frame_id = robot_ns + "/base_link";
```

2. **lidar_simulator.cpp** - Accept frame_id parameter:
```cpp
// Already parameterized, but caller needs to pass robot-specific frame
auto scan_msg = lidar_.buildLaserScanMsg(
    robot->x, robot->y, robot->theta, world, now(),
    "robot0/base_laser");  // Needs robot index
```

3. **Publish TF transforms** - Add TF broadcaster:
```cpp
// New: Publish odom -> base_link transform for each robot
geometry_msgs::msg::TransformStamped t;
t.header.stamp = now();
t.header.frame_id = robot_ns + "/odom";
t.child_frame_id = robot_ns + "/base_link";
t.transform.translation.x = robot->x;
t.transform.translation.y = robot->y;
// ... rotation
tf_broadcaster_->sendTransform(t);
```

---

## 4. WorldManager State Management

### 4.1 Current Implementation

**File:** `ros_ws/src/warehouser_simulation/src/world_manager.cpp`

WorldManager already supports multiple robots:

```cpp
// Multiple robots stored in vector
std::vector<std::unique_ptr<Robot>> robots_;

// Robot access by index
Robot* robot(size_t index = 0);

// Robot count query
size_t robotCount() const { return robots_.size(); }

// Reset with specific count
void resetWithRobotCount(size_t robot_count);

// Robot-robot collision detection
bool checkRobotCollision(size_t robot_index) const;
```

**In step()** (world_manager.cpp:155-203):
- Iterates all robots
- Clears collision flags
- Updates each robot independently
- Checks wall and robot-robot collisions
- Sets `in_robot_collision` flag per robot

### 4.2 Issues Remaining

1. **findClosestByColor()** uses first robot as reference (line 237-240):
```cpp
PickableObject* WorldManager::findClosestByColor(const std::string& color) {
    if (robots_.empty()) return nullptr;
    const auto& ref_robot = robots_[0];  // BROKEN: hardcoded robot 0
    // ...
}
```

2. **cmdVelCallback** in simulation_node.cpp only updates first robot:
```cpp
void SimulationNode::cmdVelCallback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    if (auto* robot = world_.robot()) {  // Gets robot(0)
        robot->setCommand(...);
    }
}
```

3. **pickCallback/unpickCallback** only operate on first robot.

### 4.3 Recommended Changes

**simulation_node.cpp:**
- Subscribe to per-robot topics: `/robot{N}/cmd_vel`, `/robot{N}/sim/pick`
- Or accept robot_id in message payload

**world_manager.cpp:**
- Add `findClosestByColor(const std::string& color, size_t robot_index)`

---

## 5. Observations Node Analysis

### 5.1 Current Implementation

**File:** `ros_ws/src/warehouser_observations/src/observations_node.cpp`

The node only tracks the first robot:

```cpp
const warehouser_msgs::msg::Entity* ObservationsNode::findRobot() const {
    for (const auto& entity : last_world_.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT = 0
            return &entity;  // Returns FIRST robot found
        }
    }
    return nullptr;
}
```

Single-robot publishers:
```cpp
obs_pub_ = create_publisher<...>("/observations", 10);
lidar_pub_ = create_publisher<...>("/observations/lidar_debug", 10);
scan_pub_ = create_publisher<...>("/scan", 10);
odom_pub_ = create_publisher<...>("/odom", 10);
```

### 5.2 ObservationBuilder Multi-Robot Support

**File:** `ros_ws/src/warehouser_observations/src/observation_builder.cpp`

Already supports multi-robot via V3_MultiRobot:

```cpp
warehouser_msgs::msg::Observation ObservationBuilder::build(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {  // Robot index parameter exists!

    switch (config_.version) {
        case ObservationVersion::V3_MultiRobot:
            return buildV3(world, goal, robot_index);
        // ...
    }
}
```

V3 includes relative positions of other robots:
```cpp
// First 5 dims: ego state
obs.data[0] = dx;  // goal delta
obs.data[1] = dy;
obs.data[2] = dist;
obs.data[3] = heading;
obs.data[4] = is_carrying;

// Remaining dims: relative positions of other robots
for (size_t i = 0; i < config_.max_other_robots; ++i) {
    obs.data[5 + i*3 + 0] = rel_x;
    obs.data[5 + i*3 + 1] = rel_y;
    obs.data[5 + i*3 + 2] = rel_theta;
}
```

### 5.3 Required Changes for Observations Node

**Option A: Per-Robot Publishers**
```cpp
// Create per-robot publishers
std::vector<rclcpp::Publisher<...>::SharedPtr> obs_pubs_;
std::vector<rclcpp::Publisher<...>::SharedPtr> lidar_pubs_;
std::vector<rclcpp::Publisher<...>::SharedPtr> scan_pubs_;
std::vector<rclcpp::Publisher<...>::SharedPtr> odom_pubs_;

void initializeRobotPublishers(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        std::string prefix = "/robot" + std::to_string(i);
        obs_pubs_.push_back(create_publisher<...>(prefix + "/observations", 10));
        lidar_pubs_.push_back(create_publisher<...>(prefix + "/lidar_debug", 10));
        // ...
    }
}
```

**Option B: Batched Publishers with Arrays**
```cpp
// New message type: ObservationArray.msg
warehouser_msgs/Observation[] observations  # Per-robot observations

// Single publisher for all
obs_array_pub_ = create_publisher<...>("/observations/batch", 10);
```

**Recommended: Option A** - Follows ROS conventions better, allows independent subscriptions.

---

## 6. Inference Node Analysis

### 6.1 Current Implementation

**File:** `ros_ws/src/warehouser_inference/src/inference_node.cpp`

Single-robot operation:
```cpp
// Single observation subscription
obs_sub_ = create_subscription<...>("/observations", 10, ...);

// Single command publisher
cmd_pub_ = create_publisher<...>("/cmd_vel_raw", 10);

// Single action publisher
action_pub_ = create_publisher<...>("/inference/action", 10);
```

### 6.2 Multi-Robot Options

**Option A: Single Node, Multiple Robots**
```cpp
class MultiRobotInferenceNode : public rclcpp::Node {
    size_t robot_count_;
    std::vector<rclcpp::Subscription<...>::SharedPtr> obs_subs_;
    std::vector<rclcpp::Publisher<...>::SharedPtr> cmd_pubs_;

    void initializeRobotInterfaces(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            std::string ns = "/robot" + std::to_string(i);
            obs_subs_.push_back(create_subscription<...>(ns + "/observations", ...));
            cmd_pubs_.push_back(create_publisher<...>(ns + "/cmd_vel_raw", ...));
        }
    }
};
```

**Option B: Multiple Node Instances (Composable)**
```cpp
// Launch file spawns N inference nodes with different namespaces
// Each node handles one robot
// Uses ROS2 namespacing via launch file
```

**Recommended: Option B** for deployment flexibility, but Option A during training for efficiency.

---

## 7. Safety Node Analysis

### 7.1 Current Implementation

**File:** `ros_ws/src/warehouser_safety/src/safety_node.cpp`

Single-robot safety:
```cpp
cmd_raw_sub_ = create_subscription<...>("/cmd_vel_raw", 10, ...);
lidar_sub_ = create_subscription<...>("/observations/lidar_debug", 10, ...);
cmd_pub_ = create_publisher<...>("/cmd_vel", 10);
status_pub_ = create_publisher<...>("/safety/status", 10);
```

### 7.2 Multi-Robot Requirements

1. **Per-robot safety monitoring**
2. **Robot-robot collision avoidance** (currently only wall detection)
3. **Cooperative safety** (e.g., yield at intersections)

**Recommended Approach:**

```cpp
class MultiRobotSafetyNode : public rclcpp::Node {
    struct RobotSafety {
        SafetyController controller;
        LidarData last_lidar;
        bool lidar_received = false;
    };

    std::vector<RobotSafety> robot_safety_;
    std::vector<rclcpp::Subscription<...>::SharedPtr> cmd_raw_subs_;
    std::vector<rclcpp::Subscription<...>::SharedPtr> lidar_subs_;
    std::vector<rclcpp::Publisher<...>::SharedPtr> cmd_pubs_;

    // Subscribe to /world/state for robot-robot distance checking
    rclcpp::Subscription<...>::SharedPtr world_sub_;
};
```

---

## 8. Task Manager Analysis

### 8.1 Current Implementation

**File:** `ros_ws/src/warehouser_task/src/task_manager_node.cpp`

Single-robot task tracking:
```cpp
float robot_x_, robot_y_;
bool robot_is_carrying_;
TaskStateMachine state_machine_;  // Single state machine
```

Extracts only first robot from world state:
```cpp
for (const auto& entity : msg->entities) {
    if (entity.type == 0) {  // Robot
        robot_x_ = entity.x;
        robot_y_ = entity.y;
        robot_is_carrying_ = entity.is_carrying;
        break;  // BREAKS after first robot!
    }
}
```

### 8.2 Multi-Robot Task Management Options

**Option A: Per-Robot Task Managers (Decentralized)**
```cpp
// Each robot has independent task/goal
class MultiRobotTaskManager : public rclcpp::Node {
    std::vector<TaskStateMachine> state_machines_;
    std::vector<float> robot_x_, robot_y_;
    std::vector<bool> robot_is_carrying_;
    std::vector<warehouser_msgs::msg::Goal> current_goals_;
};
```

**Option B: Central Task Allocator (Centralized)**
```cpp
// Single manager assigns tasks to robots
// Handles task allocation, conflict resolution
class CentralTaskAllocator : public rclcpp::Node {
    std::map<std::string, Task> robot_tasks_;  // robot_id -> task

    void assignTask(const std::string& robot_id, const Task& task);
    void handleTaskComplete(const std::string& robot_id);
    std::optional<std::string> findAvailableRobot();
};
```

**Recommended: Start with Option A** for simplicity, then add Option B for coordination.

---

## 9. Frontend Analysis

### 9.1 Current Multi-Robot Support

**appStore.ts:**
```typescript
// Multi-robot selection exists
selectedRobotId: string | null
setSelectedRobotId: (id: string | null) => void

// Selector for filtering robots
export function selectRobots(state: AppState): Entity[] {
    return state.entities.filter(e => e.type === 'robot')
}

export function selectSelectedRobot(state: AppState): Entity | undefined {
    const robots = selectRobots(state)
    if (state.selectedRobotId) {
        return robots.find(r => r.id === state.selectedRobotId)
    }
    return robots[0] // Default to first robot
}
```

**CanvasRobots.tsx:**
```typescript
// Already renders multiple robots with selection
export function CanvasRobots({
    robots,              // Array of robots
    selectedRobotId,     // Which is selected
    onRobotSelect,       // Click handler
    ...
}) {
    return (
        <>
            {robots.map((robot) => {
                const isSelected = robot.id === selectedRobotId
                // Render each robot with selection highlight
            })}
        </>
    )
}
```

### 9.2 Frontend Gaps

1. **Lidar visualization** - Only shows one robot's scan
2. **Trajectory trace** - Only traces selected robot (OK)
3. **Task status panel** - Shows single task status
4. **Goal setting** - No per-robot goal setting UI
5. **Keyboard control** - Controls selected robot only (OK)

### 9.3 Required Frontend Changes

**CanvasLidar.tsx** - Support multiple robots:
```typescript
// Option A: Pass array of lidar data
interface MultiLidarProps {
    robots: Array<{
        robotX: number
        robotY: number
        robotTheta: number
        ranges: number[]
    }>
    selectedRobotId?: string  // Only show selected robot's lidar
}

// Option B: Subscribe to multiple topics
// /robot0/lidar_debug, /robot1/lidar_debug, etc.
```

**RosDataBridge.tsx** - Subscribe to per-robot topics:
```typescript
// Currently subscribes to global /observations/lidar_debug
// Need to either:
// 1. Subscribe to /robot{N}/lidar_debug for each robot
// 2. Or new message type with all robots' lidar
```

**StatusPanel.tsx** - Show per-robot status:
```typescript
// Add robot selector dropdown
// Show task status for selected robot
// Or show grid of all robots' statuses
```

---

## 10. Training Environment Analysis

### 10.1 Current Implementations

**ROSGymEnv (ros_env.py):**
- Single-agent Gymnasium environment
- Communicates via `/rl/step` service
- Action masking based on carrying state
- Does NOT use robot_id in step request

**WarehouseParallelEnv (pettingzoo_env.py):**
- Multi-agent PettingZoo ParallelEnv
- Uses robot_id in RLStep requests
- Supports shared reward option
- Already functional for multi-robot

**StandaloneEnv (standalone_env.py):**
- No ROS dependency
- Single-robot only
- Good for algorithm testing

### 10.2 Training Gaps

1. **ROSGymEnv needs robot_id support** for single-agent training of specific robot
2. **StandaloneEnv needs multi-robot version** for fast iteration
3. **Observation version mismatch** - V3_MultiRobot may not match env config

### 10.3 Recommended Training Changes

**ros_env.py:**
```python
class ROSGymEnv(gym.Env):
    def __init__(self, config: EnvConfig | None = None, robot_id: int = 0) -> None:
        self.robot_id = robot_id
        # ...

    def step(self, action: Action):
        request = RLStep.Request()
        request.robot_id = self.robot_id  # Use specified robot
        # ...
```

**standalone_env.py:**
```python
class MultiRobotStandaloneEnv(gym.Env):
    """Standalone multi-robot environment without ROS."""

    def __init__(self, config: MultiAgentConfig | None = None):
        self.num_robots = config.num_agents
        self._robots: list[RobotState] = []
        # ...
```

---

## 11. Summary of Required Changes

### 11.1 Critical Path (Minimum Viable Multi-Robot)

| Priority | Component | Change | File(s) |
|----------|-----------|--------|---------|
| P0 | SimulationNode | Subscribe to per-robot topics | simulation_node.cpp |
| P0 | ObservationsNode | Per-robot publishers | observations_node.cpp |
| P0 | GetObservation.srv | Add robot_id field | GetObservation.srv |
| P1 | SafetyNode | Per-robot safety tracking | safety_node.cpp |
| P1 | InferenceNode | Per-robot inference | inference_node.cpp |
| P1 | Frontend | Multi-robot lidar display | RosDataBridge.tsx, appStore.ts |

### 11.2 Full Multi-Robot Support

| Priority | Component | Change | File(s) |
|----------|-----------|--------|---------|
| P2 | TF Frames | Per-robot frame namespacing | observations_node.cpp, lidar_simulator.cpp |
| P2 | TaskManager | Per-robot task tracking | task_manager_node.cpp |
| P2 | CommandNode | Per-robot command targeting | command_node.cpp |
| P2 | Frontend | Robot status grid panel | StatusPanel.tsx |
| P3 | Standalone training | Multi-robot version | standalone_env.py |
| P3 | Launch files | Multi-robot composition | Various .launch.py |

### 11.3 Message/Service Changes

```yaml
# GetObservation.srv - Add robot_id
int32 robot_id 0
---
warehouser_msgs/Observation observation

# LidarDebug.msg - Already has robot pose, no change needed

# New: BatchObservation.msg (optional)
int32[] robot_ids
warehouser_msgs/Observation[] observations

# TaskStatus.msg - Add robot_id
string robot_id        # NEW: which robot this status is for
string task_id
string state
string intent
string target_color
float32 distance_to_goal
```

---

## 12. Implementation Recommendations

### 12.1 Phase 1: Topic Namespacing (Week 1)

1. Modify `simulation_node.cpp` to subscribe to `/robot{N}/cmd_vel`
2. Modify `observations_node.cpp` to publish to `/robot{N}/observations`
3. Update `GetObservation.srv` with robot_id
4. Test with RLBridge (already publishes to per-robot topics)

### 12.2 Phase 2: Observations & Safety (Week 2)

1. Full per-robot observations publishing
2. Per-robot safety controllers
3. TF frame namespacing
4. Frontend multi-robot lidar

### 12.3 Phase 3: Inference & Tasks (Week 3)

1. Multi-robot inference options
2. Per-robot task management
3. Frontend task status for all robots
4. Standalone multi-robot training env

### 12.4 Testing Strategy

1. Unit tests for multi-robot WorldManager (existing: test_world_manager.cpp)
2. Unit tests for robot-robot collision (existing: test_robot_collision.cpp)
3. Integration test: N robots, independent goals
4. Integration test: N robots, shared reward
5. Frontend E2E test: Robot selection and visualization

---

## 13. Files Referenced

### ROS C++ (ros_ws/src/)

| Package | File | Key Functions |
|---------|------|---------------|
| warehouser_simulation | simulation_node.cpp | cmdVelCallback, pickCallback |
| warehouser_simulation | world_manager.cpp | step, checkRobotCollision |
| warehouser_simulation | world_manager.hpp | RobotSpawnConfig, robotCount |
| warehouser_simulation | robot.hpp | Robot class, kRadius |
| warehouser_observations | observations_node.cpp | findRobot, publishLidarDebug |
| warehouser_observations | observation_builder.cpp | buildV3, findRobotByIndex |
| warehouser_observations | lidar_simulator.cpp | scan, buildDebugMsg |
| warehouser_rl_bridge | rl_bridge_node.cpp | initializeRobotPublishers, sendAction |
| warehouser_rl_bridge | reward_calculator.cpp | calculate(robot_index) |
| warehouser_inference | inference_node.cpp | inferenceLoop |
| warehouser_safety | safety_node.cpp | cmdRawCallback |
| warehouser_task | task_manager_node.cpp | worldStateCallback |
| warehouser_command | command_node.cpp | executeCommand |

### Message/Service Definitions

| File | Status |
|------|--------|
| Entity.msg | OK - has in_robot_collision |
| WorldState.msg | OK - array of entities |
| LidarDebug.msg | OK - has robot pose |
| Observation.msg | OK - generic data array |
| RLStep.srv | OK - has robot_id |
| RLReset.srv | OK - has robot_count |
| SimReset.srv | OK - has robot_count |
| GetObservation.srv | NEEDS robot_id |
| SetGoal.srv | NEEDS robot_id |

### Python Training (training/training/)

| File | Status |
|------|--------|
| envs/ros_env.py | NEEDS robot_id support |
| envs/pettingzoo_env.py | OK - multi-agent ready |
| envs/standalone_env.py | NEEDS multi-robot version |
| models/config.py | OK - has MultiAgentConfig |

### TypeScript Frontend (web_frontend/src/)

| File | Status |
|------|--------|
| store/appStore.ts | OK - has selectedRobotId |
| components/canvas/CanvasRobots.tsx | OK - renders multiple |
| components/canvas/CanvasLidar.tsx | NEEDS multi-robot |
| components/RosDataBridge.tsx | NEEDS per-robot subscriptions |
| ros/subscriptions.ts | NEEDS per-robot topics |

---

## 14. Conclusion

The codebase has a solid foundation for multi-robot support, with WorldManager, RLBridge, and ObservationBuilder already handling multiple robots. The primary gaps are:

1. **Topic namespacing** - Most nodes still use global topics
2. **Observations node** - Only tracks first robot
3. **Frontend lidar** - Only displays one scan
4. **TF frames** - Not namespaced per robot

With the changes outlined in this document, full multi-robot support can be achieved in approximately 3 weeks of focused development.
