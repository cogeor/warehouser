# Introspect: WebSocket Real-Time Communication

Created: 2026-02-12

## Focus

Analysis of WebSocket communication patterns used for real-time robot visualization and control in the Warehouser system. This covers the ROS2-to-frontend bridge architecture, connection management, message handling, and error recovery mechanisms.

## Architecture Overview

### ROS Bridge Setup

**Bridge Type:** `rosbridge_server` (standard ROS bridge WebSocket implementation)

**Launch Configuration:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_bringup\launch\demo.launch.py:64-70`
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_bringup\launch\full_system.launch.py:75-82`
- Node: `rosbridge_websocket`
- Port: `9090` (hardcoded)
- No QoS configuration visible
- No compression settings specified

**Dependencies:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_bringup\package.xml:21` - `rosbridge_server` is an exec dependency
- `C:\Users\costa\src\warehouser\run.md:34` - Installation via apt: `ros-jazzy-rosbridge-server`

### Frontend WebSocket Client

**Library:** `roslibjs` v1.3.0 (C:\Users\costa\src\warehouser\web_frontend\package.json:20)

**Connection Management:** `C:\Users\costa\src\warehouser\web_frontend\src\ros\connection.ts` (246 lines)

**State Management:** Zustand store at `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.ts`

## Connection Management

### Initial Connection

**File:** `C:\Users\costa\src\warehouser\web_frontend\src\ros\connection.ts:97-120`

- Connection URL: `ws://localhost:9090` (hardcoded)
- Initialized once on app mount via `useEffect` in `App.tsx:12-14`
- Single global ROSLIB.Ros instance stored in module-level variable

### Reconnection Strategy

**Implementation:** `C:\Users\costa\src\warehouser\web_frontend\src\ros\connection.ts:7-95`

**Algorithm:** Exponential backoff with jitter
- Base delay: 1000ms
- Max delay: 30000ms (30 seconds)
- Factor: 2x per attempt
- Jitter: ±10% randomization
- Max attempts: 10

**Key Functions:**
- `calculateBackoffDelay()` (line 19): Implements exponential backoff calculation
- `scheduleReconnect()` (line 36): Handles automatic reconnection attempts
- `resetReconnectionState()` (line 66): Clears reconnection state on success
- `retryConnection()` (line 80): Manual retry for user-initiated reconnection

**Event Handling:**
- `connection` event (line 104): Successful connection → reset attempts, subscribe to topics
- `error` event (line 111): Logs error to console only
- `close` event (line 115): Triggers reconnection schedule

**State Tracking:**
- `reconnectAttempt` counter stored in Zustand
- `connectionError` message displayed to user
- `connected` boolean flag

### Connection Cleanup

**Gap:** No explicit cleanup/unsubscribe logic found
- No cleanup in `useEffect` return
- Topics are subscribed but never unsubscribed
- Could cause memory leaks on hot reload or component unmount

## Message Flow

### ROS2 Publishers (Backend)

**World State:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\src\simulation_node.cpp:122-123,161`
- Topic: `/world/state`
- Type: `warehouser_msgs/WorldState`
- Rate: 50 Hz (dt=0.02s, line 50 of simulation_node.hpp)
- Published every tick in timer callback

**Lidar Debug:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\src\observations_node.cpp:55-56,75`
- Topic: `/observations/lidar_debug`
- Type: `warehouser_msgs/LidarDebug`
- Rate: 10 Hz configurable (default, line 19)
- Published via timer callback

**Task Status:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\src\task_manager_node.cpp:37`
- Topic: `/task/status`
- Type: `warehouser_msgs/TaskStatus`
- Rate: Event-driven (state machine transitions)

### Frontend Subscriptions

**File:** `C:\Users\costa\src\warehouser\web_frontend\src\ros\connection.ts:122-193`

**1. World State** (lines 127-169)
- Topic: `/world/state`
- Updates: Entity positions, simulation time
- Processing: Type mapping from numeric enum to string literal
- State update: `store.setEntities()`, `store.setSimTime()`

**2. Lidar Debug** (lines 172-181)
- Topic: `/observations/lidar_debug`
- Updates: Lidar ranges, angle parameters
- State update: `store.setLidar()`

**3. Task Status** (lines 184-193)
- Topic: `/task/status`
- Updates: Task state machine status, intent
- State update: `store.setTaskStatus()`

### Message Type Handling

**Type Safety Issues:**
- All messages typed as `unknown`, then type-asserted
- No runtime validation of message structure
- Missing fields could cause silent errors

**Example from WorldState handler (line 133):**
```typescript
worldStateTopic.subscribe((msg: unknown) => {
  const message = msg as { entities: unknown[]; sim_time: number }
  // Direct type assertion without validation
```

**Type Mapping Logic (lines 148-156):**
- Numeric entity types (0-3) mapped to string literals
- Fallback to 'object' if type unknown
- Hardcoded typeMap in subscription callback

### Frontend Publishers

**1. Command Publishing** (lines 216-230)
- Topic: `/command/json`
- Type: `std_msgs/String`
- Usage: Sends JSON commands (e.g., pick actions)
- No acknowledgment or response handling

**2. Move Entity** (lines 232-246)
- Topic: `/sim/move_entity`
- Type: `std_msgs/String`
- Usage: Drag-and-drop entity repositioning
- Triggered from Canvas drag events (Canvas.tsx:321-323)

**3. Service Calls** (lines 196-214)
- Function: `callService(name: string): Promise<boolean>`
- Type: `std_srvs/Trigger`
- Services used: `/sim/start`, `/sim/pause`, `/sim/reset`
- Returns promise with success boolean

## Performance Characteristics

### Message Frequency

**Backend Rates:**
- World State: 50 Hz (20ms interval) - HIGH FREQUENCY
- Lidar Debug: 10 Hz (100ms interval)
- Odometry: 50 Hz (configurable, observations_node.cpp:20)
- Task Status: Event-driven (low frequency)

**Estimated Bandwidth:**
- WorldState: ~50 entities × 50 Hz = high update rate
- No throttling on frontend
- No message compression configured
- Full state transmitted every tick (no delta compression)

### Latency Considerations

**Potential Issues:**
- No latency measurements or tracking
- No timestamp comparison between ROS and frontend
- Animation interpolation used (Canvas.tsx:18-19): 80ms duration, may hide latency
- No explicit buffering or prediction

**Animation System (Canvas.tsx):**
- Uses Konva animations with 80ms duration
- Easing function: `Konva.Easings.EaseOut`
- Smooths visual updates but could mask underlying latency

### Rendering Performance

**Canvas Update Strategy:**
- Entity refs stored in `Map<string, Konva.Node>`
- Animated position updates via `node.to()` method
- Skip initial animation to prevent jarring startup
- Separate animation effects for robot, objects, lidar

**Optimization Patterns:**
- Position updates use refs, not full re-renders
- Initialization flags prevent animation on first render
- Object cleanup when entities removed (Canvas.tsx:197-203)

## Error Handling

### Connection Errors

**Current Implementation:**
- `connection.ts:111-113` - Logs errors to console only
- No differentiation between error types
- No user notification of connection errors (except disconnect)

**Reconnection Errors:**
- Max attempts tracked (10 limit)
- Error message set after max attempts
- User can manually retry via StatusPanel click

### Message Processing Errors

**Missing Error Handling:**
- No try-catch around message parsing
- No validation of message structure
- Type assertions could throw if structure mismatched
- Silent failures possible with malformed messages

**Example Risk (connection.ts:135-166):**
```typescript
const entity = e as {
  id: string
  type: number
  x: number
  // ... missing field checks
}
```
If any field is undefined, could cause downstream errors.

### Service Call Errors

**Current Implementation (lines 196-214):**
- Service calls resolve promise with boolean
- No error handling for service call failures
- No timeout specified
- Network errors not caught

## Findings

### 1. Hardcoded Configuration

**Issues:**
- WebSocket URL hardcoded: `ws://localhost:9090`
- Port number duplicated in two launch files
- No environment variable support
- Cannot configure for remote servers

**Locations:**
- `connection.ts:101, 58, 91`
- `demo.launch.py:68`
- `full_system.launch.py:80`

### 2. No QoS Configuration

**Missing:**
- No QoS profiles specified on ROS publishers
- No reliability settings (reliable vs best-effort)
- No durability settings (transient-local vs volatile)
- Default QoS may not be optimal for visualization

**Impact:**
- May lose messages under network stress
- No guarantees on message delivery
- Cannot tune for latency vs reliability trade-off

### 3. Message Type Safety

**Issues:**
- All messages typed as `unknown`, cast without validation
- No runtime schema validation (e.g., Zod, io-ts)
- Type definitions hardcoded in subscription callbacks
- No shared type definitions between ROS messages and TypeScript

**Locations:**
- `connection.ts:133` (WorldState)
- `connection.ts:178` (LidarDebug)
- `connection.ts:190` (TaskStatus)

### 4. No Message Compression

**Gap:**
- rosbridge supports compression (CBOR, PNG for images)
- Not configured in launch files
- High-frequency world state could benefit from compression
- Frontend sends JSON strings (not optimal)

### 5. Connection Cleanup Missing

**Issues:**
- `connection.ts:97-120` - No cleanup function
- Topics subscribed in `subscribeToTopics()` never unsubscribed
- ROSLIB.Ros instance never closed
- Could cause memory leaks

**Recommendation:** Add cleanup to useEffect:
```typescript
useEffect(() => {
  initRosConnection()
  return () => {
    // Unsubscribe from topics
    // Close connection
  }
}, [])
```

### 6. Error Handling Gaps

**Issues:**
- Connection errors only logged (line 111-113)
- No message validation errors caught
- Service call failures not handled
- No retry logic for failed publishes

### 7. No Latency Monitoring

**Missing:**
- No timestamp tracking
- No roundtrip time measurement
- No latency metrics displayed
- Animation may mask latency issues

### 8. State Management Pattern

**Issues:**
- Full state replacement on every message (no delta updates)
- WorldState contains all entities, transmitted every 20ms
- No message batching or aggregation
- Could be inefficient with many entities

### 9. Reconnection UX

**Good Practices:**
- Exponential backoff with jitter implemented correctly
- User-visible reconnection attempt counter
- Manual retry button (StatusPanel.tsx:26-28)
- Connection status displayed (App.tsx:20-22)

**Issues:**
- Max attempts hardcoded (not configurable)
- No "reconnecting" indicator shown immediately on disconnect
- Error display only after max attempts

### 10. No Message Throttling

**Gap:**
- Frontend receives all messages at full rate
- No client-side throttling (e.g., lodash.throttle)
- State updates on every message
- Could cause performance issues with high message rates

## Proposal

### High Priority

1. **Add Message Validation**
   - Implement runtime schema validation for all message types
   - Use Zod or similar for type-safe validation
   - Handle validation errors gracefully
   - Location: `connection.ts:133-193`

2. **Implement Connection Cleanup**
   - Add cleanup function to unsubscribe topics
   - Close ROSLIB.Ros connection on unmount
   - Prevent memory leaks
   - Location: `connection.ts:97-120`, `App.tsx:12-14`

3. **Add Error Handling**
   - Wrap message handlers in try-catch
   - Handle service call failures
   - Display errors to user
   - Location: `connection.ts:111-113, 196-214`

4. **Make Configuration Externalized**
   - WebSocket URL from environment variable
   - Port configuration in single location
   - Support for remote servers
   - Location: `connection.ts:101`, launch files

### Medium Priority

5. **Add Message Compression**
   - Configure rosbridge to use CBOR compression
   - Reduce bandwidth for high-frequency messages
   - Location: launch files parameters

6. **Implement Client-Side Throttling**
   - Throttle state updates for high-frequency topics
   - Use requestAnimationFrame for rendering updates
   - Location: `connection.ts:133-193`

7. **Add Latency Monitoring**
   - Track message timestamps
   - Calculate and display roundtrip time
   - Add performance metrics panel
   - Location: new file or `StatusPanel.tsx`

8. **Improve Type Safety**
   - Generate TypeScript types from ROS messages
   - Use ros2-web-bridge type generation
   - Share type definitions
   - Location: new types file

### Low Priority

9. **Add QoS Configuration**
   - Configure QoS profiles on ROS publishers
   - Match frontend expectations
   - Document QoS choices
   - Location: C++ node constructors

10. **Implement Delta Updates**
    - Send only changed entities instead of full state
    - Reduce bandwidth usage
    - Requires backend changes
    - Location: `simulation_node.cpp`, frontend state handling

## Summary

The WebSocket communication architecture uses standard rosbridge_server with roslibjs frontend. Connection management is well-implemented with exponential backoff reconnection. However, there are significant gaps in error handling, message validation, resource cleanup, and performance optimization. The system works for the current use case but lacks robustness for production deployment or scaling to more entities/robots.

**Key Strengths:**
- Well-structured reconnection logic with backoff
- Clean separation of concerns (connection, state, components)
- User-visible connection status and retry

**Key Weaknesses:**
- No message validation or type safety
- Missing connection cleanup (memory leaks)
- Hardcoded configuration (not flexible)
- No compression or throttling (bandwidth inefficient)
- Limited error handling (poor resilience)
