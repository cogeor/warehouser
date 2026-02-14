# TASK: Implement Multi-Robot Task Allocation System

Created: 2026-02-12T23:52:00Z
Build: NOT AVAILABLE (colcon not found in environment)
Tests: NOT AVAILABLE (dependencies not installed)

## Summary

Implement a multi-robot task allocation system for Warehouser that assigns tasks to robots based on cost optimization. The current implementation only supports a single robot executing one task at a time, with no queue or allocation mechanism. This task adds a complete task allocation architecture starting with an optimal Hungarian algorithm baseline, progressing to auction-based dynamic allocation, and ultimately supporting RL-based learned allocation policies.

## Context

### From Search [S]

Research identified four primary approaches to Multi-Robot Task Allocation (MRTA):

1. Hungarian Algorithm (Optimal Centralized)
   - Provides optimal solutions to assignment problems in O(n³) time
   - Best for small-to-medium fleets (up to 10-20 robots)
   - Benchmark standard for MRTA problems
   - Available in scipy.optimize.linear_sum_assignment

2. Auction-Based Allocation (Distributed)
   - Robots bid on tasks based on local cost estimates
   - Sub-optimal but scalable to large fleets (20+ robots)
   - Handles dynamic task arrival and robot failures well
   - No central coordinator required (can be peer-to-peer)

3. Optimization Techniques
   - Genetic algorithms, particle swarm optimization
   - Near-optimal performance with medium complexity
   - Suitable for multi-objective optimization

4. Reinforcement Learning-Based
   - Learns allocation policies from experience
   - Handles complex, dynamic warehouse environments (RMFS)
   - Can optimize multiple objectives simultaneously
   - Fast inference (O(1)) after training

Key performance comparison:

| Algorithm | Optimality | Speed | Scalability | Distributed | Dynamic |
|-----------|-----------|-------|-------------|-------------|---------|
| Hungarian | Optimal | O(n³) | Small fleets | No | Poor |
| Auction | Sub-optimal | Fast | Large fleets | Yes | Good |
| RL-based | Learned | Fast (inference) | Large fleets | Yes | Excellent |

### From Introspection [I]

Current Warehouser architecture has the following gaps:

1. Single Robot Design
   - TaskManagerNode tracks only one robot state (lines 43-45 of task_manager_node.hpp)
   - Task struct has no robot assignment field
   - No task queue — new goals immediately replace current task
   - Goal.msg has no robot_id field — it's a global goal

2. Multi-Robot Infrastructure Exists But Incomplete
   - RLBridgeNode supports per-robot step/reset with robot_id parameter
   - WorldManager tracks N robots in vector
   - PettingZoo env trains multiple agents BUT all agents receive same goal
   - Critical TODO at rl_bridge_node.cpp:194: "For multi-robot, need per-robot cmd_vel topics"

3. No Allocation Mechanism
   - All robots compete for same task
   - No cost-based assignment logic
   - No task queue or priority handling
   - No re-allocation on failure

4. Missing Features
   - Task priority field
   - Robot availability tracking
   - Task dependencies
   - Deadline handling
   - Re-allocation triggers (failure, timeout, battery critical)

### From Templates [T]

Reference implementations provide:

1. Hungarian Algorithm Pattern (Python)
   - Cost matrix builder with configurable weights
   - Factors: distance, battery penalty, load imbalance, priority urgency
   - Handles rectangular matrices (unequal robots/tasks)
   - Integration via scipy.optimize.linear_sum_assignment

2. Auction-Based Pattern (ROS2)
   - Centralized auctioneer node collecting bids
   - Distributed bidding from robot nodes
   - Peer-to-peer consensus variant for no central coordinator
   - Re-auction mechanism for failed tasks

3. Priority Queue Pattern (Python)
   - Priority levels: CRITICAL, HIGH, NORMAL, LOW
   - Aging mechanism to prevent starvation
   - Multi-level queues for different task types
   - Re-queue failed tasks with priority boost

4. RL-Based Allocation Pattern (PettingZoo Extension)
   - Centralized allocator agent + decentralized robot agents
   - Allocator observes global state, outputs task assignments
   - Shared reward: allocator gets credit for fleet performance
   - Multi-objective reward shaping (load balance, coverage, distance efficiency)

## Objective

Implement a phased multi-robot task allocation system that enables Warehouser to efficiently assign tasks to multiple robots, starting with an optimal baseline (Hungarian algorithm) and progressing to dynamic allocation methods (auction, RL-based).

Success Criteria:
- Multiple robots can execute different tasks simultaneously
- Tasks are queued and assigned based on cost optimization
- System handles robot failures and task timeouts with re-allocation
- Performance metrics tracked: total distance, makespan, load balance

## Scope

### Phase 1: Basic Multi-Robot Infrastructure (Foundation)

**Goal:** Enable per-robot task assignment with task queue

**Files to Modify:**
- `ros_ws/src/warehouser_task/include/warehouser_task/task_state_machine.hpp`
  - Add `int robot_id` field to Task struct
  - Add `float priority` field
  - Add `std::chrono::time_point deadline` field

- `ros_ws/src/warehouser_task/include/warehouser_task/task_manager_node.hpp`
  - Replace single `robot_x_, robot_y_, robot_is_carrying_` with `std::vector<RobotState> robot_states_`
  - Add `std::deque<Task> pending_tasks_` queue
  - Add `std::map<int, Task> robot_tasks_` (robot_id → assigned task)
  - Add `assignTaskToRobot(int robot_id, Task task)` method

- `ros_ws/src/warehouser_task/src/task_manager_node.cpp`
  - Modify `goalCallback` to add tasks to queue instead of immediate execution
  - Implement simple greedy allocator: assign next task to nearest idle robot
  - Track per-robot state updates from WorldState messages

- `ros_ws/src/warehouser_msgs/msg/Goal.msg`
  - Add `int32 robot_id` field
  - Add `float32 priority 0.5` default

- `ros_ws/src/warehouser_msgs/msg/TaskStatus.msg`
  - Add `int32 robot_id` field

- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp`
  - Fix TODO at line 194: implement per-robot cmd_vel topics
  - Change from `/cmd_vel` to `/robot_N/cmd_vel` pattern
  - Add robot namespace parameter

**Files to Create:**
- `ros_ws/src/warehouser_task/include/warehouser_task/robot_state.hpp`
  - Define RobotState struct with id, x, y, battery, is_carrying, current_load

### Phase 2: Hungarian Algorithm (Optimal Baseline)

**Goal:** Implement cost-optimal task allocation using Hungarian algorithm

**Files to Create:**
- `training/training/allocator/__init__.py`
  - Package initialization

- `training/training/allocator/hungarian.py`
  - Implement HungarianTaskAllocator class (from T.md lines 33-199)
  - Cost function with distance, battery, load, priority weights
  - Integration with scipy.optimize.linear_sum_assignment

- `training/training/allocator/base.py`
  - Define Robot and Task dataclasses
  - Define base TaskAllocator interface

- `ros_ws/src/warehouser_msgs/srv/AllocateTasks.srv`
  - Request: RobotState[] robots, TaskRequest[] tasks
  - Response: Assignment[] assignments (robot_id, task_id pairs), bool success

- `ros_ws/src/warehouser_task/scripts/allocation_service.py`
  - ROS2 node wrapping HungarianTaskAllocator (from T.md lines 1256-1320)
  - Service handler for AllocateTasks.srv

**Files to Modify:**
- `training/pyproject.toml`
  - Add scipy dependency

- `ros_ws/src/warehouser_task/src/task_manager_node.cpp`
  - Add periodic allocation timer (every 5-10 seconds)
  - Call allocation service with current robot states and pending tasks
  - Apply returned assignments

### Phase 3: Auction-Based Allocation (Dynamic)

**Goal:** Add distributed auction mechanism for dynamic re-allocation

**Files to Create:**
- `ros_ws/src/warehouser_msgs/srv/TaskBid.srv`
  - Request: int32 robot_id, int32 task_id, float32 bid_cost
  - Response: bool accepted, string message

- `ros_ws/src/warehouser_msgs/msg/TaskAward.msg`
  - int32 task_id, int32 winner_robot_id, float32 winning_cost

- `ros_ws/src/warehouser_msgs/msg/TaskDescription.msg`
  - int32 task_id, float32 pickup_x, float32 pickup_y, float32 dropoff_x, float32 dropoff_y, float32 priority

- `training/training/allocator/auction.py`
  - Implement AuctionTaskAllocator class (from T.md lines 301-527)
  - Centralized auctioneer with bid collection
  - Timeout-based auction resolution
  - Re-auction mechanism for failed tasks

- `ros_ws/src/warehouser_task/src/auction_allocator_node.cpp`
  - C++ implementation of auction node
  - Task announcement publisher
  - Bid service handler
  - Award publisher

**Files to Modify:**
- `ros_ws/src/warehouser_task/src/task_manager_node.cpp`
  - Add re-allocation triggers: robot failure, task timeout, battery critical
  - Switch between Hungarian (periodic) and auction (dynamic) modes

### Phase 4: RL-Based Allocation (Learning)

**Goal:** Train and deploy learned allocation policy

**Files to Create:**
- `training/training/envs/allocation_env.py`
  - Extend PettingZoo with allocator agent (from T.md lines 856-1079)
  - Allocator observes global state (robot positions, battery, task queue)
  - Allocator action: assignment matrix (which robot gets which task)
  - Reward shaping: load balance, coverage, distance efficiency

- `training/training/scripts/train_allocation.py`
  - Training script for allocation policy using PPO (from T.md lines 1084-1125)
  - Centralized critic, decentralized actors
  - Save trained policy to ONNX

- `ros_ws/src/warehouser_inference/src/allocation_inference_node.cpp`
  - Load ONNX allocation policy
  - Inference service for allocation decisions
  - Similar pattern to navigation inference node

**Files to Modify:**
- `training/training/envs/pettingzoo_env.py`
  - Add option for allocation mode (learned vs Hungarian)
  - Include allocated task_id in per-robot observations

## Implementation Plan

### Phase 1: Multi-Robot Foundation (Week 1)

**Step 1.1: Message Definitions**
- [ ] Add robot_id and priority fields to Goal.msg and TaskStatus.msg
- [ ] Define RobotState struct in C++ header
- [ ] Update message generation (colcon build warehouser_msgs)

**Step 1.2: Task Manager Refactoring**
- [ ] Replace single robot tracking with vector of robot states
- [ ] Add task queue (std::deque<Task>)
- [ ] Add robot assignments map (std::map<int, Task>)
- [ ] Implement assignTaskToRobot() method

**Step 1.3: Simple Greedy Allocator**
- [ ] On new task arrival: find nearest idle robot
- [ ] Cost function: Euclidean distance only
- [ ] Assign task, publish Goal with robot_id

**Step 1.4: Per-Robot Topics**
- [ ] Fix RL bridge TODO: implement /robot_N/cmd_vel topics
- [ ] Add robot namespace parameter to nodes
- [ ] Test with 2 robots in simulation

**Verification:**
- [ ] Two robots can execute different tasks simultaneously
- [ ] Tasks queued when all robots busy
- [ ] Next task assigned when robot completes current task

### Phase 2: Hungarian Algorithm (Week 2)

**Step 2.1: Python Allocator**
- [ ] Create training/training/allocator/ package
- [ ] Implement HungarianTaskAllocator class
- [ ] Add scipy dependency to pyproject.toml
- [ ] Unit tests for cost matrix and allocation logic

**Step 2.2: ROS2 Service Interface**
- [ ] Define AllocateTasks.srv message
- [ ] Create allocation_service.py ROS2 node
- [ ] Test service independently with mock data

**Step 2.3: Integration with Task Manager**
- [ ] Add allocation service client to TaskManagerNode
- [ ] Create periodic timer (every 5 seconds)
- [ ] On timer: collect robot states and pending tasks, call service
- [ ] Apply returned assignments

**Step 2.4: Cost Function Tuning**
- [ ] Experiment with weight parameters (distance, battery, load, priority)
- [ ] Benchmark against greedy baseline
- [ ] Measure metrics: total distance, makespan, load balance

**Verification:**
- [ ] Allocation minimizes total cost across fleet
- [ ] High-priority tasks assigned preferentially
- [ ] Low-battery robots receive fewer tasks
- [ ] Load balanced across robots

### Phase 3: Auction-Based Allocation (Week 3)

**Step 3.1: Auction Message Definitions**
- [ ] Define TaskBid.srv, TaskAward.msg, TaskDescription.msg
- [ ] Generate messages (colcon build warehouser_msgs)

**Step 3.2: Auction Allocator Node**
- [ ] Implement AuctionTaskAllocator in Python
- [ ] Create centralized auctioneer node
- [ ] Task announcement publisher on /auction/new_task
- [ ] Bid service on /auction/submit_bid
- [ ] Award publisher on /auction/task_awarded
- [ ] Timeout-based resolution (2 second window)

**Step 3.3: Robot Bidding Capability**
- [ ] Add bidding logic to TaskManagerNode per robot
- [ ] Subscribe to /auction/new_task
- [ ] Compute local cost estimate (distance + battery + load)
- [ ] Submit bid via service call
- [ ] Listen for award, start task if won

**Step 3.4: Re-allocation Triggers**
- [ ] Detect robot failure (heartbeat timeout)
- [ ] Detect task timeout (robot stuck for > 30 seconds)
- [ ] Re-auction failed tasks with priority boost
- [ ] Test with simulated robot failures

**Verification:**
- [ ] Tasks auctioned and awarded within timeout window
- [ ] Lowest bidder wins task
- [ ] Failed tasks re-auctioned successfully
- [ ] No central coordinator bottleneck (distributed)

### Phase 4: RL-Based Allocation (Week 4+)

**Step 4.1: Allocator Agent Environment**
- [ ] Create WarehouseAllocationEnv in allocation_env.py
- [ ] Add "allocator" agent to possible_agents
- [ ] Define allocator observation space (global state)
- [ ] Define allocator action space (assignment matrix)
- [ ] Implement allocation reward function (load balance, coverage, efficiency)

**Step 4.2: Training**
- [ ] Write train_allocation.py script
- [ ] Train PPO policy for allocator agent
- [ ] Train for 1M timesteps with varied task arrival patterns
- [ ] Export trained policy to ONNX

**Step 4.3: Inference Node**
- [ ] Create allocation_inference_node.cpp
- [ ] Load ONNX policy
- [ ] Inference service similar to navigation inference
- [ ] Integrate with TaskManagerNode

**Step 4.4: Comparison and Tuning**
- [ ] Benchmark RL policy vs Hungarian vs auction
- [ ] Measure performance on dynamic scenarios (failures, urgent tasks)
- [ ] Fine-tune reward weights

**Verification:**
- [ ] RL policy learns efficient allocation
- [ ] Performance competitive with Hungarian on static scenarios
- [ ] Superior performance on dynamic scenarios (task arrival, failures)
- [ ] Fast inference (< 10ms per allocation decision)

## Interface Definitions

### New ROS2 Messages

#### AllocateTasks.srv
```
# Request: Task allocation request
RobotState[] robots
TaskRequest[] tasks
---
# Response: Optimal assignments
Assignment[] assignments
bool success
string info
```

#### TaskBid.srv
```
# Request: Submit bid on task
int32 robot_id
int32 task_id
float32 bid_cost
---
# Response: Bid acceptance
bool accepted
string message
```

#### TaskAward.msg
```
int32 task_id
int32 winner_robot_id
float32 winning_cost
```

#### TaskDescription.msg
```
int32 task_id
float32 pickup_x
float32 pickup_y
float32 dropoff_x
float32 dropoff_y
float32 priority
```

### Modified Messages

#### Goal.msg (add fields)
```
float32 x
float32 y
string target_color
bool active
int32 robot_id        # NEW: Target robot for this goal
float32 priority 0.5  # NEW: Task priority (0.0 to 1.0)
```

#### TaskStatus.msg (add field)
```
# ... existing fields ...
int32 robot_id  # NEW: Robot executing this task
```

### Data Structures

#### RobotState (C++ struct)
```cpp
struct RobotState {
    int id;
    float x;
    float y;
    float battery;
    bool is_carrying;
    int current_load;
};
```

#### Task (Python dataclass)
```python
@dataclass
class Task:
    id: int
    pickup_x: float
    pickup_y: float
    dropoff_x: float
    dropoff_y: float
    priority: float  # 0.0 to 1.0
```

## Architecture Notes

### Allocation Modes

The system supports three allocation modes:

1. **Greedy Mode (Phase 1)**
   - Assign next task to nearest idle robot
   - O(n) complexity, minimal overhead
   - Use for: Single task arrivals, simple scenarios

2. **Hungarian Mode (Phase 2)**
   - Periodic re-optimization every 5-10 seconds
   - O(n³) complexity, optimal assignment
   - Use for: Static or semi-static task sets, small fleets (< 20 robots)

3. **Auction Mode (Phase 3)**
   - Dynamic allocation on task arrival
   - Distributed bidding, 2-second auction window
   - Use for: Dynamic task arrival, robot failures, large fleets (20+ robots)

4. **RL Mode (Phase 4)**
   - Learned policy from trained allocator agent
   - O(1) inference, handles complex objectives
   - Use for: Complex warehouse operations, multi-objective optimization

### Cost Function Design

Allocation cost computed as weighted sum:

```
cost = w1 * distance + w2 * battery_penalty + w3 * load_imbalance + w4 * priority_urgency
```

**Recommended Weights:**
- w1 (distance): 1.0
- w2 (battery): 0.5 (exponential penalty for low battery)
- w3 (load): 0.3 (prefer balanced distribution)
- w4 (priority): 2.0 (high-priority tasks favored)

### Re-allocation Triggers

The system re-allocates tasks when:

1. **Robot Failure:** Heartbeat timeout (> 5 seconds)
2. **Task Timeout:** Robot stuck or no progress (> 30 seconds)
3. **Battery Critical:** Robot battery < 20%, reassign remaining tasks
4. **New High-Priority Task:** Preempt low-priority tasks if needed
5. **Periodic Re-optimization:** Every 5-10 seconds in Hungarian mode

### Topic Structure

Per-robot topics follow namespace pattern:

```
/robot_0/cmd_vel          # Velocity commands
/robot_0/task/goal        # Goal assignment
/robot_0/task/status      # Task status
/robot_1/cmd_vel
/robot_1/task/goal
/robot_1/task/status
...
```

Global topics:

```
/task/goal_input          # New task submissions (no robot_id yet)
/auction/new_task         # Task auction announcement
/auction/task_awarded     # Auction results
```

## Files Summary

### Files to Create (20 files)

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_task/include/warehouser_task/robot_state.hpp` | RobotState struct definition |
| `ros_ws/src/warehouser_msgs/srv/AllocateTasks.srv` | Allocation request/response service |
| `ros_ws/src/warehouser_msgs/srv/TaskBid.srv` | Bid submission service |
| `ros_ws/src/warehouser_msgs/msg/TaskAward.msg` | Auction award message |
| `ros_ws/src/warehouser_msgs/msg/TaskDescription.msg` | Task announcement message |
| `training/training/allocator/__init__.py` | Allocator package init |
| `training/training/allocator/base.py` | Base allocator interface and data classes |
| `training/training/allocator/hungarian.py` | Hungarian algorithm implementation |
| `training/training/allocator/auction.py` | Auction algorithm implementation |
| `training/training/allocator/greedy.py` | Simple greedy allocator |
| `training/training/envs/allocation_env.py` | PettingZoo env with allocator agent |
| `training/training/scripts/train_allocation.py` | Training script for allocation policy |
| `training/tests/test_allocator.py` | Unit tests for allocators |
| `ros_ws/src/warehouser_task/scripts/allocation_service.py` | ROS2 allocation service node (Python) |
| `ros_ws/src/warehouser_task/src/auction_allocator_node.cpp` | Auction node (C++) |
| `ros_ws/src/warehouser_task/include/warehouser_task/auction_allocator_node.hpp` | Auction node header |
| `ros_ws/src/warehouser_inference/src/allocation_inference_node.cpp` | RL inference for allocation |
| `ros_ws/src/warehouser_inference/include/warehouser_inference/allocation_inference_node.hpp` | Allocation inference header |
| `.arch/allocation/README.md` | Architecture documentation |
| `.arch/allocation/diagrams/allocation_flow.md` | Flow diagrams for allocation modes |

### Files to Modify (13 files)

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_task/include/warehouser_task/task_state_machine.hpp` | Add robot_id, priority, deadline to Task struct |
| `ros_ws/src/warehouser_task/src/task_state_machine.cpp` | Handle new Task fields |
| `ros_ws/src/warehouser_task/include/warehouser_task/task_manager_node.hpp` | Replace single robot tracking with vector; add task queue; add assignments map |
| `ros_ws/src/warehouser_task/src/task_manager_node.cpp` | Implement task queuing, assignment logic, allocation service client, re-allocation triggers |
| `ros_ws/src/warehouser_msgs/msg/Goal.msg` | Add robot_id and priority fields |
| `ros_ws/src/warehouser_msgs/msg/TaskStatus.msg` | Add robot_id field |
| `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp` | Fix TODO line 194: per-robot cmd_vel topics |
| `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp` | Add robot namespace parameter |
| `training/pyproject.toml` | Add scipy dependency |
| `training/training/envs/pettingzoo_env.py` | Support allocated task_id in observations; mode selection |
| `training/training/models/config.py` | Add AllocationConfig for allocator parameters |
| `web_frontend/src/store/appStore.ts` | Add per-robot task tracking, task queue state |
| `web_frontend/src/components/StatusPanel.tsx` | Display task queue, per-robot assignments |

## Testing Strategy

### Unit Tests

1. **Allocator Logic (Python)**
   - Test cost matrix generation
   - Test Hungarian assignment correctness
   - Test auction bid collection and winner selection
   - Test priority queue ordering and aging

2. **ROS2 Services (Integration)**
   - Mock robot states and tasks
   - Call allocation service, verify assignments
   - Test bid service with multiple bidders
   - Test award message reception

### Integration Tests

1. **Two-Robot Scenario**
   - Spawn 2 robots in simulation
   - Submit 4 tasks to queue
   - Verify tasks assigned and executed in parallel
   - Check no task conflicts

2. **Re-allocation on Failure**
   - Assign task to robot
   - Simulate robot failure (kill node)
   - Verify task re-assigned to another robot
   - Check task completion

3. **Dynamic Task Arrival**
   - Submit tasks one at a time during execution
   - Verify greedy/auction allocation
   - Measure latency from submission to assignment

### Performance Benchmarks

**Metrics:**
- Total distance traveled by fleet
- Makespan (time to complete all tasks)
- Load balance variance across robots
- Allocation latency (time to assign task)
- Success rate (% tasks completed without re-allocation)

**Scenarios:**
1. Static: 10 tasks, 5 robots, no failures
2. Dynamic: Tasks arrive at 1 task/10 seconds, 5 robots
3. Failures: 20 tasks, 10 robots, 2 robots fail mid-execution
4. Urgent: Mix of normal and high-priority tasks

**Compare:**
- Greedy vs Hungarian vs Auction vs RL
- Small fleet (2-5 robots) vs large fleet (10-20 robots)

## Verification Checklist

### Phase 1: Multi-Robot Foundation
- [ ] Multiple robots tracked independently in TaskManagerNode
- [ ] Task queue accepts and stores pending tasks
- [ ] Greedy allocator assigns tasks to nearest robot
- [ ] Per-robot topics work (/robot_N/cmd_vel)
- [ ] Two robots execute different tasks simultaneously
- [ ] Tests pass: Two-robot scenario

### Phase 2: Hungarian Algorithm
- [ ] HungarianTaskAllocator computes optimal assignments
- [ ] Cost function considers distance, battery, load, priority
- [ ] ROS2 allocation service responds correctly
- [ ] Periodic allocation timer triggers re-optimization
- [ ] Allocation improves total distance vs greedy baseline
- [ ] Tests pass: Allocator unit tests, integration tests

### Phase 3: Auction-Based Allocation
- [ ] Auction node announces tasks on /auction/new_task
- [ ] Robots submit bids via /auction/submit_bid
- [ ] Auctioneer awards task to lowest bidder
- [ ] Re-auction triggered on robot failure
- [ ] Re-auction triggered on task timeout
- [ ] Tests pass: Dynamic task arrival, failure scenarios

### Phase 4: RL-Based Allocation
- [ ] Allocator agent added to PettingZoo environment
- [ ] Training converges (allocation reward increases)
- [ ] Policy exported to ONNX successfully
- [ ] Inference node loads and runs policy
- [ ] RL policy performance competitive with Hungarian
- [ ] Tests pass: Performance benchmarks

## Next Steps

**Immediate (This Week):**
1. Implement Phase 1 message definitions (Goal.msg, TaskStatus.msg)
2. Refactor TaskManagerNode to track multiple robots
3. Add task queue (std::deque<Task>)
4. Implement simple greedy allocator

**Short-Term (Next 2 Weeks):**
5. Create HungarianTaskAllocator in Python
6. Add ROS2 allocation service
7. Integrate with TaskManagerNode periodic timer
8. Benchmark Hungarian vs greedy

**Medium-Term (Next Month):**
9. Implement auction-based allocator
10. Add re-allocation triggers
11. Test with dynamic scenarios

**Long-Term (Future):**
12. Train RL allocation policy
13. Deploy to inference node
14. Optimize for production use

## References

### Academic Sources
- ACM Systematic Literature Review on MRTA (2025): https://dl.acm.org/doi/10.1145/3700591
- Distributed Hungarian Method: https://link.springer.com/chapter/10.1007/978-3-642-13022-9_72
- Warehouse AGV Allocation: https://publications.eai.eu/index.php/airo/article/view/9913
- Deep RL for RMFS: https://www.aimspress.com/article/doi/10.3934/mbe.2023087

### Implementation References
- scipy.optimize.linear_sum_assignment: https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.linear_sum_assignment.html
- Python Priority Queue: https://docs.python.org/3/library/queue.html
- PettingZoo Multi-Agent: https://pettingzoo.farama.org/

### Internal References
- Study findings: `.delegate/study/20260212-234213-task-allocation-algorithms/S.md`
- Introspection analysis: `.delegate/study/20260212-234213-task-allocation-algorithms/I.md`
- Template implementations: `.delegate/study/20260212-234213-task-allocation-algorithms/T.md`
- Current task manager: `ros_ws/src/warehouser_task/`
- Multi-robot training: `training/training/envs/pettingzoo_env.py`
