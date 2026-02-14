# TASK: Implement Robust State Synchronization with QoS, Versioning, and Recovery

Created: 2026-02-12T23:45:00Z
Build: FAIL (colcon not available in environment)
Tests: FAIL (missing pydantic dependency)

## Summary

Enhance the state synchronization architecture in Warehouser by implementing ROS2 QoS best practices, adding state versioning for consistency validation, and providing checkpoint/restore capabilities for training reproducibility. The current implementation has a solid authoritative server pattern but lacks critical features for production robustness: TRANSIENT_LOCAL durability for late-joining clients, sequence tracking for dropped message detection, and state persistence for curriculum learning.

## Context

### Current Architecture Strengths

**From Introspection (I.md):**
- WorldManager correctly implements single source of truth pattern (world_manager.hpp:36-145)
- State published at 50 Hz with atomic updates (simulation_node.cpp:156-168)
- Training uses synchronous service pattern, avoiding pub/sub race conditions (ros_env.py:124-211)
- Frontend has robust reconnection with exponential backoff (connection.ts:36-61)
- Single-threaded ROS2 executor prevents state divergence

**From Search (S.md):**
- Authoritative server pattern is correct for Warehouser's architecture
- Research validates current approach: single writer, multiple read-only observers
- 50 Hz publication rate is manageable with Reliable QoS
- WebSocket bridge pattern aligns with 2026 best practices

### Critical Gaps Identified

**QoS Configuration Issues (I.md findings):**
1. **Missing TRANSIENT_LOCAL durability** (simulation_node.cpp:122-124)
   - Late-joining frontends don't receive current state
   - Must wait up to 20ms for next tick
   - Can show "empty world" briefly after reconnection

2. **Inefficient history depth** (simulation_node.cpp:122-124)
   - Current: depth=10 wastes memory
   - Recommended: depth=1 (only current state needed)

3. **No explicit QoS on subscribers** (rl_bridge_node.cpp:39-42, observations_node.cpp:43-46)
   - Using defaults that may mismatch publisher
   - Should explicitly match TRANSIENT_LOCAL

**State Consistency Issues:**
4. **No state versioning** (WorldState.msg:1-7)
   - Cannot detect dropped messages
   - No sequence tracking for discontinuity detection
   - Frontend can't validate state continuity

5. **Brittle polling pattern** (rl_bridge_node.cpp:163-168)
   - Reset waits for state update via polling loop
   - Timing-dependent, fragile
   - Should use service response pattern like SimStep

**State Recovery Issues:**
6. **No checkpoint/restore** (world_manager.cpp:81-106)
   - Can only reset to initial state
   - Cannot save/load arbitrary states
   - Blocks curriculum learning use cases
   - No deterministic replay for debugging

**From Template Analysis (T.md):**
- Reference implementations show standard patterns for all gaps
- ROS2 QoS profiles well-documented and battle-tested
- Checkpoint serialization pattern is straightforward
- Frontend version tracking is a common pattern

## Target State

### 1. Production-Grade QoS Configuration

**World State Topic:**
```cpp
auto state_qos = rclcpp::QoS(rclcpp::KeepLast(1))
    .reliable()
    .transient_local();
```

**Benefits:**
- Late-joining clients receive state immediately via DDS persistence
- Memory efficient (depth=1 vs depth=10)
- Aligns with ROS2 best practices for state topics

### 2. State Versioning and Consistency Validation

**Enhanced WorldState Message:**
```
std_msgs/Header header    # Timestamp and frame_id
uint64 version           # Monotonic sequence number
float32 sim_time         # Simulation time (existing)
Entity[] entities        # Entity list (existing)
bool running            # Running state (existing)
```

**Benefits:**
- Detect dropped messages via sequence gaps
- Timestamp correlation for latency measurement
- Enable consistency validation across clients

### 3. Checkpoint/Restore System

**New Services:**
- `/sim/save_checkpoint` - Serialize full world state to file
- `/sim/load_checkpoint` - Restore exact world state from file

**Use Cases:**
- Curriculum learning (start from advanced scenarios)
- Episode replay for debugging specific failures
- Regression testing with deterministic scenarios
- Parallel training with varied initial conditions

### 4. State Recovery and Monitoring

**Enhanced Frontend:**
- Version tracking with missed update alerts
- Performance metrics dashboard
- State rehydration after reconnection via TRANSIENT_LOCAL

**C++ Nodes:**
- QoS event handlers for dropped message detection
- Latency and bandwidth metrics
- Client health monitoring

## Implementation Plan

### Phase 1: QoS Configuration (HIGH PRIORITY)

**Objective:** Fix critical late-joining subscriber issue and optimize memory usage.

**Tasks:**
- [ ] Update world state publisher QoS in simulation_node.cpp
  - Replace line 122-124 with TRANSIENT_LOCAL configuration
  - Change depth from 10 to 1

- [ ] Update world state subscribers with matching QoS
  - rl_bridge_node.cpp:39-42
  - observations_node.cpp:43-46

- [ ] Add high-frequency sensor QoS for lidar debug
  - Use BEST_EFFORT, depth=5 for performance

- [ ] Build and verify QoS compatibility
  - Test late-joining frontend receives state immediately
  - Verify no QoS mismatch warnings in logs

**Verification:**
```bash
# Terminal 1: Start simulation
ros2 run warehouser_simulation simulation_node

# Terminal 2: Check QoS settings
ros2 topic info /world/state --verbose

# Terminal 3: Late-joining subscriber test
sleep 5 && ros2 topic echo /world/state --once
# Should receive state immediately (not wait for next tick)
```

### Phase 2: State Versioning (HIGH PRIORITY)

**Objective:** Add sequence tracking for consistency validation.

**Tasks:**
- [ ] Extend WorldState.msg with header and version
  - Add `std_msgs/Header header`
  - Add `uint64 version`

- [ ] Update simulation_node.cpp tick() method
  - Track `state_version_` member variable
  - Populate header.stamp, header.frame_id, version

- [ ] Add version checking in rl_bridge_node.cpp
  - Log warnings for sequence gaps
  - Track dropped message metrics

- [ ] Add frontend version monitoring
  - Update connection.ts to extract version
  - Track missed versions in appStore.ts
  - Display metrics in debug panel

**Verification:**
```bash
# Monitor version sequence
ros2 topic echo /world/state --field version

# Simulate dropped messages (kill subscriber briefly)
# Verify frontend logs missed version count
```

### Phase 3: Checkpoint/Restore Services (MEDIUM PRIORITY)

**Objective:** Enable state persistence for training reproducibility.

**Tasks:**
- [ ] Create new service definitions
  - SaveCheckpoint.srv (filepath → success/message)
  - LoadCheckpoint.srv (filepath → success/message/state)

- [ ] Implement WorldManager serialization
  - `serializeState()` - JSON serialization of full state
  - `restoreState(json)` - Deserialize and restore
  - Include RNG state for determinism

- [ ] Add checkpoint service handlers in simulation_node.cpp
  - `handleSaveCheckpoint` - calls world_.saveCheckpoint()
  - `handleLoadCheckpoint` - calls world_.loadCheckpoint()

- [ ] Extend training environment with checkpoint support
  - `save_checkpoint(episode, reward)` - Python method
  - `load_checkpoint(episode)` - Python method
  - Store metadata (episode, reward, seed) with checkpoint

**Verification:**
```bash
# Save checkpoint at episode 100
ros2 service call /sim/save_checkpoint \
  warehouser_msgs/srv/SaveCheckpoint "{filepath: '/tmp/ep100.checkpoint'}"

# Load checkpoint
ros2 service call /sim/load_checkpoint \
  warehouser_msgs/srv/LoadCheckpoint "{filepath: '/tmp/ep100.checkpoint'}"

# Verify state matches saved state
```

### Phase 4: State Monitoring and Metrics (MEDIUM PRIORITY)

**Objective:** Add observability for production debugging.

**Tasks:**
- [ ] Add QoS event handlers in C++ nodes
  - Deadline missed callbacks
  - Liveliness changed callbacks
  - Log warnings for QoS violations

- [ ] Implement performance metrics tracking
  - Publication latency (sim → training/frontend)
  - Message rate and bandwidth
  - Subscriber count

- [ ] Add frontend performance dashboard
  - Message rate display
  - Latency histogram
  - Dropped message counter
  - Connection status indicator

- [ ] Add ROS2 diagnostic publishers
  - `/diagnostics` topic for health monitoring
  - Integration with rqt_runtime_monitor

**Verification:**
```bash
# Monitor QoS events
ros2 topic echo /diagnostics

# Check metrics in frontend
# Open browser console, verify metrics object updates

# Use ROS2 tools
ros2 topic hz /world/state
ros2 topic bw /world/state
```

### Phase 5: Remove Brittle Polling Pattern (LOW PRIORITY)

**Objective:** Replace timing-dependent reset synchronization.

**Tasks:**
- [ ] Extend SimReset service response
  - Add `WorldState state` to response
  - Consistent with SimStep pattern

- [ ] Update handleReset in simulation_node.cpp
  - Return final state in response

- [ ] Remove polling loop in rl_bridge_node.cpp
  - Replace lines 163-168 with service response usage
  - Use returned state directly

- [ ] Update training reset logic
  - Verify service response includes state

**Verification:**
```bash
# Reset and verify response includes state
ros2 service call /sim/reset std_srvs/srv/Trigger

# Check RL bridge logs
# Should NOT see "waiting for state update" polling messages
```

### Phase 6: Frontend Interpolation (FUTURE)

**Objective:** Smooth 60fps rendering from 50Hz updates.

**Tasks:**
- [ ] Add interpolation middleware in appStore.ts
  - Store previous and current state
  - Calculate interpolation alpha from timestamps

- [ ] Implement entity position interpolation
  - Linear interpolation for positions
  - Slerp for orientations

- [ ] Add render loop with requestAnimationFrame
  - 60fps rendering independent of state updates
  - Smooth visualization

- [ ] Add toggle for interpolation on/off
  - Debug mode to see raw state updates

**Verification:**
```javascript
// In browser console
store.getState().metrics.renderFps
// Should show 60fps with smooth motion
```

## Interface Definitions

### Enhanced WorldState Message

**File:** `ros_ws/src/warehouser_msgs/msg/WorldState.msg`

```
std_msgs/Header header
uint64 version
float32 sim_time
Entity[] entities
bool running
```

### Checkpoint Services

**File:** `ros_ws/src/warehouser_msgs/srv/SaveCheckpoint.srv`

```
string filepath
---
bool success
string message
```

**File:** `ros_ws/src/warehouser_msgs/srv/LoadCheckpoint.srv`

```
string filepath
---
bool success
string message
warehouser_msgs/WorldState state
```

### Enhanced SimReset Service (Optional)

**File:** `ros_ws/src/warehouser_msgs/srv/SimReset.srv`

```
int64 seed 0
---
bool success
string message
warehouser_msgs/WorldState state
```

### Frontend State Interface

**File:** `web_frontend/src/store/appStore.ts`

```typescript
interface PerformanceMetrics {
  messageRate: number;
  latency: number;
  droppedMessages: number;
  lastMeasurement: number;
}

interface AppState {
  // Existing fields...
  entities: Entity[];
  simTime: number;
  connected: boolean;

  // New fields
  lastStateVersion: number;
  missedVersions: number;
  metrics: PerformanceMetrics;

  // New methods
  setWorldState: (entities: Entity[], version: number, timestamp: number) => void;
  updateMetrics: (receivedAt: number, serverTimestamp: number) => void;
}
```

## Files to Create

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_msgs/srv/SaveCheckpoint.srv` | Service definition for saving checkpoints |
| `ros_ws/src/warehouser_msgs/srv/LoadCheckpoint.srv` | Service definition for loading checkpoints |
| `ros_ws/src/warehouser_msgs/srv/SimReset.srv` | Enhanced reset service with state response |
| `docs/state-synchronization.md` | Architecture documentation for state sync patterns |

## Files to Modify

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_msgs/msg/WorldState.msg` | Add header and version fields |
| `ros_ws/src/warehouser_simulation/src/simulation_node.cpp` | Update QoS, add versioning, add checkpoint services |
| `ros_ws/src/warehouser_simulation/include/warehouser_simulation/simulation_node.hpp` | Add state_version_ member, checkpoint service declarations |
| `ros_ws/src/warehouser_simulation/src/world_manager.cpp` | Add serializeState/restoreState methods |
| `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp` | Add checkpoint method declarations |
| `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp` | Update QoS, remove polling loop, add version checking |
| `ros_ws/src/warehouser_observations/src/observations_node.cpp` | Update QoS configuration |
| `web_frontend/src/ros/connection.ts` | Add version extraction, metrics tracking |
| `web_frontend/src/store/appStore.ts` | Add version tracking, performance metrics |
| `training/training/envs/ros_env.py` | Add checkpoint save/load methods |

## Architecture Notes

### QoS Decision Matrix

From research (S.md), QoS policies must align with use case:

| Use Case | Reliability | Durability | Depth | Justification |
|----------|-------------|------------|-------|---------------|
| World State | Reliable | Transient Local | 1 | State snapshot, late-joining needs current state |
| Lidar Debug | Best Effort | Volatile | 5 | High-freq visualization, tolerate drops |
| Task Goals | Reliable | Transient Local | 1 | Published once, needed by all nodes |
| Clock | Best Effort | Volatile | 1 | Time sync, only latest matters |

### State Flow Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        WorldManager                              │
│                  (Authoritative State)                          │
│  - robots_, objects_, walls_, zones_                            │
│  - sim_time_, running_, state_version_                          │
└─────────────────┬───────────────────────────────────────────────┘
                  │
                  │ step(dt), reset(), checkpoint
                  │
                  ▼
        ┌─────────────────────┐
        │  SimulationNode     │
        │  (State Publisher)  │
        │  QoS: Reliable,     │
        │  Transient Local,   │
        │  depth=1            │
        └─────────┬───────────┘
                  │
                  │ publish 50 Hz
                  │ /world/state (versioned)
                  │
         ┌────────┼────────┬────────────┐
         │        │        │            │
         ▼        ▼        ▼            ▼
    ┌────────┐ ┌──────┐ ┌─────────┐  ┌─────────┐
    │ RL     │ │ Obs  │ │rosbridge│  │Frontend │
    │ Bridge │ │ Node │ │WebSocket│  │ Zustand │
    │ (v-chk)│ │      │ │         │  │ (v-chk) │
    └────┬───┘ └──┬───┘ └────┬────┘  └────┬────┘
         │        │          │            │
         │        │          │            │
         │        │          └───→ Version tracking
         │        │                 Metrics dashboard
         │        │
         │        └──→ Build observations
         │             (20Hz obs, 10Hz lidar)
         │
         └──→ RLStep/RLReset services
              Save/Load checkpoints
```

### Consistency Model

Warehouser uses **strong consistency** with authoritative server:

- **Single writer:** SimulationNode publishes authoritative state
- **Multiple readers:** Training, frontend, observations nodes
- **No write conflicts:** Only simulation mutates state
- **Synchronous mutations:** Training uses service calls, not optimistic updates
- **No reconciliation needed:** Clients trust server state

This is simpler than eventual consistency models (CRDTs) because there's no peer-to-peer coordination.

### Versioning Strategy

**Sequence numbers** provide:
1. **Gap detection:** Client detects version jumps (missed messages)
2. **Latency measurement:** Compare receive time vs header timestamp
3. **Replay validation:** Ensure checkpoint replay is complete
4. **Debug aid:** Correlate state across logs by version number

**NOT used for:**
- Conflict resolution (no conflicts in authoritative model)
- Causal ordering (single writer = total order guaranteed)

### Checkpoint Format

JSON serialization chosen for:
- **Human-readable** for debugging
- **Language-agnostic** (C++ saves, Python loads)
- **Schema evolution** (add fields without breaking old checkpoints)

Alternative: Binary serialization (Protobuf, MessagePack) for performance if needed.

### Time Synchronization

ROS2 sim time already implemented (simulation_node.cpp:164-167):
- Publishes `/clock` at 50 Hz
- Enables use_sim_time parameter
- Critical for deterministic replay with checkpoints

Checkpoint must store:
- **sim_time_** for temporal consistency
- **RNG state** for action sampling reproducibility
- **Entity states** for full world reconstruction

## Verification Checklist

### QoS Configuration Verification
- [ ] Late-joining frontend receives state immediately (< 100ms)
- [ ] No QoS incompatibility warnings in ROS2 logs
- [ ] `ros2 topic info /world/state --verbose` shows Transient Local
- [ ] Subscriber count matches expected clients

### State Versioning Verification
- [ ] Version increments monotonically (no duplicates)
- [ ] Header timestamp advances with sim_time
- [ ] Frontend detects and logs missed versions
- [ ] Gap detection works when subscriber paused/resumed

### Checkpoint System Verification
- [ ] Save checkpoint succeeds with valid filepath
- [ ] Load checkpoint restores exact state (diff world before/after)
- [ ] Training can save/load episodes
- [ ] RNG state restoration produces identical action sequences
- [ ] Invalid filepath returns clear error message

### Performance Metrics Verification
- [ ] Frontend metrics update at ~1 Hz
- [ ] Message rate shows ~50 Hz for world state
- [ ] Latency < 50ms for local connections
- [ ] Dropped message count accurate (test by pausing subscriber)

### Integration Testing
- [ ] Build succeeds: `colcon build --packages-select warehouser_msgs warehouser_simulation warehouser_rl_bridge`
- [ ] Tests pass: `colcon test --packages-select warehouser_simulation`
- [ ] Training episode completes with checkpoints
- [ ] Frontend displays metrics correctly
- [ ] Multi-client scenario: 2 frontends + training simultaneously

## Success Criteria

1. **Late-joining clients work:** Frontend reconnection shows state within 100ms
2. **Version tracking accurate:** Frontend detects 100% of dropped messages
3. **Checkpoints functional:** Load checkpoint restores identical state (verified by diff)
4. **No performance regression:** 50 Hz publication maintained under load
5. **Production ready:** Metrics and monitoring enable debugging in deployment

## References

### Research Sources (S.md)
- ROS2 QoS Design: https://design.ros2.org/articles/qos.html
- ROS2 QoS Documentation: https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Quality-of-Service-Settings.html
- WebSocket State Sync (2026): https://dasroot.net/posts/2026/02/python-websocket-servers-real-time-communication-patterns/
- Eventual Consistency: https://blog.logrocket.com/solving-eventual-consistency-frontend/

### Implementation Patterns (T.md)
- State snapshot QoS template (T.md:26-44)
- Versioning and timestamps (T.md:81-156)
- Checkpoint serialization (T.md:159-301)
- Frontend metrics tracking (T.md:587-636)

### Current Implementation (I.md)
- WorldManager authoritative source (I.md:13-28)
- SimulationNode 50 Hz publisher (I.md:31-50)
- Frontend reconnection logic (I.md:367-391)
- ROS2 single-threaded executor prevents races (I.md:124-128)

## Implementation Order Rationale

**Phase 1 (QoS) first** because:
- Fixes immediate user-facing issue (late-joining frontend blank screen)
- Low risk (well-understood ROS2 feature)
- Quick win (< 10 lines of code)
- Enables Phase 2 testing (need late-joining to test versioning)

**Phase 2 (Versioning) second** because:
- Builds on Phase 1 (needs TRANSIENT_LOCAL for complete gap detection)
- Enables observability for later phases
- Non-breaking change (adds fields, doesn't remove)

**Phase 3 (Checkpoints) third** because:
- Higher complexity (serialization, file I/O)
- Depends on versioning for replay validation
- Enables advanced training use cases

**Phase 4 (Monitoring) fourth** because:
- Uses versioning from Phase 2
- Production hardening, not core functionality
- Can be added incrementally

**Phases 5-6 (Polish) last** because:
- Nice-to-have improvements
- Current polling pattern works (just not elegant)
- Interpolation is UX enhancement, not critical

## Notes

- All changes maintain backward compatibility except WorldState.msg (requires rebuild)
- No changes to physics simulation or training algorithms
- Frontend changes are additive (version tracking optional)
- Checkpoint format is versioned (can add fields in future)
