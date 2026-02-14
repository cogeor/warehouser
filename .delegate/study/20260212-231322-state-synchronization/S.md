# Search: State Synchronization Patterns for Robotics Systems

Created: 2026-02-12T23:15:00Z

## Queries Executed

1. "ROS2 QoS policies state synchronization transient local durability best practices 2026"
2. "distributed state synchronization patterns robotics simulation CRDT event sourcing 2026"
3. "real-time frontend state synchronization WebSocket optimistic updates reconciliation patterns 2026"

## Findings

### 1. ROS2 QoS Policies for State Synchronization

**Key QoS Policies for State Management:**

- **Transient Local Durability**: Publishers persist samples for late-joining subscribers. Replaces ROS1 "latching" publishers. Critical for state topics where newcomers need the current state immediately.
- **Reliable Delivery**: Ensures message delivery with retransmission. Essential for state synchronization but increases CPU/bandwidth usage.
- **History Depth**: Controls how many messages are persisted. Use `KEEP_LAST` with depth=1 for current state, or higher depths for state history.

**Recommended QoS Profiles by Use Case:**

| Use Case | Reliability | Durability | History | Depth | Rationale |
|----------|-------------|------------|---------|-------|-----------|
| World State Snapshot | Reliable | Transient Local | Keep Last | 1 | Late-joining subscribers get current state |
| Mission Plan | Reliable | Transient Local | Keep All | N/A | Preserves full plan for disconnected robots |
| High-Frequency Sensor | Best Effort | Volatile | Keep Last | 10 | Low latency, tolerate drops |
| Robot Description | Reliable | Transient Local | Keep Last | 1 | Published once, needed by all nodes |

**QoS Compatibility Rules:**

- Both publisher and subscriber must agree on `Transient Local` for late-joining to work
- "Request vs Offered" model: Subscriber requests minimum quality, publisher offers maximum quality
- Publishers with `Best Effort` or `Volatile` do not connect to subscribers requesting `Reliable` or `Transient Local`

**Code Example (Python/rclpy):**

```python
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy

# For world state snapshots
state_qos = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
    depth=1
)
node.create_subscription(WorldState, '/world_state', callback, state_qos)
```

**Performance Considerations:**

- Reliable QoS increases CPU and bandwidth usage
- High history depth consumes memory - keep depth small unless explicitly needed
- Lifespan policy only applies to `Transient Local` durability
- Default ROS2 settings: Keep Last (depth=10), Reliable, Volatile, System Default liveliness

**Sources:**
- [ROS 2 Quality of Service policies](https://design.ros2.org/articles/qos.html)
- [Quality of Service settings - ROS 2 Documentation](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
- [Mastering ROS 2 QoS Profiles](https://medium.com/@ultroninverse/mastering-ros-2-qos-profiles-a-practical-field-guide-on-reliability-latency-scalability-b3562eb70a26)
- [Manage Quality of Service Policies in ROS 2](https://www.mathworks.com/help/ros/ug/manage-quality-of-service-policies-in-ros2.html)

### 2. Distributed State Patterns: CRDT and Event Sourcing

**CRDTs (Conflict-free Replicated Data Types):**

CRDTs provide eventual consistency without coordination. Two types exist:
- **Operation-based CRDTs**: Propagate operations (events) that must be commutative
- **State-based CRDTs**: Propagate full state and merge using lattice properties

**CRDT + Event Sourcing Integration:**

Event sourcing plays nicely with CRDT requirements. Operations are persisted as events and replayed on demand. For replicated event sourcing, operation-based CRDTs are a good fit since events represent operations.

**Key Properties:**

- Operations must be commutative: applying the same events in any order produces the same final state
- Enables optimizations like storing CRDT snapshots and replaying from the snapshot
- Can use single highly-available replicated event log, or per-replica event logs (sharding)

**Robotics Applications - CORTEX Framework:**

The CORTEX working memory for robotics uses a three-layered CRDT design:
1. **DDS middleware**: Provides reliable multicast
2. **CRDT graph**: Provides eventual consistency
3. **High-level API**: Simple interface to edit graph elements

**Use Case:** Synchronizing physics simulator state with working memory for cognitive robotics. Acquires predicates establishing relationships among objects in the current scene.

**Distributed Systems Architecture:**

- Each node represented as independent actor with encapsulated state
- State modified only through message passing
- Ensures state isolation, reducing complexity of shared mutable state

**Real-Time Synchronization Research (2026):**

Hybrid models incorporate:
- Operational Transformation (OT) for conflict resolution
- Edge-deployed synchronization buffers
- Balance between latency, consistency, and scalability
- Event-driven architectures with pub-sub systems (WebSockets, MQTT)

**Sources:**
- [Replicated Event Sourcing - Akka](https://doc.akka.io/libraries/akka-core/current/typed/replicated-eventsourcing.html)
- [CRDT Tutorial for Beginners](https://github.com/ljwagerfield/crdt)
- [Operation based CRDTs: protocol](https://www.bartoszsypytkowski.com/operation-based-crdts-protocol/)
- [Efficient State Synchronization in Distributed Electrical Grid Systems Using CRDTs](https://www.mdpi.com/2624-831X/6/1/6)
- [Enhancing Robotic Perception through Synchronized Simulation](https://pmc.ncbi.nlm.nih.gov/articles/PMC11014409/)

### 3. Frontend State Synchronization: Optimistic Updates and Reconciliation

**WebSocket State Synchronization (2026):**

WebSocket remains the cornerstone for real-time communication in 2026, with expanded use cases in collaborative tools, IoT, and gaming. Modern tools include:
- websockets (v16.0)
- FastAPI (v2026)
- Quart

Performance benchmarks: 10,000+ concurrent connections per server instance with sub-100ms latency.

**Optimistic Updates Pattern:**

Optimistic UI updates assume user actions will succeed and immediately reflect changes in the UI without waiting for backend confirmation. This provides a snappy, responsive user experience.

**Reconciliation Strategy:**

If the operation fails, the system rolls back the UI to its previous state. Systems lean on optimistic synchronization to provide instant UI updates, assuming success and reconciling when acknowledgments arrive.

**Real-Time Synchronization Pipeline:**

1. Frontend sends update via persistent connection (WebSocket)
2. Backend authenticates, validates, and updates central data store
3. Updated data/events broadcast to subscribed clients via pub/sub pattern
4. Connected clients receive and apply changes immediately, refreshing UI
5. Pipeline runs with minimal latency for smooth, live experience

**Conflict Resolution Strategies:**

**Optimistic Concurrency Control:**
- Version numbers detect conflicts
- Client updates include version check
- Server rejects if version mismatch or requires manual merging

**CRDT-based Resolution (Automatic):**
- Libraries: Yjs, Automerge
- Enable concurrent updates to merge seamlessly without losing data
- Best for apps requiring automatic conflict resolution

**Operational Transformation (OT):**
- Ensures consistency by transforming simultaneous operations
- More complex to implement manually than CRDTs

**Best Practices for Real-Time State Sync:**

1. **Optimistic UI Updates**: Update interface immediately, validate in background
2. **Exponential Backoff Retry**: Gradually increase wait time between retries during outages
3. **Clear UI Indicators**: Show "syncing", "offline", or "conflict detected" states
4. **Connection Management**: Implement reconnection logic with state recovery
5. **Performance**: Achieve sub-100ms latency, 99.99% uptime for production systems

**State Management Patterns:**

- **Server State vs Client State**: Separate concerns - server state is source of truth, client state is local UI state
- **Zustand/Redux Patterns**: Use middleware for WebSocket integration, action queuing, and optimistic updates
- **State Reconciliation**: Merge server state with optimistic local changes on acknowledgment

**Sources:**
- [Python WebSocket Servers: Real-Time Communication Patterns](https://dasroot.net/posts/2026/02/python-websocket-servers-real-time-communication-patterns/)
- [Real-time data synchronization between backend and frontend](https://www.zigpoll.com/content/can-you-explain-how-your-backend-handles-realtime-data-synchronization-for-the-frontend-interface)
- [Solving eventual consistency in frontend](https://blog.logrocket.com/solving-eventual-consistency-frontend/)
- [How to Use WebSockets in React for Real-Time Applications](https://oneuptime.com/blog/post/2026-01-15-websockets-react-real-time-applications/view)
- [Synchronizing state with Websockets and JSON Patch](https://cetra3.github.io/blog/synchronising-with-websocket/)
- [WebSocket architecture best practices](https://ably.com/topic/websocket-architecture-best-practices)

### 4. Multi-Client Synchronization Patterns

**Authoritative State Source:**

- Central server maintains authoritative state
- All updates flow through central authority
- Server broadcasts state changes to all connected clients
- Prevents split-brain scenarios

**Pub/Sub Pattern:**

- Clients subscribe to state updates
- Server publishes changes to all subscribers
- Supports multiple simultaneous viewers
- Scales well with connection pooling and message broadcasting

**Conflict Resolution:**

- **Last-Write-Wins (LWW)**: Simple but can lose data
- **Version Vectors**: Track causality for concurrent updates
- **Application-Specific Logic**: Custom merge strategies based on domain knowledge
- **CRDT Merge**: Automatic conflict-free merging for supported data types

**Consistency Models:**

- **Strong Consistency**: All clients see same state at same time (high latency)
- **Eventual Consistency**: Clients converge to same state over time (low latency)
- **Causal Consistency**: Preserve cause-effect relationships (middle ground)

**For Warehouser Context:**

- Single authoritative simulation node
- Multiple visualization clients (frontend dashboards)
- Training client receives state but doesn't modify it
- No write conflicts - simulation is single writer

### 5. Simulation State: Deterministic Replay and Checkpointing

**Deterministic Replay Requirements:**

- Fixed random seed for reproducibility
- Event sourcing: record all inputs/commands
- Replay events in same order
- Ensures same final state for testing/debugging

**Checkpoint/Restore Patterns:**

- **Full State Snapshots**: Serialize entire world state at intervals
- **Incremental Checkpoints**: Store only deltas from previous checkpoint
- **Copy-on-Write**: Efficient memory usage for similar states

**Time Synchronization:**

- Use ROS2 time abstraction (`rclcpp::Time`, `rclpy.Time`)
- Support sim time vs wall time
- Synchronize step cadence across distributed nodes
- Use simulation clock for deterministic replay

**Use Cases for Warehouser:**

- Save/restore training episodes
- Debug specific scenarios
- Parallel training with different starting states
- Regression testing with deterministic scenarios

### 6. Gaming/Real-Time Patterns: Client-Side Prediction and Server Reconciliation

**Client-Side Prediction:**

- Client simulates input immediately for responsive feel
- Server validates and computes authoritative state
- Client reconciles prediction with server state

**Server Reconciliation:**

- Server broadcasts authoritative state updates
- Client compares predicted state with server state
- If mismatch: rewind and replay inputs from server state
- Smooth interpolation to hide discontinuities

**Lag Compensation:**

- **Rewind Time**: Server rewinds world state to client's timestamp for hit detection
- **Interpolation**: Client displays slightly delayed state, interpolates between snapshots
- **Extrapolation**: Predict future state based on velocity/acceleration (risky)

**Not Directly Applicable to Warehouser:**

Warehouser has authoritative simulation without client prediction:
- Frontend is pure visualization (no input affecting state)
- Training client observes but doesn't predict
- No need for lag compensation

**Potentially Useful:**

- **Interpolation**: Frontend could interpolate between 50Hz state updates for smooth 60fps rendering
- **Extrapolation**: Dead reckoning for robot position between updates

## Cloned

No repositories cloned for this research.

## Synthesis and Recommendations for Warehouser

Based on the research, here are specific recommendations for Warehouser's state synchronization architecture:

### 1. ROS2 World State Topic

**Recommended QoS:**

```python
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy

world_state_qos = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
    depth=1
)
```

**Rationale:**
- `RELIABLE`: Ensures training client and frontend receive every state update
- `TRANSIENT_LOCAL`: Late-joining frontends get current state immediately
- `KEEP_LAST` depth=1: Only current state matters, reduces memory usage
- 50Hz publication rate is manageable with Reliable QoS

### 2. High-Frequency Observation Data (Lidar)

**Recommended QoS:**

```python
lidar_qos = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.VOLATILE,
    history=HistoryPolicy.KEEP_LAST,
    depth=5
)
```

**Rationale:**
- `BEST_EFFORT`: Lower latency, tolerate occasional drops
- `VOLATILE`: No need for late-joining persistence
- Depth=5: Small buffer for burst tolerance

### 3. Frontend State Management Architecture

**Pattern:** Zustand + WebSocket Bridge

```typescript
interface SimulationState {
  // Server state (authoritative)
  worldState: WorldState | null;
  connectionStatus: 'connecting' | 'connected' | 'disconnected';
  lastUpdateTime: number;

  // Client state (local UI)
  selectedRobot: string | null;
  cameraPosition: Vec3;

  // Actions
  updateWorldState: (state: WorldState) => void;
  setConnectionStatus: (status: string) => void;
}
```

**WebSocket Integration:**
- Subscribe to ROS2 topic via rosbridge_suite or custom WebSocket server
- Push state updates to Zustand store
- Optimistic updates not needed (read-only frontend)
- Interpolation middleware for smooth 60fps rendering from 50Hz data

### 4. Training Client State Handling

**Pattern:** Synchronous ROS2 Service Calls

Training already uses synchronous step/reset services - this is correct pattern:
- Training client sends action via service call
- Simulation processes action, updates world state
- Service response includes new observation
- No optimistic updates needed (training is sequential)

**Checkpointing for Training:**
- Implement save/load state services
- Serialize full world state (entity positions, velocities, object states)
- Use for curriculum learning, debugging specific scenarios
- Store checkpoints with metadata (episode number, cumulative reward)

### 5. Multi-Client Visualization

**Pattern:** Pub/Sub with Authoritative Simulation

- Simulation node is single writer (authoritative state source)
- Frontend clients are pure subscribers (no state modifications)
- No conflict resolution needed
- Use ROS2 Transient Local for late-joining viewers

**Scalability:**
- ROS2 DDS layer handles multi-client broadcasting
- Consider rosbridge_suite WebSocket server for browser clients
- Load balancing: one rosbridge instance can handle 1000+ WebSocket connections

### 6. Time Synchronization and Determinism

**Recommendations:**

- Use ROS2 sim time for training reproducibility
- Publish clock updates on `/clock` topic
- Training episodes use fixed random seed + sim time for determinism
- Checkpoint includes RNG state for perfect replay

**Implementation:**

```cpp
// In simulation node
rclcpp::Clock::SharedPtr sim_clock_;
auto clock_msg = rosgraph_msgs::msg::Clock();
clock_msg.clock = sim_clock_->now();
clock_publisher_->publish(clock_msg);
```

### 7. State Reconciliation Strategy

**For Warehouser Context:**

Since simulation is authoritative and clients are read-only:
- **No reconciliation needed** for state conflicts
- Frontend uses **last-value-wins** for state updates
- Training uses **synchronous request-response** (implicit consistency)

**Connection Loss Handling:**

Frontend:
- Display "disconnected" state clearly
- Attempt reconnection with exponential backoff
- Request full state snapshot after reconnection (Transient Local handles this)

Training:
- Fail fast on connection loss
- Let training script handle retry/recovery
- Log episode state for debugging

### 8. Performance Monitoring

**Metrics to Track:**

- State publication latency (simulation → training/frontend)
- WebSocket message rate and size
- QoS dropped messages (for Best Effort topics)
- Memory usage for history buffers

**Tools:**
- `ros2 topic hz /world_state` - measure publication rate
- `ros2 topic bw /world_state` - measure bandwidth
- ROS2 QoS events for monitoring dropped messages

## Proposal

Based on this research, the recommended state synchronization architecture for Warehouser is:

1. **Authoritative Simulation Pattern**: Single simulation node owns world state, clients are read-only subscribers
2. **ROS2 QoS Strategy**: Use Reliable + Transient Local (depth=1) for world state, Best Effort for high-frequency sensor data
3. **Frontend**: WebSocket bridge → Zustand store, interpolation middleware for smooth rendering
4. **Training**: Keep existing synchronous service pattern, add checkpointing for reproducibility
5. **Multi-Client**: ROS2 DDS pub/sub handles multiple viewers automatically, use rosbridge for WebSocket clients
6. **Time**: Use ROS2 sim time with fixed seeds for deterministic training

**Implementation Priority:**
1. Verify current QoS settings align with recommendations
2. Add Transient Local durability to world state topic for late-joining frontends
3. Implement checkpoint/restore services for training reproducibility
4. Add connection status monitoring to frontend
5. Implement interpolation for smooth frontend rendering

**CRDT/Event Sourcing**: Not recommended for Warehouser's current architecture since there's a single authoritative state source with no concurrent writes. Consider for future multi-agent training with peer-to-peer robot coordination.
