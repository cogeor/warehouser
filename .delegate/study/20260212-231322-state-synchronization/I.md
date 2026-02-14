# Introspect: State Synchronization Implementation Analysis

Created: 2026-02-12T23:20:00Z

## Focus

Deep analysis of state synchronization implementation in Warehouser, examining how state flows between the simulation, training, and frontend components. This analysis covers authoritative state management, distribution mechanisms, timing/ordering guarantees, and identifies gaps versus best practices.

## Current State Architecture

### Single Source of Truth: WorldManager

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\include\warehouser_simulation\world_manager.hpp`

The `WorldManager` class (lines 36-145) is explicitly documented as the "single source of truth for world state." This is the authoritative state holder containing:

- **Entities**: Robots, objects, walls, zones stored in unique_ptr vectors (lines 134-137)
- **Simulation time**: `sim_time_` tracks authoritative time (line 143)
- **Running state**: `running_` flag controls simulation execution (line 144)
- **Initial state**: Cached for reset functionality (lines 140-141)

**State mutation** happens exclusively through:
- `step(float dt)` - advances simulation physics (line 60)
- `reset()` - restores initial state (line 56)
- `moveEntity()` - manual entity positioning (line 104)
- Robot command callbacks via subscribers

This design correctly implements the **authoritative server pattern** recommended in the research.

### State Production: SimulationNode

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\src\simulation_node.cpp`

The `SimulationNode` publishes world state at **50 Hz** (dt=0.02s, line 72):

```cpp
void SimulationNode::tick() {
    world_.step(dt_);
    state_pub_->publish(world_.toMsg());  // Line 161
    clock_pub_->publish(clock_msg);        // Line 167
}
```

**Publisher creation** (line 122-124):
```cpp
state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
    "/world/state", 10);
```

**CRITICAL GAP #1**: Uses default QoS settings (depth=10, Reliable, **Volatile**). Missing recommended `TRANSIENT_LOCAL` durability for late-joining subscribers.

### State Message Structure

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\WorldState.msg`

The state message is well-designed:
- `Entity[] entities` - flat array of all entities
- `float32 sim_time` - authoritative simulation time
- `bool running` - simulation state

**Serialization** happens in `WorldManager::toMsg()` (world_manager.cpp:215-241):
- Converts all robots, objects, walls, zones to Entity messages
- Creates new message every tick (no state diffing)

**Message size**: Approximately 50-100 bytes per entity. For a warehouse with 3 robots, 5 objects, 4 walls, 1 zone = ~650-1300 bytes at 50 Hz = **32.5-65 KB/s bandwidth**.

## State Consumers

### 1. Training Client (Python/ROS2)

**File:** `C:\Users\costa\src\warehouser\training\training\envs\ros_env.py`

Training uses **synchronous service calls** (lines 124-146 for reset, 173-211 for step):

```python
# Reset pattern
request = RLReset.Request()
future = self._reset_client.call_async(request)
rclpy.spin_until_future_complete(self._node, future, timeout_sec=5.0)
response = future.result()
obs = np.array(response.observation.data, dtype=np.float32)
```

This is **request/response**, not publish/subscribe. Training does NOT consume the `/world/state` topic directly.

**State flow for training:**
1. Training → `/rl/step` service call with action
2. RLBridgeNode executes action on simulation
3. RLBridgeNode reads updated world state
4. RLBridgeNode builds observation and calculates reward
5. Response returns observation/reward to training

**INSIGHT**: Training client is **decoupled from world state publication**. It receives state derivatives (observations) via service responses, not raw world state.

### 2. RLBridgeNode (Observation Consumer)

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\rl_bridge_node.cpp`

RLBridgeNode subscribes to `/world/state` (lines 39-42):

```cpp
world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
    "/world/state", 10,
    std::bind(&RLBridgeNode::worldStateCallback, this, std::placeholders::_1));
```

**State handling** (lines 75-79):
```cpp
void RLBridgeNode::worldStateCallback(
    const warehouser_msgs::msg::WorldState::SharedPtr msg) {
    curr_world_ = *msg;  // Copy entire state
    world_received_ = true;
}
```

**CRITICAL GAP #2**: Uses default QoS (depth=10, Reliable, Volatile). No explicit QoS configuration visible.

**State usage in RLStep** (lines 86-130):
- Stores previous world state per robot (line 103)
- Steps simulation via `/sim/step` service call (line 111)
- Service response includes updated world state (line 232)
- Uses stored `curr_world_` for reward calculation

**RACE CONDITION RISK**: The `worldStateCallback` updates `curr_world_` asynchronously at 50 Hz, while `handleRLStep` reads it synchronously. If a world state update arrives during step processing, `curr_world_` could change mid-calculation. However, this is **mitigated** because:
1. ROS2 nodes are single-threaded by default
2. Callbacks execute sequentially
3. Service handler blocks other callbacks during execution

### 3. ObservationsNode (State Consumer)

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observations_node.cpp`

Subscribes to `/world/state` with same default QoS (lines 43-46):

```cpp
world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
    "/world/state", 10,
    std::bind(&ObservationsNode::worldStateCallback, this, std::placeholders::_1));
```

**State caching** (lines 88-92):
```cpp
void ObservationsNode::worldStateCallback(
    const warehouser_msgs::msg::WorldState::SharedPtr msg) {
    last_world_ = *msg;  // Copy entire state
    world_received_ = true;
}
```

**State usage**: Reads `last_world_` at multiple frequencies:
- Observation publishing: 20 Hz (line 18)
- Lidar debug: 10 Hz (line 19)
- Odometry: 50 Hz (line 20)

**TIMING MISMATCH**: Odometry publishes at 50 Hz (same as world state), but observation and lidar are slower. This is intentional for performance, but creates **temporal aliasing** - slower consumers may skip world state updates.

### 4. Frontend (TypeScript/WebSocket)

**File:** `C:\Users\costa\src\warehouser\web_frontend\src\ros\connection.ts`

Frontend connects via rosbridge WebSocket (line 101):

```typescript
ros = new ROSLIB.Ros({
    url: 'ws://localhost:9090',
})
```

**World state subscription** (lines 127-169):
```typescript
const worldStateTopic = new ROSLIB.Topic({
    ros,
    name: '/world/state',
    messageType: 'warehouser_msgs/WorldState',
})

worldStateTopic.subscribe((msg: unknown) => {
    const entities: Entity[] = message.entities.map(...)
    store.setEntities(entities)
    store.setSimTime(message.sim_time)
})
```

**State storage** (file: `web_frontend/src/store/appStore.ts`, lines 26-27):
```typescript
entities: Entity[]
setEntities: (entities: Entity[]) => void
```

**CRITICAL GAP #3**: No QoS configuration visible in frontend subscription (rosbridge limitation - uses default QoS).

**Frontend state updates**:
- Zustand store uses **last-value-wins** (line 66)
- No optimistic updates (frontend is read-only)
- No state reconciliation (no bidirectional sync)
- Reconnection logic with exponential backoff (lines 36-61)

**ISSUE**: Frontend has reconnection logic, but may miss state updates during disconnection. After reconnect, it will receive next published state, but there's **no state snapshot recovery** since publisher uses Volatile durability.

## State Synchronization Mechanisms

### 1. Publish/Subscribe (World State Topic)

**Topic:** `/world/state`
**Rate:** 50 Hz
**QoS:** Default (depth=10, Reliable, **Volatile**)
**Consumers:** RLBridgeNode, ObservationsNode, Frontend (via rosbridge)

**Flow:**
```
SimulationNode (50Hz tick)
    → world_.step(dt)
    → state_pub_->publish(world_.toMsg())
    → DDS middleware broadcasts
    → RLBridgeNode.worldStateCallback (copies to curr_world_)
    → ObservationsNode.worldStateCallback (copies to last_world_)
    → rosbridge → WebSocket → Frontend (updates Zustand store)
```

**Timing characteristics:**
- Publisher: 50 Hz (20ms period)
- Propagation latency: <1ms local, ~10-50ms over WebSocket
- Frontend render: 60 fps (16.6ms) - interpolation would smooth updates

### 2. Request/Response (Services)

**Services used for state mutation and observation:**

**RLStep** (`/rl/step`):
```
Training client → RLStep.Request (action)
    → RLBridgeNode.handleRLStep
    → sendAction (publishes to /cmd_vel)
    → stepSimulation (calls /sim/step service)
    → SimulationNode.handleStep
        → world_.step(dt) × N
        → returns updated world state
    → calculate reward
    → getObservation (calls /observations/get)
    → returns RLStep.Response
```

**RLReset** (`/rl/reset`):
```
Training client → RLReset.Request
    → RLBridgeNode.handleRLReset
    → resetSimulation (calls /sim/reset)
    → SimulationNode.handleReset → world_.reset()
    → setRandomGoal
    → wait for world state update (lines 163-168)
    → getObservation × robot_count
    → returns RLReset.Response
```

**SimStep** (`/sim/step`):
```
Client → SimStep.Request (num_ticks)
    → SimulationNode.handleStep
    → world_.step(dt) × num_ticks
    → returns final WorldState in response
```

**SYNCHRONIZATION POINT**: `RLBridgeNode.handleRLReset` waits for world state update after reset (lines 163-168):

```cpp
rclcpp::Rate rate(100);
for (int i = 0; i < 50 && rclcpp::ok(); ++i) {
    rclcpp::spin_some(shared_from_this());
    rate.sleep();
    if (world_received_) break;
}
```

This **polling loop** ensures `curr_world_` is updated before building observations. However, it's a **brittle pattern** - depends on timing assumptions.

## QoS and Timing Analysis

### Current QoS Configuration

**World State Publisher** (simulation_node.cpp:122-124):
- Reliability: **RELIABLE** (default)
- Durability: **VOLATILE** (default)
- History: **KEEP_LAST** (default)
- Depth: **10** (explicit parameter)

**Subscribers** (rl_bridge_node.cpp, observations_node.cpp):
- Same defaults (depth=10, Reliable, Volatile)

### Gap Analysis vs Best Practices

**Research recommendation** (from S.md, lines 286-302):
```python
world_state_qos = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,      # ✓ Current: RELIABLE
    durability=DurabilityPolicy.TRANSIENT_LOCAL, # ✗ Current: VOLATILE
    history=HistoryPolicy.KEEP_LAST,             # ✓ Current: KEEP_LAST
    depth=1                                      # ✗ Current: 10
)
```

**Missing features:**
1. **TRANSIENT_LOCAL durability**: Late-joining subscribers (frontend reconnections) don't get current state
2. **Depth optimization**: Depth=10 wastes memory; only current state needed (depth=1)

### Timing Analysis

**Publication frequency**: 50 Hz (20ms period)

**Consumer frequencies:**
- RLBridgeNode: On-demand (service calls)
- ObservationsNode observation: 20 Hz (every 2.5 world states)
- ObservationsNode lidar: 10 Hz (every 5 world states)
- ObservationsNode odometry: 50 Hz (every world state)
- Frontend: 50 Hz received, rendered at 60 fps

**Temporal aliasing**: ObservationsNode skips world state updates for observation (20Hz) and lidar (10Hz). This is **intentional optimization** but means observations may not reflect the absolute latest state.

**Clock synchronization**: SimulationNode publishes `/clock` at 50 Hz (simulation_node.cpp:164-167). This enables ROS2 sim time, supporting deterministic replay.

## Consistency Assessment

### Strong Points

1. **Single authoritative source**: WorldManager is clearly the single source of truth
2. **Immutable state transitions**: State updates happen atomically in `step()` method
3. **Deterministic execution**: Fixed timestep, deterministic physics
4. **Clock publication**: Enables sim time for reproducibility

### Weak Points

1. **No state versioning**: Messages don't include version numbers or sequence IDs
2. **No consistency checks**: Consumers don't validate state continuity
3. **Temporal coupling in reset**: RLBridgeNode polls for state update after reset (brittle)
4. **No state recovery**: Frontend loses state during disconnection (Volatile durability)

### Race Conditions

**Potential race in RLStep** (rl_bridge_node.cpp:86-130):

The `handleRLStep` method:
1. Stores `prev_world_states_[robot_id] = curr_world_` (line 103)
2. Steps simulation via service call (line 111)
3. Service returns updated state in response (line 232)
4. Uses `curr_world_` for reward calculation (line 117)

**Question**: Does step 4 use the service response state or the subscriber-updated `curr_world_`?

**Answer** (line 232 in rl_bridge_node.cpp):
```cpp
curr_world_ = result->state;  // Uses service response
```

So the service response **overwrites** `curr_world_`, ensuring consistency. **No race condition.**

### State Divergence Risks

**Low risk overall** due to:
- Single writer (simulation)
- Read-only consumers
- Synchronous service calls for mutations

**Potential divergence**: If `/sim/step` service and timer tick execute concurrently, state could advance twice. However, ROS2 single-threaded executor prevents this.

## Recovery and Reconnection

### Frontend Reconnection

**File:** `web_frontend/src/ros/connection.ts`

**Reconnection strategy** (lines 36-61):
- Exponential backoff with jitter
- Max 10 attempts
- Delays: 1s, 2s, 4s, 8s, 16s, 30s (capped)

**Connection handlers** (lines 104-119):
```typescript
ros.on('connection', () => {
    store.setConnected(true)
    resetReconnectionState()
    subscribeToTopics()  // Resubscribe to all topics
})

ros.on('close', () => {
    store.setConnected(false)
    scheduleReconnect()
})
```

**State recovery**: After reconnection, frontend resubscribes to topics and waits for next published message. With **Volatile durability**, it won't receive the current state until the next tick (up to 20ms delay). This is acceptable for visualization but could cause brief "empty world" display.

**RECOMMENDATION**: Use TRANSIENT_LOCAL durability to provide immediate state on reconnection.

### Training Client Reconnection

**File:** `training/training/envs/ros_env.py`

**No reconnection logic**. Training uses **fail-fast** approach:
- Service calls have 5-second timeout (line 88, 90)
- Returns error observation on timeout (line 122, 189)
- Training script handles episode failure

This is **appropriate** for training - failed episodes are discarded.

### State Replay

**No state replay mechanism** currently implemented. Recommendations from S.md (lines 213-240):
- Event sourcing: record all actions/commands
- Checkpoint: serialize full world state
- Deterministic replay: fixed seed + recorded actions

**Current reset** (world_manager.cpp:81-106):
- Restores initial state from cached configs
- Resets sim_time to 0
- Does NOT restore arbitrary checkpoints

**GAP**: No save/load arbitrary state checkpoints for:
- Debugging specific scenarios
- Curriculum learning (start from advanced states)
- Regression testing with deterministic scenarios

## State Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        WorldManager                              │
│                  (Authoritative State)                          │
│  - robots_, objects_, walls_, zones_                            │
│  - sim_time_, running_                                          │
└─────────────────┬───────────────────────────────────────────────┘
                  │
                  │ step(dt)
                  │ reset()
                  │ moveEntity()
                  │
                  ▼
        ┌─────────────────────┐
        │  SimulationNode     │
        │  (State Publisher)  │
        └─────────┬───────────┘
                  │
                  │ publish 50 Hz
                  │ /world/state
                  │ QoS: Reliable, Volatile, depth=10
                  │
         ┌────────┼────────┬────────────┐
         │        │        │            │
         ▼        ▼        ▼            ▼
    ┌────────┐ ┌──────┐ ┌─────────┐  ┌─────────┐
    │ RL     │ │ Obs  │ │rosbridge│  │Frontend │
    │ Bridge │ │ Node │ │WebSocket│  │ Zustand │
    └────┬───┘ └──┬───┘ └────┬────┘  └────┬────┘
         │        │          │            │
         │ copy   │ copy     │ forward    │ setEntities()
         │ 50 Hz  │ 50 Hz    │ 50 Hz      │ last-value-wins
         ▼        ▼          ▼            ▼
    curr_world_ last_world_ → Frontend  entities[]
    per-robot                  store
    state
         │        │
         │        │ build observations
         │        │ 20 Hz (obs), 10 Hz (lidar), 50 Hz (odom)
         │        ▼
         │   /observations
         │   /observations/lidar_debug
         │   /scan
         │   /odom
         │
         │ RLStep service
         ▼
    ┌─────────────────┐
    │ Training Client │
    │  (Python Gym)   │
    └─────────────────┘
```

## Specific File Roles

### State Authority
- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp` (lines 36-145): Authoritative state container
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp` (lines 81-241): State mutation and serialization

### State Publication
- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp` (lines 156-168): 50 Hz publication tick
- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp` (lines 122-124): Publisher creation (missing QoS config)

### State Consumption (ROS2)
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp` (lines 39-42, 75-79): World state subscriber
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp` (lines 86-130): RLStep service (uses state for reward)
- `ros_ws/src/warehouser_observations/src/observations_node.cpp` (lines 43-46, 88-92): World state subscriber
- `ros_ws/src/warehouser_observations/src/observations_node.cpp` (lines 99-106, 108-127): Observation builders

### State Consumption (Frontend)
- `web_frontend/src/ros/connection.ts` (lines 127-169): World state subscription via rosbridge
- `web_frontend/src/store/appStore.ts` (lines 26-27, 65-66): Zustand store (last-value-wins)
- `web_frontend/src/ros/connection.ts` (lines 104-119): Connection/reconnection handlers

### State Messages
- `ros_ws/src/warehouser_msgs/msg/WorldState.msg` (lines 1-7): State message definition
- `ros_ws/src/warehouser_msgs/msg/Entity.msg` (lines 1-36): Entity structure

### State Services
- `ros_ws/src/warehouser_msgs/srv/RLStep.srv` (lines 1-19): Training step service
- `ros_ws/src/warehouser_msgs/srv/RLReset.srv` (lines 1-15): Training reset service
- `ros_ws/src/warehouser_msgs/srv/SimStep.srv`: Simulation step service

## Findings Summary

### Architecture
- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp:36-145`: WorldManager correctly implements single source of truth pattern
- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp:156-168`: State published at 50 Hz in tick() method
- `training/training/envs/ros_env.py:124-211`: Training uses synchronous services, not state subscription (good separation of concerns)

### QoS Configuration
- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp:122-124`: Missing TRANSIENT_LOCAL durability for late-joining subscribers
- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp:122-124`: Depth=10 wastes memory; should be depth=1 for state snapshots
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:39-42`: Subscriber uses default QoS (missing explicit configuration)
- `ros_ws/src/warehouser_observations/src/observations_node.cpp:43-46`: Subscriber uses default QoS (missing explicit configuration)

### State Consistency
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:232`: Service response overwrites curr_world_, preventing race conditions
- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp:163-168`: Brittle polling loop for state update after reset
- `ros_ws/src/warehouser_msgs/msg/WorldState.msg:1-7`: No sequence ID or version number for consistency validation

### State Recovery
- `web_frontend/src/ros/connection.ts:104-119`: Good reconnection logic with exponential backoff
- `web_frontend/src/ros/connection.ts:127-169`: Will lose state during disconnection (Volatile durability issue)
- `ros_ws/src/warehouser_simulation/src/world_manager.cpp:81-106`: Reset to initial state only, no arbitrary checkpoint save/load

### Timing
- `ros_ws/src/warehouser_observations/src/observations_node.cpp:18-20`: Intentional temporal aliasing (20 Hz obs, 10 Hz lidar, 50 Hz odom)
- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp:164-167`: Clock publication enables sim time (good for determinism)

### Frontend
- `web_frontend/src/store/appStore.ts:65-66`: Correct last-value-wins pattern for read-only frontend
- `web_frontend/src/ros/connection.ts:36-61`: Well-implemented reconnection with backoff and jitter

## Proposal

### Priority 1: Fix QoS Configuration

**Change world state publisher to TRANSIENT_LOCAL:**

File: `ros_ws/src/warehouser_simulation/src/simulation_node.cpp:122-124`

Current:
```cpp
state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
    "/world/state", 10);
```

Recommended:
```cpp
auto state_qos = rclcpp::QoS(1)
    .reliability(RMW_QOS_POLICY_RELIABILITY_RELIABLE)
    .durability(RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL)
    .keep_last(1);
state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
    "/world/state", state_qos);
```

**Benefits:**
- Late-joining frontends get current state immediately
- Reduces memory usage (depth=1 instead of 10)
- Aligns with research recommendations

### Priority 2: Add State Versioning

**Add sequence number to WorldState message:**

File: `ros_ws/src/warehouser_msgs/msg/WorldState.msg`

Add:
```
uint64 sequence
```

File: `ros_ws/src/warehouser_simulation/src/world_manager.cpp`

Track and increment sequence in `toMsg()`.

**Benefits:**
- Consumers can detect dropped messages
- Enables discontinuity detection
- Supports state consistency validation

### Priority 3: Implement Checkpoint System

**Add save/load state services:**

New messages:
- `SaveCheckpoint.srv` - request: `string name`, response: `bool success`
- `LoadCheckpoint.srv` - request: `string name`, response: `bool success, WorldState state`

**Implementation in WorldManager:**
- Serialize entire world state (entity positions, velocities, RNG state)
- Save to file or memory
- Restore exact state on load

**Benefits:**
- Curriculum learning (start from advanced scenarios)
- Debugging specific episodes
- Regression testing with deterministic scenarios

### Priority 4: Replace Brittle Polling with Service Response

**Current issue** (rl_bridge_node.cpp:163-168):
```cpp
for (int i = 0; i < 50 && rclcpp::ok(); ++i) {
    rclcpp::spin_some(shared_from_this());
    rate.sleep();
    if (world_received_) break;
}
```

**Recommendation**: Modify `/sim/reset` service to return final WorldState in response (like `/sim/step` does).

File: `ros_ws/src/warehouser_msgs/srv/SimReset.srv` (create if doesn't exist):
```
# Request
int64 seed 0
---
# Response
bool success
string message
warehouser_msgs/WorldState state  # Add this
```

**Benefits:**
- Eliminates timing dependency
- More robust
- Consistent with SimStep service pattern

### Priority 5: Add Frontend State Interpolation

**File:** `web_frontend/src/store/appStore.ts`

Add middleware to interpolate between 50 Hz state updates for smooth 60 fps rendering.

**Pattern:**
```typescript
interface InterpolatedState {
  previous: WorldState
  current: WorldState
  timestamp: number
}

// In render loop
const alpha = (renderTime - prevTime) / (currTime - prevTime)
const interpolated = lerp(previous, current, alpha)
```

**Benefits:**
- Smoother visualization
- Reduces perceived jitter
- Better user experience

### Priority 6: Add State Monitoring

**Add ROS2 QoS event callbacks** to detect dropped messages:

```cpp
state_sub_->set_on_requested_deadline_missed_callback(
    [this](rclcpp::QOSRequestedDeadlineMissedInfo &) {
        RCLCPP_WARN(get_logger(), "Missed world state deadline");
    });
```

**Add metrics tracking:**
- Publication latency
- Dropped message count
- Subscriber count
- Message size

**Benefits:**
- Early detection of synchronization issues
- Performance monitoring
- Debugging support

## Conclusion

The current state synchronization implementation is **fundamentally sound**:
- Clear authoritative source (WorldManager)
- Appropriate pattern selection (pub/sub for state, services for mutations)
- No race conditions or consistency violations

However, there are **important gaps** versus best practices:
1. Missing TRANSIENT_LOCAL durability for late-joining subscribers
2. No state versioning or sequence tracking
3. No checkpoint/restore for training reproducibility
4. Brittle polling pattern in reset handler
5. No frontend interpolation for smooth rendering

These gaps are **addressable through incremental improvements** without requiring architectural changes. The system is well-positioned to scale to multi-robot scenarios with the existing design.
