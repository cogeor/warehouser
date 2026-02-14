# Introspect: Task Allocation Implementation Analysis

Created: 2026-02-12T23:45:00Z

## Focus

Analysis of the current task management and allocation architecture in Warehouser, examining how tasks are created, assigned, and executed in single-robot and multi-robot scenarios.

## Current Architecture Overview

Warehouser implements a **single-robot task management system** with recent multi-robot infrastructure additions but **no task allocation mechanism**.

### Component Relationships

```
Command Node → Task Manager Node → RL Bridge Node → Simulation
     ↓              ↓                    ↓                ↓
  Parse JSON   State Machine      Per-Robot Step    N Robots
  commands     Single Task         Per-Robot Obs    in World
```

## 1. Task Management Architecture

### 1.1 Task Manager Node

**Location:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\`

**Files:**
- `include/warehouser_task/task_manager_node.hpp` (lines 1-68)
- `src/task_manager_node.cpp` (lines 1-219)
- `include/warehouser_task/task_state_machine.hpp` (lines 1-88)
- `src/task_state_machine.cpp` (lines 1-178)

**Key Characteristics:**
- **Single active task only** — `state_machine_` manages one task at a time
- **No robot assignment logic** — assumes single robot
- **No task queue** — new goal immediately replaces current task
- **Single robot state tracking** — `robot_x_`, `robot_y_`, `robot_is_carrying_` (lines 43-45 of task_manager_node.hpp)

### 1.2 Task State Machine

**States (task_state_machine.hpp:9-18):**
```cpp
enum class TaskState {
    IDLE,
    NAVIGATING_TO_PICK,
    PICKING,
    NAVIGATING_TO_PLACE,
    PLACING,
    COMPLETED,
    FAILED,
    CANCELLED
};
```

**Events (task_state_machine.hpp:20-31):**
- COMMAND_RECEIVED, REACHED_OBJECT, PICK_SUCCESS/FAILED
- REACHED_DESTINATION, PLACE_SUCCESS/FAILED
- TIMEOUT, CANCEL_REQUESTED, COLLISION

**Task Data Structure (task_state_machine.hpp:33-51):**
```cpp
struct Task {
    std::string task_id;
    std::string intent;  // "pick", "navigate", "pick_and_place"
    std::string target_object_id;
    std::string target_color;
    float object_x, object_y;
    float pickup_radius;
    float dest_x, dest_y;
    float place_radius;
    std::string failure_reason;
};
```

**Key Observation:** Task is simple — no robot assignment, no priority, no deadline, no dependencies.

### 1.3 Task Creation

**Source:** `task_manager_node.cpp:85-115` (goalCallback)

**Flow:**
1. Goal message arrives on `/task/goal_input`
2. Task created with timestamp ID (line 88)
3. Intent determined from goal content (lines 95-103)
4. Current goal coordinates set (lines 106-107)
5. State machine started with COMMAND_RECEIVED event (line 111)

**Critical Gap:** No concept of multiple pending tasks or task queue.

### 1.4 Task Execution

**Proximity-Based State Transitions (task_manager_node.cpp:67-82):**
- Checks `distanceToGoal()` against `pickup_radius_` or `place_radius_`
- Transitions NAVIGATING_TO_PICK → PICKING when within radius
- Transitions NAVIGATING_TO_PLACE → PLACING when within radius

**Action Triggers (task_manager_node.cpp:117-131):**
- Action messages from `/inference/action` trigger pick/place attempts
- Checks robot carrying state to verify success
- No direct robot control — relies on inference node for navigation

## 2. Multi-Robot Infrastructure

### 2.1 RL Bridge Node (Multi-Robot Support Added)

**Location:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\`

**Files:**
- `include/warehouser_rl_bridge/rl_bridge_node.hpp` (lines 1-90)
- `src/rl_bridge_node.cpp` (lines 1-300+)

**Multi-Robot Configuration (rl_bridge_node.hpp:51-56):**
```cpp
size_t robot_count_ = 1;
std::vector<warehouser_msgs::msg::WorldState> prev_world_states_;
std::vector<RewardCalculator> reward_calculators_;
```

**Per-Robot Step Service (rl_bridge_node.cpp:86-130):**
- RLStep.srv takes `robot_id` parameter (line 90)
- Validates robot_id against robot_count (lines 90-100)
- Stores per-robot previous state (line 103)
- Sends action to specific robot (line 106)
- Calculates per-robot reward (lines 115-118)
- Returns per-robot observation (line 122)

**Reset Service with Robot Count (rl_bridge_node.cpp:132-189):**
- RLReset.srv takes `robot_count` parameter (line 141)
- Resizes per-robot state vectors (line 150)
- Creates reward calculators for each robot (lines 151-157)
- Returns per-robot observations array (lines 178-181)

**CRITICAL TODO (rl_bridge_node.cpp:194-195):**
```cpp
// TODO: For multi-robot, need per-robot cmd_vel topics or action message
// For now, use robot_id 0 for backward compatibility
```

**Observation:** Multi-robot support exists in RL bridge but action sending is robot_id 0 only!

### 2.2 World Manager (Multi-Robot Entity Tracking)

**Location:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\include\warehouser_simulation\world_manager.hpp`

**Multi-Robot Support (lines 17-83):**
```cpp
struct RobotSpawnConfig {
    std::string id = "robot";
    float x, y, theta;
};

struct WorldConfig {
    std::array<float, 3> robot_spawn;  // Legacy single robot
    std::vector<RobotSpawnConfig> robot_spawns;  // Multi-robot
};

// Entity access
Robot* robot(size_t index = 0);
size_t robotCount() const { return robots_.size(); }
size_t addRobot(const RobotSpawnConfig& config);
```

**Storage (line 134):**
```cpp
std::vector<std::unique_ptr<Robot>> robots_;
```

**Observation:** World can track N robots, but systems above don't leverage this fully.

### 2.3 PettingZoo Environment (MARL Training)

**Location:** `C:\Users\costa\src\warehouser\training\training\envs\pettingzoo_env.py`

**Architecture (lines 38-72):**
- `possible_agents`: List of agent IDs (e.g., "robot_0", "robot_1", ...)
- Per-agent observation spaces (lines 55-63)
- Per-agent action spaces (lines 64-72)

**Reset (lines 136-194):**
- Passes `robot_count` to RLReset service (line 168)
- Returns per-agent observations dict (lines 178-192)

**Step (lines 199-308):**
- Iterates through agents, calling RLStep per robot (lines 243-258)
- **Sequential stepping** — robots act one at a time (not parallel)
- Shared reward option (lines 286-289)
- Returns per-agent (obs, reward, terminated, truncated, info) dicts

**Key Issue:** Each robot steps independently with `robot_id`, but there's **no task assignment** — all robots receive the same global goal!

### 2.4 Message Definitions

**RLStep.srv (lines 1-18):**
```
# Request
int32 robot_id 0
float32 action_linear
float32 action_angular
float32 action_pick
float32 action_place
int32 num_steps 1
---
# Response
int32 robot_id
Observation observation
float32 reward
bool terminated
bool truncated
string info
```

**RLReset.srv (lines 1-14):**
```
# Request
int64 seed 0
int32 robot_count 1
string config
---
# Response
bool success
int32 robot_count
Observation[] observations
Observation observation  # Legacy
string info
```

**Goal.msg (lines 1-7):**
```
float32 x
float32 y
string target_color
bool active
```

**Observation:** Goal message has no robot_id field — it's a global goal!

## 3. Task Types and Parameters

### 3.1 Supported Task Intents

**From task_state_machine.cpp:92-101:**
1. **"navigate"** — Move to destination (dest_x, dest_y)
2. **"pick"** — Navigate to object, pick it up
3. **"pick_and_place"** — Navigate, pick, navigate to dest, place

### 3.2 Completion Criteria

**Success:**
- Navigate: Reach destination within `place_radius` (default 0.5m)
- Pick: Reach object within `pickup_radius`, robot.is_carrying = true
- Pick_and_place: Complete pick, reach dest, place (robot.is_carrying = false)

**Failure (task_state_machine.cpp:109-175):**
- Timeout reaching object/destination
- Collision during navigation
- Pick action failed (couldn't grasp)
- Place action failed (couldn't release)

**Observation:** No partial success, no retry logic, no task reassignment on failure.

## 4. Assignment Mechanism Analysis

### 4.1 Current "Assignment" (None)

**Command Node → Task Manager Flow:**
1. User sends JSON command to `/command/json`
2. CommandNode parses and resolves object/zone (command_node.cpp:71-109)
3. Publishes Goal to `/task/goal_input`
4. TaskManagerNode receives goal, creates task
5. **No robot selection occurs** — implicitly robot 0

**Frontend Goal Setting (appStore.ts:35-38):**
```typescript
// Task
taskState: string
taskIntent: string
setTaskStatus: (state: string, intent: string) => void
```

**Observation:** Frontend tracks single task state, no per-robot task display.

### 4.2 Multi-Robot Training Without Allocation

**PettingZoo step() behavior (pettingzoo_env.py:239-258):**
```python
for i, agent in enumerate(self.agents):
    request = RLStep.Request()
    request.robot_id = i
    request.action_linear = float(action[0])
    # ... send to RL bridge
```

**What happens:**
- All robots receive same goal from `/task/goal`
- Each robot independently tries to reach the goal
- Reward calculated per robot based on same goal
- No coordination — robots compete for same object!

**Critical Gap:** Multi-agent training exists but with **no task differentiation per agent**.

## 5. Dynamic Handling Gaps

### 5.1 Task Cancellation

**Implemented:** Cancel service exists (`/task/cancel`, task_manager_node.cpp:133-142)
- Sends CANCEL_REQUESTED event
- Transitions to CANCELLED state
- Disables inference

**Gap:** Only cancels current task, no queue to manage.

### 5.2 Re-allocation

**Not Implemented:**
- No mechanism to reassign failed tasks
- No monitoring of robot availability
- No rebalancing of task distribution
- No handling of robot failures or battery depletion

### 5.3 Priority Changes

**Not Implemented:**
- Tasks have no priority field
- No preemption mechanism
- No urgency handling
- FIFO implicit (but no queue exists)

## 6. Integration Points

### 6.1 RL Training Interaction

**Current:**
- RL bridge provides per-robot observations via `/observations/get` service
- Per-robot rewards calculated based on progress to goal
- All robots trained on same goal (no task diversity)

**Gap:** Training doesn't expose allocation decisions — robots learn navigation only, not task selection.

### 6.2 Frontend Display

**Current (StatusPanel.tsx:4-7, 13):**
```typescript
const taskState = useAppStore((s) => s.taskState)
const taskIntent = useAppStore((s) => s.taskIntent)
const robot = entities.find((e) => e.type === 'robot')  // First robot only
```

**Gap:**
- Single task state displayed
- Single robot position tracked
- No per-robot task assignment view
- No task queue visualization

### 6.3 ROS2 Interfaces

**Topics:**
- `/task/goal_input` — Goal message (no robot_id)
- `/task/goal` — Echoed goal (no robot_id)
- `/task/status` — TaskStatus message (no robot_id)
- `/cmd_vel` — Velocity command (no robot namespace)

**Services:**
- `/task/cancel` — Cancel current task (no robot_id)
- `/rl/step` — Per-robot step (has robot_id)
- `/rl/reset` — Multi-robot reset (has robot_count)

**Observation:** RL bridge has per-robot interfaces, but task system doesn't.

## Findings Summary

### Critical Gaps

1. **C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\src\task_manager_node.cpp:43-45**
   - Single robot state tracking only
   - No vector of robot states
   - No per-robot task assignments

2. **C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\include\warehouser_task\task_state_machine.hpp:33-51**
   - Task struct lacks robot assignment field
   - No priority, deadline, or dependencies
   - No task queue data structure

3. **C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\rl_bridge_node.cpp:194-195**
   - Multi-robot action sending not implemented
   - Only robot_id 0 receives cmd_vel commands
   - Per-robot topics/namespaces needed

4. **C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\Goal.msg:1-7**
   - No robot_id field in Goal message
   - All robots see same global goal
   - No per-robot goal assignment

5. **C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.ts:35-38**
   - Single task state in frontend
   - No task queue or per-robot assignment display
   - No allocation visualization

### Architecture Mismatches

1. **RL Bridge vs Task Manager:**
   - RL bridge supports N robots with per-robot step/obs
   - Task manager assumes single robot
   - No integration layer for allocation

2. **Training vs Deployment:**
   - PettingZoo env trains multiple agents
   - All agents compete for same goal (no diversity)
   - No learned allocation policy

3. **Simulation vs Control:**
   - World manager tracks N robots
   - Command/action interfaces don't support per-robot addressing
   - Topic structure needs namespacing

## Proposal: Task Allocation Architecture

### Phase 1: Basic Multi-Robot Task Assignment

**1. Add Task Queue to Task Manager**
- Replace single `active_task_` with `std::vector<Task> pending_tasks_`
- Add `std::map<size_t, Task> robot_tasks_` (robot_id → assigned task)
- Implement FIFO assignment: next available robot gets next task

**2. Extend Task Message with Robot Assignment**
- Add `int32 robot_id` to TaskStatus.msg
- Add `int32 assigned_robot_id` to Task struct
- Publish per-robot goal topics: `/robot_N/task/goal`

**3. Implement Simple Greedy Allocator**
- On new task arrival: find closest idle robot
- Assign task to robot, update state
- Cost function: Euclidean distance to task location

**4. Per-Robot Topic Namespacing**
- `/robot_0/cmd_vel`, `/robot_1/cmd_vel`, etc.
- RL bridge sends actions to correct robot topic
- Fix TODO at rl_bridge_node.cpp:194

### Phase 2: Hungarian Algorithm (Optimal Static Allocation)

**1. Implement Cost Matrix Builder**
- Cost[i][j] = cost for robot i to execute task j
- Factors: distance, battery level, current load
- Update on world state changes

**2. Periodic Re-allocation**
- Every 5-10 seconds, re-run Hungarian algorithm
- Use scipy.optimize.linear_sum_assignment (Python bridge)
- Or implement in C++ with eigen3

**3. Task Preemption Support**
- Add task priority field
- Allow preemption of low-priority tasks
- Re-allocate preempted tasks

### Phase 3: Market-Based Auction

**1. Distributed Auction Protocol**
- Robots bid on tasks based on local cost
- Auctioneer node awards tasks to lowest bidders
- Implement Contract Net Protocol

**2. Dynamic Re-allocation**
- Re-auction on robot failure
- Re-auction on task timeout
- Priority-based bidding modifiers

### Phase 4: RL-Based Allocation

**1. Extend Training Environment**
- Add allocation action space to PettingZoo env
- Centralized allocator agent or meta-controller
- Reward based on fleet-level metrics (makespan, balance)

**2. Multi-Objective Optimization**
- Minimize total distance traveled
- Minimize makespan (time to complete all tasks)
- Balance load across robots
- Reduce charging conflicts

## Implementation Priorities

1. **Immediate:** Per-robot topic namespacing (fix rl_bridge_node.cpp:194)
2. **Short-term:** Task queue and simple greedy allocation
3. **Medium-term:** Hungarian algorithm for static allocation
4. **Long-term:** Auction or RL-based dynamic allocation

## References

- Task Manager Architecture: `.arch/task_manager/README.md`
- Multi-Robot MRTA Research: `.delegate/study/20260212-234213-task-allocation-algorithms/S.md`
- Multi-Agent Training: `training/training/envs/pettingzoo_env.py`
