# TASK: Implement Fleet Management System with Industry Standards Support

Created: 2026-02-12T22:30:00Z
Build: SKIP (colcon not available in MSYS64 environment)
Tests: FAIL (pydantic module not found - dependency issue)

## Summary

Implement a comprehensive fleet management system for Warehouser that provides standardized interfaces for multi-robot coordination, task allocation, and monitoring. The system will support both Open-RMF (facility-level orchestration) and VDA5050 (vehicle-level protocol) standards while fixing critical bugs in the current multi-robot implementation.

## Context

### Research Findings (S.md)

Open-RMF provides centralized task queuing, conflict-free resource scheduling, and fleet adapter utilities. Key architectural components include:
- Fleet Adapters: Translation layer between proprietary robot APIs and standardized protocols
- Task Dispatcher: Coordinates competitive bidding among fleet adapters
- Traffic Schedule: Maintains conflict-free resource scheduling
- Task Planner: Solves optimal task allocation with battery-aware recharging

VDA5050 is the European standard for AGV/AMR interoperability using MQTT with JSON messages. Version 2.0/3.0 provides:
- Graph-based navigation (nodes + edges)
- Action state tracking (WAITING, PAUSED, FAILED, FINISHED)
- QoS differentiation for reliability
- Idle state definition for order acceptance

### Current State (I.md)

Warehouser has **foundational multi-robot infrastructure but critical gaps**:

**Strengths:**
- PettingZoo ParallelEnv for multi-agent RL (1-10 robots)
- V3_MultiRobot observations with per-robot state
- Multi-robot RLStep/RLReset service interfaces
- Exploration rewards with coverage tracking

**Critical Bugs (P0):**
1. rl_bridge_node.cpp:194 - Actions only route to robot_0 (other robots never receive commands)
2. rl_bridge_node.cpp:242 - Reset doesn't spawn N robots (robot_count parameter ignored)
3. GetObservation.srv - Missing robot_id parameter for per-robot queries

**Missing Fleet Features (P1):**
- No fleet state aggregation or RobotStatus messages
- No task assignment mechanism or queue management
- No robot namespacing (/robot_0/cmd_vel, /robot_1/cmd_vel)
- No battery tracking or charging state
- No robot mode states (IDLE, MOVING, CHARGING, PAUSED, ERROR)

**Standards Compliance (P2):**
- No Open-RMF FleetState, BidNotice, BidProposal messages
- No VDA5050 MQTT connector or protocol support
- No traffic coordination or conflict resolution

### Template Patterns (T.md)

Five comprehensive implementation patterns identified:

1. **Open-RMF Fleet Adapter**: ROS2 interface with FleetState publishing at 10 Hz, BidNotice/BidProposal handling, cost-based task allocation
2. **VDA5050 Protocol**: MQTT/JSON messages with order (graph navigation), state (vehicle status), instantActions (emergency), visualization (high-frequency position)
3. **Fleet State Aggregation**: Metrics calculation (utilization, battery, throughput), robot filtering, nearest-robot queries
4. **Task Allocation**: Cost-based bidding (distance + battery + time + priority), Hungarian algorithm for multi-task optimization
5. **Fleet Dashboard**: Real-time WebSocket streaming, React components, performance visualization

## Objective

Build a production-ready fleet management system that:
1. Fixes all critical multi-robot bugs to enable actual multi-robot operation
2. Implements standardized fleet interfaces (Open-RMF, VDA5050)
3. Provides task allocation with cost-based optimization
4. Enables real-time fleet monitoring and metrics
5. Maintains modularity and follows Warehouser architecture principles

## Target Architecture

```
┌─────────────────────────────────────────────────────────┐
│         External Fleet Manager (Open-RMF / VDA5050)      │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│           warehouser_fleet_manager (NEW)                 │
│  ┌───────────────────┐     ┌──────────────────────────┐ │
│  │ Open-RMF Adapter  │     │ VDA5050 Connector        │ │
│  │ - FleetState pub  │     │ - MQTT bridge            │ │
│  │ - BidNotice sub   │     │ - Order subscriber       │ │
│  │ - Task bidding    │     │ - State publisher        │ │
│  └───────────────────┘     └──────────────────────────┘ │
│  ┌───────────────────────────────────────────────────┐  │
│  │ Fleet State Aggregator                            │  │
│  │ - Metrics calculation                             │  │
│  │ - Robot status tracking                           │  │
│  │ - Task allocation optimization                    │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────┘
                      │ Per-robot namespaced topics
┌─────────────────────▼───────────────────────────────────┐
│         warehouser_rl_bridge (FIXED)                     │
│  - Per-robot cmd_vel routing                             │
│  - Multi-robot reset with spawn count                    │
│  - Robot-indexed observations                            │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│         warehouser_simulation (ENHANCED)                 │
│  - Multi-robot spawning                                  │
│  - Robot namespaced topic subscriptions                  │
└─────────────────────────────────────────────────────────┘
```

## Implementation Plan

### Phase 0: Critical Bug Fixes (1 week, P0)

**Goal:** Make multi-robot system actually work

- [ ] Fix action routing in rl_bridge_node.cpp
  - Implement per-robot cmd_vel topics: /robot_0/cmd_vel, /robot_1/cmd_vel, etc.
  - Route RLStep actions to correct robot based on robot_id parameter
  - Update simulation to subscribe to per-robot cmd_vel topics

- [ ] Fix multi-robot reset in rl_bridge_node.cpp
  - Pass robot_count to simulation reset service
  - Implement SimReset.srv with robot_count parameter
  - Update world_manager to spawn N robots on reset

- [ ] Add robot_id parameter to GetObservation.srv
  - Update service definition with robot_id field
  - Modify observation_builder to use robot_id parameter
  - Update rl_bridge to pass robot_id when calling service

- [ ] Add robot identification to Entity message
  - Add robot_id string field to Entity.msg
  - Update simulation to assign robot IDs on spawn
  - Ensure consistent robot indexing across system

**Verification:** Run PettingZoo env with 3+ robots, confirm all robots move independently

### Phase 1: Fleet State Infrastructure (2-3 weeks, P1)

**Goal:** Build foundation for fleet-level coordination

- [ ] Define core fleet messages in warehouser_msgs
  - RobotStatus.msg: robot_id, position, battery, mode, current_task, velocity
  - FleetState.msg: fleet_name, RobotStatus[] robots, timestamp
  - RobotMode.msg: IDLE=0, CHARGING=1, MOVING=2, PAUSED=3, EMERGENCY=5, PERFORMING_ACTION=10
  - FleetMetrics.msg: total_robots, active_robots, utilization, avg_battery, tasks_per_hour

- [ ] Create warehouser_fleet_manager package
  - Package structure: src/, include/, config/, launch/
  - C++ node with rclcpp
  - FleetStateAggregator class for metrics calculation

- [ ] Implement fleet state publisher
  - Subscribe to /world/state from simulation
  - Parse robot entities and aggregate status
  - Publish FleetState to /fleet/state at 10 Hz
  - Compute FleetMetrics and publish to /fleet/metrics at 1 Hz

- [ ] Add battery tracking to simulation
  - Add battery_percent field to Entity.msg
  - Implement battery drain model (distance-based)
  - Add charging zone entity type
  - Simulate battery recharge when in charging zone

- [ ] Implement robot mode state machine
  - Infer mode from robot state (velocity, task, battery, location)
  - IDLE: velocity=0, no task
  - MOVING: velocity>0
  - CHARGING: in charging zone, battery<100%
  - PERFORMING_ACTION: executing pick/place
  - PAUSED: velocity=0, has task

**Verification:** Visualize FleetState in RViz, verify metrics accuracy with 5 robots

### Phase 2: Task Management and Allocation (2-3 weeks, P1)

**Goal:** Enable intelligent task assignment across fleet

- [ ] Define task messages in warehouser_msgs
  - Task.msg: task_id, task_type (PICKUP, DELIVERY, CHARGE, PATROL), pickup_location, delivery_location, priority, deadline, payload_weight
  - TaskBid.msg: robot_id, task_id, cost, estimated_completion_time, confidence
  - TaskAssignment.msg: task_id, robot_id, assignment_time

- [ ] Define task allocation services
  - RequestTaskAllocation.srv: Task[] tasks → TaskAssignment[] assignments, bool success
  - GetAvailableRobots.srv: RobotMode[] acceptable_modes → string[] robot_ids
  - CancelTask.srv: string task_id → bool success

- [ ] Implement task bidding system in fleet_manager
  - TaskBiddingSystem class with cost calculation
  - Cost components: distance (1.0x), battery penalty (5.0x), time (0.5x), priority (10.0x)
  - Battery feasibility check (10% safety margin)
  - Filter robots by availability (IDLE, WAITING modes only)

- [ ] Implement task allocation optimizer
  - Multi-task optimization using Hungarian algorithm (scipy.optimize.linear_sum_assignment)
  - Build cost matrix: robots × tasks
  - Solve optimal assignment problem
  - Publish assignments to /fleet/task_assignments

- [ ] Extend task_manager for multi-robot support
  - Accept robot_id parameter in task goal messages
  - Per-robot task state machines (not global singleton)
  - Task completion tracking and reporting
  - Handle task cancellation and reassignment

- [ ] Add task queue management
  - Priority queue for pending tasks
  - Task aging to prevent starvation
  - Deadline-aware scheduling
  - Automatic reallocation on robot failure

**Verification:** Assign 10 tasks to 5 robots, verify optimal allocation, track completion times

### Phase 3: Open-RMF Integration (2 weeks, P2)

**Goal:** Enable facility-level orchestration with Open-RMF standard

- [ ] Install Open-RMF dependencies
  - rmf_fleet_msgs: FleetState, RobotState, RobotMode, Location
  - rmf_task_msgs: BidNotice, BidProposal, TaskRequest
  - builtin_interfaces: Time, Duration

- [ ] Implement Open-RMF message publishers in fleet_manager
  - Convert warehouser RobotStatus to rmf_fleet_msgs/RobotState
  - Publish to /fleet_states topic (Open-RMF standard)
  - Include battery_percent, mode, location, path
  - Sequence number tracking per robot

- [ ] Implement BidNotice subscriber
  - Parse task request JSON from BidNotice
  - Calculate best robot assignment using existing bidding system
  - Compute prev_cost (current fleet cost) and new_cost (with task)
  - Estimate finish_time based on distance and task duration

- [ ] Implement BidProposal publisher
  - Publish to /bid_proposals topic (Open-RMF standard)
  - Include fleet_name, expected_robot_name, costs, finish_time
  - Handle dry_run mode (no actual assignment)

- [ ] Add PathRequest and ModeRequest subscribers
  - PathRequest: Receive traffic-scheduled paths from RMF
  - ModeRequest: Handle mode changes (e.g., PAUSE for traffic control)
  - Translate to Warehouser navigation commands

- [ ] Create Open-RMF configuration
  - Fleet capabilities: max speed, battery capacity, charging time
  - Robot specifications: dimensions, payload capacity
  - Map metadata: coordinate system, floor levels

**Verification:** Connect to Open-RMF TaskDispatcher, participate in task auction, execute assigned tasks

### Phase 4: VDA5050 Protocol Support (2 weeks, P2)

**Goal:** Enable vehicle-level interoperability with VDA5050 standard

- [ ] Create warehouser_vda5050_connector package
  - C++ node with MQTT client (paho-mqtt)
  - JSON parsing (nlohmann/json)
  - VDA5050 v2.0.0 message schemas

- [ ] Implement MQTT client and topic structure
  - Connect to MQTT broker (configurable URL)
  - Topic pattern: uagv/v2/warehouser/{robot_id}/{topic}
  - Topics: order, instantActions (QoS 0), state, visualization (QoS 0), connection (QoS 1)

- [ ] Implement Order message subscriber
  - Parse order JSON (orderId, nodes, edges, actions)
  - Convert node/edge graph to waypoint list
  - Extract actions (pick, drop, charge, wait) with parameters
  - Handle released/unreleased staging
  - Send navigation commands to assigned robot

- [ ] Implement State message publisher
  - Collect robot state from simulation
  - Populate VDA5050 state schema: nodeStates, edgeStates, actionStates, agvPosition, velocity, batteryState
  - Track lastNodeId, lastNodeSequenceId, distanceSinceLastNode
  - Publish at 1 Hz (QoS 0)

- [ ] Implement Visualization message publisher
  - High-frequency position updates (10 Hz)
  - agvPosition and velocity only (lightweight)
  - Drop if offline (not critical)

- [ ] Implement InstantActions subscriber
  - Handle stopPause, resumeDrive actions
  - Emergency stop integration with safety controller
  - Immediate action execution (override current order)

- [ ] Implement Connection heartbeat publisher
  - Connection state (ONLINE, OFFLINE, CONNECTIONBROKEN)
  - Publish at 0.5 Hz (QoS 1, at least once)

- [ ] Add VDA5050 action mapping
  - pick → /robot_{id}/pick_object service
  - drop → /robot_{id}/drop_object service
  - charge → navigate to charging station + start charging
  - wait → set mode to WAITING, pause navigation
  - stopPause → emergency stop via safety controller

**Verification:** Send VDA5050 Order via MQTT, observe robot navigation, verify State messages

### Phase 5: Fleet Monitoring Dashboard (2-3 weeks, P1)

**Goal:** Real-time visualization and performance analytics

- [ ] Extend web_frontend with fleet monitoring
  - FleetDashboard.tsx component
  - FleetMap.tsx real-time visualization
  - MetricCard.tsx reusable metric display
  - RobotStatusTable.tsx per-robot details

- [ ] Implement WebSocket fleet state streaming
  - Backend: Node.js WebSocket server (ws library)
  - Subscribe to ROS2 /fleet/state and /fleet/metrics via rclnodejs
  - Stream updates to browser clients at 10 Hz
  - Handle client connect/disconnect

- [ ] Add fleet metrics display
  - Total robots, active, idle, charging, emergency
  - Average battery level with warning indicators
  - Fleet utilization percentage
  - Tasks per hour throughput
  - Average velocity of moving robots

- [ ] Add per-robot status table
  - Robot ID, position (x, y), battery indicator
  - Mode badge with color coding
  - Current task ID (if assigned)
  - Velocity display

- [ ] Implement fleet map visualization
  - Canvas-based 2D map rendering
  - Robot circles with mode-based colors
  - Orientation arrows showing heading
  - Robot ID labels
  - Zoom and pan controls

- [ ] Add historical data tracking
  - Store metrics to TimescaleDB or InfluxDB
  - Task completion timeline
  - Battery level trends
  - Robot utilization over time
  - Performance analytics charts

- [ ] Implement alerting system
  - Low battery warnings (< 20%)
  - Collision detection alerts
  - Task timeout notifications (missed deadlines)
  - Robot emergency state alerts
  - Fleet utilization anomalies

**Verification:** Monitor fleet of 10 robots in real-time, verify metrics accuracy, test alerting

## Interface Definitions

### Core Messages (warehouser_msgs/msg)

```msg
# RobotStatus.msg
string robot_id
float32 x
float32 y
float32 theta
float32 battery_percent
uint8 mode  # RobotMode enum
string current_task_id
float32 velocity
float32[] planned_path_x
float32[] planned_path_y
builtin_interfaces/Time timestamp

# FleetState.msg
string fleet_name
RobotStatus[] robots
builtin_interfaces/Time timestamp

# RobotMode.msg
uint8 IDLE = 0
uint8 CHARGING = 1
uint8 MOVING = 2
uint8 PAUSED = 3
uint8 WAITING = 4
uint8 EMERGENCY = 5
uint8 PERFORMING_ACTION = 10

# FleetMetrics.msg
int32 total_robots
int32 active_robots
int32 idle_robots
int32 charging_robots
int32 emergency_robots
float32 average_battery
int32 tasks_in_progress
int32 tasks_completed_last_hour
float32 fleet_utilization  # 0.0 to 1.0
float32 average_velocity
builtin_interfaces/Time timestamp

# Task.msg
string task_id
uint8 task_type  # PICKUP=0, DELIVERY=1, CHARGE=2, PATROL=3
float32 pickup_x
float32 pickup_y
float32 delivery_x
float32 delivery_y
int32 priority  # 1-10, higher is more urgent
builtin_interfaces/Time deadline  # optional
float32 payload_weight

# TaskBid.msg
string robot_id
string task_id
float32 cost
builtin_interfaces/Time estimated_completion_time
float32 confidence  # 0.0 to 1.0

# TaskAssignment.msg
string task_id
string robot_id
builtin_interfaces/Time assignment_time
float32 estimated_cost
```

### Services (warehouser_msgs/srv)

```srv
# RequestTaskAllocation.srv
Task[] tasks
---
TaskAssignment[] assignments
bool success
string message

# GetAvailableRobots.srv
uint8[] acceptable_modes  # RobotMode values
---
string[] robot_ids
bool success

# CancelTask.srv
string task_id
---
bool success
string message

# SimReset.srv (modify existing)
int32 robot_count  # NEW: number of robots to spawn
---
bool success

# GetObservation.srv (modify existing)
int32 robot_id  # NEW: which robot's observation
---
float32[] observation
bool success
```

### TypeScript Interfaces (web_frontend/src/types/fleet.ts)

```typescript
interface RobotStatus {
  robot_id: string;
  x: number;
  y: number;
  theta: number;
  battery_percent: number;
  mode: RobotMode;
  current_task_id: string;
  velocity: number;
  timestamp: number;
}

enum RobotMode {
  IDLE = 0,
  CHARGING = 1,
  MOVING = 2,
  PAUSED = 3,
  WAITING = 4,
  EMERGENCY = 5,
  PERFORMING_ACTION = 10
}

interface FleetState {
  fleet_name: string;
  robots: RobotStatus[];
  timestamp: number;
}

interface FleetMetrics {
  total_robots: number;
  active_robots: number;
  idle_robots: number;
  charging_robots: number;
  emergency_robots: number;
  average_battery: number;
  tasks_in_progress: number;
  tasks_completed_last_hour: number;
  fleet_utilization: number;
  average_velocity: number;
  timestamp: number;
}

interface Task {
  task_id: string;
  task_type: TaskType;
  pickup_location: [number, number];
  delivery_location: [number, number];
  priority: number;
  deadline?: number;
  payload_weight: number;
}

enum TaskType {
  PICKUP = 0,
  DELIVERY = 1,
  CHARGE = 2,
  PATROL = 3
}

interface TaskAssignment {
  task_id: string;
  robot_id: string;
  assignment_time: number;
  estimated_cost: number;
}
```

## New Packages to Create

| Package | Language | Purpose | Key Exports |
|---------|----------|---------|-------------|
| warehouser_fleet_manager | C++ | Fleet-level orchestration, state aggregation, task allocation | FleetStateAggregator, TaskBiddingSystem, fleet_manager_node |
| warehouser_vda5050_connector | C++ | VDA5050 protocol bridge via MQTT | VDA5050Connector, MQTTClient, vda5050_connector_node |
| (optional) warehouser_rmf_adapter | C++ | Open-RMF fleet adapter | RMFFleetAdapter, rmf_adapter_node |

## Files to Create

| File | Purpose |
|------|---------|
| ros_ws/src/warehouser_msgs/msg/RobotStatus.msg | Per-robot detailed state |
| ros_ws/src/warehouser_msgs/msg/FleetState.msg | Aggregate fleet status |
| ros_ws/src/warehouser_msgs/msg/RobotMode.msg | Robot operational mode enum |
| ros_ws/src/warehouser_msgs/msg/FleetMetrics.msg | Performance KPIs |
| ros_ws/src/warehouser_msgs/msg/Task.msg | Task definition |
| ros_ws/src/warehouser_msgs/msg/TaskBid.msg | Cost estimate for task |
| ros_ws/src/warehouser_msgs/msg/TaskAssignment.msg | Assignment result |
| ros_ws/src/warehouser_msgs/srv/RequestTaskAllocation.srv | Task allocation service |
| ros_ws/src/warehouser_msgs/srv/GetAvailableRobots.srv | Query available robots |
| ros_ws/src/warehouser_msgs/srv/CancelTask.srv | Cancel task service |
| ros_ws/src/warehouser_fleet_manager/src/fleet_manager_node.cpp | Main fleet orchestration node |
| ros_ws/src/warehouser_fleet_manager/src/fleet_state_aggregator.cpp | State aggregation logic |
| ros_ws/src/warehouser_fleet_manager/src/task_bidding_system.cpp | Task allocation algorithm |
| ros_ws/src/warehouser_fleet_manager/include/warehouser_fleet_manager/fleet_state_aggregator.hpp | Header |
| ros_ws/src/warehouser_fleet_manager/include/warehouser_fleet_manager/task_bidding_system.hpp | Header |
| ros_ws/src/warehouser_fleet_manager/config/fleet_config.yaml | Fleet configuration |
| ros_ws/src/warehouser_fleet_manager/launch/fleet_manager.launch.py | Launch file |
| ros_ws/src/warehouser_vda5050_connector/src/vda5050_connector_node.cpp | VDA5050 MQTT bridge |
| ros_ws/src/warehouser_vda5050_connector/src/mqtt_client.cpp | MQTT client wrapper |
| ros_ws/src/warehouser_vda5050_connector/src/order_translator.cpp | Order graph parser |
| ros_ws/src/warehouser_vda5050_connector/src/state_publisher.cpp | State message builder |
| ros_ws/src/warehouser_vda5050_connector/include/warehouser_vda5050_connector/vda5050_schemas.hpp | JSON schemas |
| ros_ws/src/warehouser_vda5050_connector/config/vda5050_config.yaml | MQTT broker, topic config |
| web_frontend/src/components/FleetDashboard.tsx | Main dashboard component |
| web_frontend/src/components/FleetMap.tsx | 2D fleet visualization |
| web_frontend/src/components/MetricCard.tsx | Reusable metric display |
| web_frontend/src/components/RobotStatusTable.tsx | Per-robot status table |
| web_frontend/src/hooks/useWebSocket.ts | WebSocket connection hook |
| web_frontend/src/stores/fleetStore.ts | Zustand fleet state store |
| web_frontend/src/types/fleet.ts | TypeScript interfaces |
| web_frontend/server/websocket.ts | WebSocket server |
| web_frontend/server/ros_bridge.ts | ROS2 to WebSocket bridge |

## Files to Modify

| File | Change |
|------|--------|
| ros_ws/src/warehouser_msgs/msg/Entity.msg | Add `string robot_id` field, `float32 battery_percent` field |
| ros_ws/src/warehouser_msgs/srv/GetObservation.srv | Add `int32 robot_id` request field |
| ros_ws/src/warehouser_simulation/srv/SimReset.srv | Add `int32 robot_count` request field (or create if missing) |
| ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp | Fix action routing (line 194), fix reset spawn (line 242), add robot_id to GetObservation call (line 259) |
| ros_ws/src/warehouser_simulation/src/simulation_node.cpp | Subscribe to per-robot cmd_vel topics (/robot_0/cmd_vel, etc.), implement multi-robot spawning on reset |
| ros_ws/src/warehouser_simulation/src/world_manager.cpp | Add `spawnRobots(int count)` method, assign robot_id to entities, implement battery drain model |
| ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp | Declare `spawnRobots`, add `robot_count_` member |
| ros_ws/src/warehouser_task/src/task_manager_node.cpp | Accept robot_id in task goal, per-robot task state machines instead of singleton |
| ros_ws/src/warehouser_task/include/warehouser_task/task_state_machine.hpp | Add robot_id to Task struct |
| web_frontend/src/ros/connection.ts | Add fleet state subscriptions (/fleet/state, /fleet/metrics) |
| web_frontend/src/store/appStore.ts | Add fleetState and fleetMetrics to Zustand store |
| web_frontend/src/App.tsx | Add route for FleetDashboard component |

## Architecture Notes

### Modularity Principles

1. **Separation of Concerns**
   - Simulation layer: Physics, entity management, world state
   - RL Bridge layer: Training interface, reward calculation
   - Fleet Manager layer: Orchestration, task allocation, state aggregation
   - Protocol Adapters: Open-RMF and VDA5050 translations (optional, separate packages)

2. **Interface Standardization**
   - ROS2 messages follow REP-103 coordinate conventions
   - Open-RMF compliance for facility-level integration
   - VDA5050 compliance for vehicle-level integration
   - Clear service contracts with comprehensive error handling

3. **Robot Namespacing**
   - Per-robot topic hierarchy: /robot_{id}/cmd_vel, /robot_{id}/status
   - Global fleet topics: /fleet/state, /fleet/metrics, /fleet/task_assignments
   - Consistent robot_id throughout system (string format: "robot_0", "robot_1", etc.)

4. **Scalability Considerations**
   - FleetState published at 10 Hz (responsive but not overwhelming)
   - FleetMetrics published at 1 Hz (sufficient for monitoring)
   - VDA5050 State at 1 Hz, Visualization at 10 Hz (matches spec)
   - Task allocation optimized with Hungarian algorithm (O(n³) acceptable for <100 robots)

5. **Error Handling**
   - All services return success bool + message string
   - Robot failure detection via heartbeat timeout
   - Task reallocation on robot failure or timeout
   - Emergency stop propagation via InstantActions

6. **Configuration Management**
   - YAML configuration files for all tunable parameters
   - Cost weights for task bidding (distance, battery, time, priority)
   - Fleet capabilities (max speed, battery capacity, charging rate)
   - MQTT broker settings, topic structure
   - Battery drain model parameters

### Technology Stack

**ROS2 Packages (C++):**
- Standard: C++23, vcpkg dependencies
- Error handling: std::expected (not exceptions)
- Threading: Single-threaded nodes (no mutex)
- Logging: RCLCPP_INFO/WARN/ERROR
- Testing: Google Test

**Python Components (Optional):**
- Fleet state aggregator can be Python or C++ (C++ preferred for performance)
- Training integration already uses rclpy

**Web Frontend:**
- TypeScript 5+ strict mode, React 18+
- State: Zustand for fleet state management
- Visualization: Canvas 2D for fleet map
- Communication: WebSocket for real-time updates

**External Dependencies:**
- MQTT: paho-mqtt (C++) or mosquitto (broker)
- JSON: nlohmann/json
- Optimization: scipy (Python) or similar C++ library
- Time series DB: TimescaleDB or InfluxDB (optional)

### Dual-Standard Strategy

**Open-RMF (Facility-Level):**
- Use for internal Warehouser deployments
- Provides traffic scheduling, task dispatching
- Heterogeneous fleet coordination
- Battery-aware task planning

**VDA5050 (Vehicle-Level):**
- Use for external integration with commercial fleet managers
- Industry-standard protocol compliance
- MQTT-based decoupling
- Graph-based navigation compatibility

**Bridge Between Standards:**
- Fleet Manager acts as translation layer
- Open-RMF task requests → VDA5050 Orders
- VDA5050 State → Open-RMF RobotState
- Unified cost-based bidding system

## Acceptance Criteria

### Phase 0 (Critical Fixes)
- [ ] Multi-robot PettingZoo env with 5 robots, all robots move independently
- [ ] RLStep actions route to correct robot based on robot_id parameter
- [ ] RLReset spawns N robots as specified by robot_count parameter
- [ ] GetObservation service returns per-robot observations

### Phase 1 (Fleet State)
- [ ] FleetState message published at 10 Hz with accurate robot positions
- [ ] FleetMetrics calculated correctly (utilization, battery, throughput)
- [ ] Battery tracking: drains during movement, recharges in charging zone
- [ ] Robot mode correctly inferred (IDLE, MOVING, CHARGING, etc.)

### Phase 2 (Task Allocation)
- [ ] Assign 10 tasks to 5 robots, verify optimal allocation
- [ ] Task cost calculation accounts for distance, battery, priority
- [ ] Task queue manages priorities and deadlines
- [ ] Task completion tracked and reported to metrics
- [ ] Handle robot failure: reassign tasks to other robots

### Phase 3 (Open-RMF)
- [ ] Publish FleetState in Open-RMF format to /fleet_states
- [ ] Respond to BidNotice with BidProposal containing accurate cost estimate
- [ ] Execute tasks assigned via Open-RMF Task Dispatcher
- [ ] Handle PathRequest and ModeRequest from traffic scheduler

### Phase 4 (VDA5050)
- [ ] Receive VDA5050 Order via MQTT, parse nodes/edges/actions
- [ ] Navigate to nodes in sequence, execute actions (pick, drop)
- [ ] Publish VDA5050 State with nodeStates, edgeStates, actionStates
- [ ] Publish Visualization at 10 Hz, State at 1 Hz
- [ ] Handle InstantActions (stopPause) immediately

### Phase 5 (Dashboard)
- [ ] Display fleet metrics in real-time: 10 robots monitored
- [ ] Per-robot status table with battery, mode, task, position
- [ ] Fleet map visualization with robot positions and orientations
- [ ] Alerts trigger for low battery, emergency state, task timeout
- [ ] Historical data: view task completion trends over last 24 hours

### Overall System
- [ ] Build passes: colcon build in ros_ws
- [ ] Tests pass: pytest in training/, colcon test in ros_ws
- [ ] Fleet of 10 robots completes 50 tasks in simulation
- [ ] Average fleet utilization > 70% during task execution
- [ ] Zero task assignment conflicts or robot collisions
- [ ] Dashboard updates within 100ms latency

## Testing Strategy

### Unit Tests

**C++ (Google Test):**
- FleetStateAggregator: metrics calculation, robot filtering
- TaskBiddingSystem: cost calculation, bid ranking, feasibility checks
- VDA5050 message parsing: Order, State, InstantActions JSON schemas
- MQTT client: connect, publish, subscribe, reconnect

**Python (pytest):**
- PettingZoo env: multi-robot observations, actions, rewards
- Task allocation optimizer: Hungarian algorithm correctness
- Battery model: drain rate, charging rate

### Integration Tests

**ROS2 Integration (C++ + launch tests):**
- rl_bridge → simulation: multi-robot action routing
- fleet_manager → simulation: fleet state aggregation from WorldState
- fleet_manager → task_manager: task assignment execution
- vda5050_connector → simulation: Order to navigation translation

**End-to-End:**
- Spawn 5 robots, assign 10 tasks, verify all complete successfully
- Send VDA5050 Order, verify robot navigates and executes actions
- Submit Open-RMF BidNotice, verify BidProposal and task execution
- Test robot failure: kill one robot, verify tasks reassigned

### Performance Tests

- Fleet state publishing frequency: measure actual Hz vs target 10 Hz
- Task allocation latency: 10 tasks to 10 robots < 100ms
- WebSocket streaming: 10 clients, 10 robots, no dropped messages
- MQTT throughput: 100 messages/sec sustained
- Scalability: 20 robots, verify system remains responsive

### Compliance Tests

**Open-RMF Compliance:**
- FleetState message format matches rmf_fleet_msgs spec
- BidProposal includes all required fields
- Mode constants match RMF standard values

**VDA5050 Compliance:**
- Order message parses all v2.0.0 fields
- State message includes all required fields
- Topic structure matches spec: uagv/v2/{manufacturer}/{serialNumber}/{topic}
- QoS levels correct: State (0), Connection (1)

## Dependencies

### Build Dependencies

**vcpkg (C++):**
- nlohmann-json: JSON parsing
- paho-mqtt: MQTT client
- rclcpp: ROS2 C++ client
- rmf-fleet-msgs: Open-RMF message definitions (optional)
- rmf-task-msgs: Open-RMF task messages (optional)

**npm (TypeScript):**
- ws: WebSocket server
- vda-5050: TypeScript VDA5050 library (optional)
- zustand: State management
- react: UI framework

**Python (uv):**
- pettingzoo: Multi-agent RL environment
- scipy: Optimization algorithms (Hungarian)
- rclpy: ROS2 Python client

### Runtime Dependencies

**ROS2 Packages:**
- warehouser_msgs: Message definitions
- warehouser_simulation: Core simulation
- warehouser_rl_bridge: RL training interface

**External Services:**
- MQTT broker (Mosquitto recommended) - for VDA5050
- TimescaleDB or InfluxDB - for historical data (optional)
- Open-RMF Traffic Schedule node - for traffic coordination (optional)

## Timeline Estimate

- Phase 0 (Critical Fixes): 1 week
- Phase 1 (Fleet State): 2-3 weeks
- Phase 2 (Task Allocation): 2-3 weeks
- Phase 3 (Open-RMF): 2 weeks
- Phase 4 (VDA5050): 2 weeks
- Phase 5 (Dashboard): 2-3 weeks

**Total: 11-14 weeks** for complete fleet management system with full standards compliance

**Minimum Viable Fleet Manager: 5-6 weeks** (Phase 0 + Phase 1 + Phase 2 only)

## Priority Breakdown

**Must Have (MVP):**
- Phase 0: Critical bug fixes
- Phase 1: Fleet state infrastructure
- Phase 2: Task allocation

**Should Have:**
- Phase 5: Fleet dashboard (monitoring is critical for production)
- Phase 3: Open-RMF (standardization important for scalability)

**Nice to Have:**
- Phase 4: VDA5050 (useful for external integration but not core functionality)
- Historical data tracking
- Advanced analytics

## Risk Mitigation

**Technical Risks:**
1. Multi-robot collision avoidance not implemented
   - Mitigation: Start with large workspace, low robot density
   - Future: Implement traffic coordination (Phase 3 Open-RMF)

2. Battery model is simplified (distance-based only)
   - Mitigation: Configurable drain rate, conservative estimates
   - Future: Add payload-based drain, acceleration costs

3. Task allocation scalability (O(n³) Hungarian algorithm)
   - Mitigation: Acceptable for <100 robots, practical limit ~50 robots
   - Future: Approximate algorithms for larger fleets

**Integration Risks:**
1. Open-RMF dependencies may have version conflicts
   - Mitigation: Phase 3 is optional, use standard ROS2 messages as base
   - Fallback: Custom message definitions instead of rmf_fleet_msgs

2. MQTT broker reliability for VDA5050
   - Mitigation: Phase 4 is nice-to-have, not critical path
   - Fallback: REST API or direct ROS2 topics

**Timeline Risks:**
1. Debugging multi-robot issues time-consuming
   - Mitigation: Comprehensive logging, visualization tools
   - Incremental testing: 2 robots → 3 robots → 5 robots

2. Frontend WebSocket integration complex
   - Mitigation: Phase 5 can be delayed, use RViz temporarily
   - Fallback: Command-line monitoring tools

## Next Steps

1. **Immediate Action (Day 1-2):** Fix Phase 0 critical bugs
   - Start with rl_bridge_node.cpp action routing fix
   - Verify with simple 2-robot test case
   - Commit and test thoroughly before proceeding

2. **Foundation (Week 1-2):** Implement Phase 1 fleet state
   - Define core messages in warehouser_msgs
   - Create fleet_manager package structure
   - Basic FleetState publisher from simulation

3. **Intelligence (Week 3-4):** Implement Phase 2 task allocation
   - Task bidding system with cost calculation
   - Simple queue management
   - Test with 5 robots, 10 tasks

4. **Production Polish:** Monitoring, testing, documentation
   - Phase 5 dashboard for visibility
   - Comprehensive testing
   - Configuration tuning

5. **Standards Compliance (Optional):** Phase 3 and 4 based on requirements
   - Add if integrating with external systems
   - Otherwise, custom interfaces are sufficient

## References

**Research Sources:**
- S.md: Open-RMF architecture, VDA5050 protocol specification, integration patterns
- I.md: Current multi-robot implementation analysis, critical bugs, architectural gaps
- T.md: Implementation patterns from Open-RMF, VDA5050, fleet management systems

**Standards:**
- Open-RMF: https://github.com/open-rmf/rmf_demos
- VDA5050 v2.0.0: https://github.com/VDA5050/VDA5050
- REP 103: Standard Units of Measure and Coordinate Conventions

**Key Repositories:**
- open-rmf/rmf_fleet_adapter: Fleet adapter reference implementation
- inorbit-ai/ros_amr_interop: ROS2 VDA5050 connector examples
- coatyio/vda-5050-lib.js: TypeScript VDA5050 library
