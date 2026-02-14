# Template

Created: 2026-02-12T23:30:00Z

## Sources

1. ROS2 QoS documentation and best practices (from S.md research)
2. Existing Warehouser codebase patterns:
   - `ros_ws/src/warehouser_simulation/src/simulation_node.cpp`
   - `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp`
   - `web_frontend/src/ros/connection.ts`
   - `web_frontend/src/store/appStore.ts`
3. WebSocket state synchronization patterns (2026 best practices)
4. CRDT and event sourcing patterns for distributed systems

## Pattern Analysis

### 1. ROS2 QoS Configuration Templates

**Current State in Warehouser:**
- Publishers and subscribers use default QoS (depth=10, Reliable, Volatile)
- No explicit QoS configuration for state topics
- Late-joining frontend clients don't receive initial state

**Template Pattern: State Snapshot QoS**

```cpp
// C++ (simulation_node.cpp, rl_bridge_node.cpp)
#include <rclcpp/qos.hpp>

// World state: Late-joining subscribers need current state
auto state_qos = rclcpp::QoS(rclcpp::KeepLast(1))
    .reliable()
    .transient_local()  // Key: enables late-joining
    .durability_volatile();

state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
    "/world/state", state_qos);

// Subscription side (rl_bridge, observations nodes)
world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
    "/world/state", state_qos,
    std::bind(&Node::worldStateCallback, this, std::placeholders::_1));
```

**Template Pattern: High-Frequency Sensor QoS**

```cpp
// For lidar debug topic (50+ Hz, tolerates drops)
auto sensor_qos = rclcpp::QoS(rclcpp::KeepLast(5))
    .best_effort()      // Lower latency, allow drops
    .durability_volatile();

lidar_debug_pub_ = create_publisher<warehouser_msgs::msg::LidarDebug>(
    "/observations/lidar_debug", sensor_qos);
```

**Template Pattern: Configuration/Description QoS**

```cpp
// For robot description, task goals (publish once, needed by all)
auto config_qos = rclcpp::QoS(rclcpp::KeepLast(1))
    .reliable()
    .transient_local()
    .lifespan(std::chrono::seconds(0));  // Persist indefinitely

goal_pub_ = create_publisher<warehouser_msgs::msg::Goal>(
    "/task/goal", config_qos);
```

**QoS Profile Reference Table:**

| Topic | Reliability | Durability | Depth | Rationale |
|-------|-------------|------------|-------|-----------|
| `/world/state` | Reliable | Transient Local | 1 | Late-joining gets state |
| `/observations/lidar_debug` | Best Effort | Volatile | 5 | High-freq, tolerate drops |
| `/task/goal` | Reliable | Transient Local | 1 | Config needed by all |
| `/clock` | Best Effort | Volatile | 1 | Time sync, latest only |
| `/cmd_vel` | Best Effort | Volatile | 1 | Control input, latest only |

### 2. State Versioning and Timestamps

**Template Pattern: Versioned World State**

Add to `warehouser_msgs/msg/WorldState.msg`:
```
std_msgs/Header header  # Includes timestamp and sequence number
int64 version           # Monotonic version counter
float32 sim_time        # Simulation time (already exists)
Entity[] entities       # Entity list (already exists)
```

**Template Pattern: Version Tracking in C++**

```cpp
// In simulation_node.hpp
class SimulationNode : public rclcpp::Node {
private:
    uint64_t state_version_ = 0;

    void tick() {
        world_.step(dt_);

        auto state_msg = world_.toMsg();

        // Add versioning metadata
        state_msg.header.stamp = now();
        state_msg.header.frame_id = "world";
        state_msg.version = ++state_version_;

        state_pub_->publish(state_msg);

        // Clock publishing (unchanged)
        rosgraph_msgs::msg::Clock clock_msg;
        clock_msg.clock = rclcpp::Time(static_cast<int64_t>(world_.simTime() * 1e9));
        clock_pub_->publish(clock_msg);
    }
};
```

**Template Pattern: Client-Side Version Checking**

```typescript
// In web_frontend/src/store/appStore.ts
interface AppState {
  // ... existing fields ...
  lastStateVersion: number;
  missedVersions: number;
  setWorldState: (entities: Entity[], version: number, timestamp: number) => void;
}

export const useAppStore = create<AppState>((set, get) => ({
  // ... existing state ...
  lastStateVersion: 0,
  missedVersions: 0,

  setWorldState: (entities, version, timestamp) => {
    const state = get();

    // Detect missed updates
    const expectedVersion = state.lastStateVersion + 1;
    if (version > expectedVersion) {
      const missed = version - expectedVersion;
      console.warn(`Missed ${missed} state updates`);
      set({ missedVersions: state.missedVersions + missed });
    }

    set({
      entities,
      lastStateVersion: version,
      lastUpdateTimestamp: timestamp
    });
  },
}));
```

### 3. Checkpoint/Restore Pattern

**Template Pattern: Serializable World State**

```cpp
// In world_manager.hpp
#include <expected>
#include <string>
#include <fstream>

class WorldManager {
public:
    // Serialize full world state to JSON
    std::expected<std::string, std::string> serializeState() const;

    // Restore world state from JSON
    std::expected<void, std::string> restoreState(const std::string& json);

    // Save checkpoint to file
    std::expected<void, std::string> saveCheckpoint(const std::string& filepath) const {
        auto json_result = serializeState();
        if (!json_result) {
            return std::unexpected(json_result.error());
        }

        std::ofstream file(filepath);
        if (!file) {
            return std::unexpected("Failed to open file: " + filepath);
        }

        file << json_result.value();
        return {};
    }

    // Load checkpoint from file
    std::expected<void, std::string> loadCheckpoint(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file) {
            return std::unexpected("Failed to open file: " + filepath);
        }

        std::string json((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
        return restoreState(json);
    }

    // Get RNG state for deterministic replay
    std::mt19937::result_type getRngState() const { return rng_(); }
    void setRngState(std::mt19937::result_type state) { rng_.seed(state); }
};
```

**Template Pattern: ROS2 Checkpoint Service**

```cpp
// New service definition: warehouser_msgs/srv/SaveCheckpoint.srv
string filepath
---
bool success
string message

// New service definition: warehouser_msgs/srv/LoadCheckpoint.srv
string filepath
---
bool success
string message
warehouser_msgs/WorldState state
```

```cpp
// In simulation_node.cpp
checkpoint_save_srv_ = create_service<warehouser_msgs::srv::SaveCheckpoint>(
    "/sim/save_checkpoint",
    std::bind(&SimulationNode::handleSaveCheckpoint, this,
              std::placeholders::_1, std::placeholders::_2));

checkpoint_load_srv_ = create_service<warehouser_msgs::srv::LoadCheckpoint>(
    "/sim/load_checkpoint",
    std::bind(&SimulationNode::handleLoadCheckpoint, this,
              std::placeholders::_1, std::placeholders::_2));

void SimulationNode::handleSaveCheckpoint(
    const warehouser_msgs::srv::SaveCheckpoint::Request::SharedPtr request,
    warehouser_msgs::srv::SaveCheckpoint::Response::SharedPtr response) {

    auto result = world_.saveCheckpoint(request->filepath);

    if (result) {
        response->success = true;
        response->message = "Checkpoint saved to: " + request->filepath;
        RCLCPP_INFO(get_logger(), "Saved checkpoint to %s", request->filepath.c_str());
    } else {
        response->success = false;
        response->message = result.error();
        RCLCPP_ERROR(get_logger(), "Failed to save checkpoint: %s", result.error().c_str());
    }
}
```

**Template Pattern: Training Episode Checkpointing**

```python
# In training/training/envs/ros_env.py
from pathlib import Path
import json

class ROSEnv(gym.Env):
    def __init__(self, checkpoint_dir: Path | None = None):
        # ... existing init ...
        self.checkpoint_dir = checkpoint_dir or Path("checkpoints")
        self.checkpoint_dir.mkdir(exist_ok=True)
        self.episode_count = 0

    def save_checkpoint(self, reward: float, info: dict) -> Path:
        """Save episode checkpoint with metadata."""
        checkpoint_data = {
            "episode": self.episode_count,
            "cumulative_reward": reward,
            "step_count": self.step_count,
            "seed": self.current_seed,
            "info": info,
        }

        filepath = self.checkpoint_dir / f"episode_{self.episode_count:06d}.json"
        filepath.write_text(json.dumps(checkpoint_data, indent=2))

        # Also save world state via ROS service
        req = SaveCheckpointRequest()
        req.filepath = str(filepath.with_suffix(".world"))
        self.checkpoint_client.call(req)

        return filepath

    def load_checkpoint(self, episode: int) -> dict:
        """Load checkpoint and restore world state."""
        filepath = self.checkpoint_dir / f"episode_{episode:06d}.json"
        checkpoint_data = json.loads(filepath.read_text())

        # Restore world state
        req = LoadCheckpointRequest()
        req.filepath = str(filepath.with_suffix(".world"))
        resp = self.checkpoint_client.call(req)

        return checkpoint_data
```

### 4. Frontend State Recovery

**Template Pattern: Connection Status with Recovery**

Already well-implemented in `web_frontend/src/ros/connection.ts`. Key patterns:

```typescript
// Exponential backoff with jitter (already implemented)
const RECONNECT_CONFIG = {
  baseDelay: 1000,      // 1 second
  maxDelay: 30000,      // 30 seconds
  factor: 2,
  jitter: 0.1,
  maxAttempts: 10,
}

export function calculateBackoffDelay(attempt: number): number {
  const exponentialDelay = RECONNECT_CONFIG.baseDelay * Math.pow(RECONNECT_CONFIG.factor, attempt)
  const cappedDelay = Math.min(exponentialDelay, RECONNECT_CONFIG.maxDelay)
  const jitterRange = cappedDelay * RECONNECT_CONFIG.jitter
  const jitter = (Math.random() * 2 - 1) * jitterRange
  return Math.round(cappedDelay + jitter)
}
```

**Template Pattern: State Rehydration After Reconnect**

```typescript
// Enhancement to connection.ts
ros.on('connection', () => {
  console.log('Connected to ROS')
  store.setConnected(true)
  resetReconnectionState()

  // Request full state snapshot after reconnection
  requestStateSnapshot()

  subscribeToTopics()
})

/**
 * Request full state snapshot (uses Transient Local to get latest)
 */
function requestStateSnapshot() {
  if (!ros) return

  const store = useAppStore.getState()

  // With Transient Local QoS, subscription automatically receives latest state
  // No explicit service call needed - DDS handles it

  // Clear stale state during reconnection
  store.clearStaleState()
}
```

**Template Pattern: Optimistic Update Reconciliation**

```typescript
// Pattern for future interactive features (drag-and-drop entities)
interface AppState {
  // ... existing fields ...
  optimisticUpdates: Map<string, Entity>;  // entity_id -> optimistic state

  applyOptimisticUpdate: (entityId: string, update: Partial<Entity>) => void;
  reconcileUpdate: (entities: Entity[], serverVersion: number) => void;
}

export const useAppStore = create<AppState>((set, get) => ({
  // ... existing state ...
  optimisticUpdates: new Map(),

  applyOptimisticUpdate: (entityId, update) => {
    const state = get();
    const entity = state.entities.find(e => e.id === entityId);
    if (!entity) return;

    // Apply update immediately for responsive UI
    const optimisticEntity = { ...entity, ...update };

    set({
      optimisticUpdates: new Map(state.optimisticUpdates).set(entityId, optimisticEntity),
      entities: state.entities.map(e =>
        e.id === entityId ? optimisticEntity : e
      ),
    });
  },

  reconcileUpdate: (serverEntities, serverVersion) => {
    const state = get();

    // Server state is authoritative - discard optimistic updates
    // If server disagrees, revert to server state
    set({
      entities: serverEntities,
      optimisticUpdates: new Map(),  // Clear optimistic state
      lastStateVersion: serverVersion,
    });
  },
}));
```

### 5. Multi-Client Synchronization

**Pattern: Authoritative Server, Multiple Observers**

Already correctly implemented in Warehouser:
- Simulation node is single authoritative writer
- Frontend clients are pure subscribers (read-only)
- Training client uses synchronous request-response (no conflicts)

**Template Pattern: Client Registry (Optional for Advanced Features)**

```cpp
// For tracking connected clients and their state versions
// Useful for debugging and monitoring

class SimulationNode : public rclcpp::Node {
private:
    struct ClientInfo {
        std::string client_id;
        uint64_t last_ack_version;
        rclcpp::Time last_seen;
    };

    std::unordered_map<std::string, ClientInfo> connected_clients_;

    // Service for clients to register/heartbeat
    rclcpp::Service<warehouser_msgs::srv::ClientHeartbeat>::SharedPtr heartbeat_srv_;

    void handleClientHeartbeat(
        const warehouser_msgs::srv::ClientHeartbeat::Request::SharedPtr request,
        warehouser_msgs::srv::ClientHeartbeat::Response::SharedPtr response) {

        ClientInfo info;
        info.client_id = request->client_id;
        info.last_ack_version = request->last_received_version;
        info.last_seen = now();

        connected_clients_[request->client_id] = info;

        response->current_version = state_version_;
        response->success = true;
    }

    // Periodic cleanup of stale clients
    void cleanupStaleClients() {
        auto cutoff = now() - rclcpp::Duration::from_seconds(30.0);

        for (auto it = connected_clients_.begin(); it != connected_clients_.end(); ) {
            if (it->second.last_seen < cutoff) {
                RCLCPP_INFO(get_logger(), "Removing stale client: %s", it->first.c_str());
                it = connected_clients_.erase(it);
            } else {
                ++it;
            }
        }
    }
};
```

### 6. Deterministic Replay

**Template Pattern: Simulation Time and Fixed Seeds**

```cpp
// In simulation_node.cpp - already partially implemented
class SimulationNode : public rclcpp::Node {
private:
    bool use_sim_time_ = true;
    double sim_time_ = 0.0;
    uint32_t current_seed_ = 0;

    void handleReset(
        const std_srvs::srv::Trigger::Request::SharedPtr request,
        std_srvs::srv::Trigger::Response::SharedPtr response) {

        // Allow seed to be configured via parameter
        current_seed_ = get_parameter("reset_seed").as_int();
        if (current_seed_ == 0) {
            // Generate random seed if not specified
            std::random_device rd;
            current_seed_ = rd();
        }

        world_.reset(current_seed_);
        sim_time_ = 0.0;
        state_version_ = 0;

        response->success = true;
        response->message = std::format("Reset with seed: {}", current_seed_);

        RCLCPP_INFO(get_logger(), "Reset simulation (seed=%u)", current_seed_);
    }
};
```

**Template Pattern: Event Log for Replay**

```cpp
// Event logging for deterministic replay
struct SimulationEvent {
    double timestamp;
    std::string event_type;  // "cmd_vel", "pick", "unpick", "step"
    std::string data;        // JSON payload
};

class SimulationNode : public rclcpp::Node {
private:
    std::vector<SimulationEvent> event_log_;
    bool replay_mode_ = false;
    size_t replay_index_ = 0;

    void logEvent(const std::string& type, const std::string& data) {
        if (!replay_mode_) {
            SimulationEvent event;
            event.timestamp = world_.simTime();
            event.event_type = type;
            event.data = data;
            event_log_.push_back(event);
        }
    }

    void cmdVelCallback(const geometry_msgs::msg::Twist::SharedPtr msg) {
        // Log the command
        std::string data = std::format("{{\"linear\": {}, \"angular\": {}}}",
                                       msg->linear.x, msg->angular.z);
        logEvent("cmd_vel", data);

        // Execute command
        if (auto* robot = world_.robot()) {
            robot->setCommand(static_cast<float>(msg->linear.x),
                            static_cast<float>(msg->angular.z));
        }
    }

    void saveEventLog(const std::string& filepath) {
        std::ofstream file(filepath);
        file << "[\n";
        for (size_t i = 0; i < event_log_.size(); ++i) {
            const auto& event = event_log_[i];
            file << "  {\"timestamp\": " << event.timestamp
                 << ", \"type\": \"" << event.event_type << "\""
                 << ", \"data\": " << event.data << "}";
            if (i < event_log_.size() - 1) file << ",";
            file << "\n";
        }
        file << "]\n";
    }
};
```

### 7. Performance Monitoring

**Template Pattern: QoS Event Monitoring**

```cpp
// Monitor dropped messages and QoS violations
#include <rclcpp/qos_event.hpp>

class SimulationNode : public rclcpp::Node {
private:
    void setupQosEventHandlers() {
        // Monitor message drops on state publisher
        auto dropped_callback = [this](rclcpp::QOSDeadlineOfferedInfo& event) {
            RCLCPP_WARN(get_logger(),
                "State publisher dropped %d messages",
                event.total_count_change);
        };

        state_pub_->set_on_deadline_missed_callback(dropped_callback);

        // Monitor liveliness
        auto liveliness_callback = [this](rclcpp::QOSLivelinessChangedInfo& event) {
            RCLCPP_INFO(get_logger(),
                "Liveliness changed: alive=%d, not_alive=%d",
                event.alive_count, event.not_alive_count);
        };

        state_pub_->set_on_liveliness_changed_callback(liveliness_callback);
    }
};
```

**Template Pattern: Frontend Performance Metrics**

```typescript
// In web_frontend/src/store/appStore.ts
interface PerformanceMetrics {
  messageRate: number;        // Messages per second
  latency: number;            // Average latency (ms)
  droppedMessages: number;
  lastMeasurement: number;
}

interface AppState {
  // ... existing fields ...
  metrics: PerformanceMetrics;
  updateMetrics: (receivedAt: number, serverTimestamp: number) => void;
}

export const useAppStore = create<AppState>((set, get) => ({
  // ... existing state ...
  metrics: {
    messageRate: 0,
    latency: 0,
    droppedMessages: 0,
    lastMeasurement: Date.now(),
  },

  updateMetrics: (receivedAt, serverTimestamp) => {
    const state = get();
    const now = Date.now();
    const elapsed = (now - state.metrics.lastMeasurement) / 1000;

    // Calculate message rate (exponential moving average)
    const alpha = 0.1;
    const instantRate = 1 / elapsed;
    const newRate = alpha * instantRate + (1 - alpha) * state.metrics.messageRate;

    // Calculate latency
    const latency = now - serverTimestamp;

    set({
      metrics: {
        messageRate: newRate,
        latency,
        droppedMessages: state.missedVersions,
        lastMeasurement: now,
      },
    });
  },
}));
```

## Application to Warehouser

### Immediate Improvements (High Priority)

1. **Add Transient Local QoS to World State Topic**
   - Modify `simulation_node.cpp` line 122-123
   - Replace `create_publisher<...>("/world/state", 10)` with explicit QoS
   - Frontend clients will receive state immediately on connection

2. **Add State Versioning**
   - Extend `WorldState.msg` with header and version fields
   - Track version in `SimulationNode::tick()`
   - Monitor missed versions in frontend

3. **Enhance Connection Recovery**
   - Already well-implemented in frontend
   - Add state reconciliation after reconnect
   - Display connection metrics to user

### Medium Priority

4. **Implement Checkpoint/Restore Services**
   - Add `SaveCheckpoint.srv` and `LoadCheckpoint.srv`
   - Implement serialization in `WorldManager`
   - Use for training episode replay and debugging

5. **Add Performance Monitoring**
   - QoS event handlers in C++ nodes
   - Frontend metrics dashboard
   - Log dropped messages and latency

### Future Enhancements (Low Priority)

6. **Event Logging for Deterministic Replay**
   - Log all simulation inputs
   - Replay from event log for testing
   - Use for regression testing

7. **Client Registry**
   - Track connected clients
   - Monitor client health
   - Useful for multi-user scenarios

## Copy-Paste Ready Snippets

### Snippet 1: Update World State Publisher QoS

```cpp
// In ros_ws/src/warehouser_simulation/src/simulation_node.cpp
// Replace line 122-123:

// OLD:
// state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
//     "/world/state", 10);

// NEW:
auto state_qos = rclcpp::QoS(rclcpp::KeepLast(1))
    .reliable()
    .transient_local();

state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
    "/world/state", state_qos);
```

### Snippet 2: Update World State Subscriber QoS

```cpp
// In ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp
// Replace line 39-42:

// OLD:
// world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
//     "/world/state", 10,
//     std::bind(&RLBridgeNode::worldStateCallback, this,
//               std::placeholders::_1));

// NEW:
auto state_qos = rclcpp::QoS(rclcpp::KeepLast(1))
    .reliable()
    .transient_local();

world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
    "/world/state", state_qos,
    std::bind(&RLBridgeNode::worldStateCallback, this,
              std::placeholders::_1));
```

### Snippet 3: Add State Version Tracking

```cpp
// In ros_ws/src/warehouser_simulation/include/warehouser_simulation/simulation_node.hpp
// Add to private members:

class SimulationNode : public rclcpp::Node {
private:
    // ... existing members ...
    uint64_t state_version_ = 0;  // ADD THIS
};

// In ros_ws/src/warehouser_simulation/src/simulation_node.cpp
// Update tick() method around line 156:

void SimulationNode::tick() {
    world_.step(dt_);

    auto state_msg = world_.toMsg();

    // ADD VERSIONING:
    state_msg.header.stamp = now();
    state_msg.header.frame_id = "world";
    state_msg.version = ++state_version_;

    state_pub_->publish(state_msg);

    // ... rest of function unchanged ...
}
```

### Snippet 4: Frontend Version Monitoring

```typescript
// In web_frontend/src/ros/connection.ts
// Update worldStateTopic.subscribe callback around line 133:

worldStateTopic.subscribe((msg: unknown) => {
  const message = msg as {
    entities: unknown[];
    sim_time: number;
    header: { stamp: { sec: number; nanosec: number } };
    version: number;
  }

  const entities: Entity[] = message.entities.map((e: unknown) => {
    // ... existing entity mapping ...
  })

  // ADD VERSION TRACKING:
  const timestamp = message.header.stamp.sec * 1000 +
                   message.header.stamp.nanosec / 1_000_000;
  const version = message.version || 0;

  store.setWorldState(entities, version, timestamp);
  store.setSimTime(message.sim_time);
})
```

## References

- ROS2 QoS Design: https://design.ros2.org/articles/qos.html
- ROS2 QoS Documentation: https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Quality-of-Service-Settings.html
- WebSocket Best Practices: https://ably.com/topic/websocket-architecture-best-practices
- Exponential Backoff Pattern: Already implemented in `connection.ts`
- CRDT Patterns: https://github.com/ljwagerfield/crdt (for future peer-to-peer features)
