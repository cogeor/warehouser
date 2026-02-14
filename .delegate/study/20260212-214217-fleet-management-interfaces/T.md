# Template Analysis: Fleet Management Interfaces

Created: 2026-02-12T22:15:00Z

## Sources

### Primary References
1. **Open-RMF Fleet Adapter**: https://github.com/open-rmf/rmf_demos/tree/main/rmf_demos_fleet_adapter
2. **VDA5050 Protocol Specification**: https://github.com/VDA5050/VDA5050/blob/main/VDA5050_EN.md
3. **ROS AMR Interop**: https://github.com/inorbit-ai/ros_amr_interop
4. **VDA5050 TypeScript Library**: https://github.com/coatyio/vda-5050-lib.js
5. **Open-RMF Message Definitions**: https://github.com/open-rmf/rmf_internal_msgs

### Additional Context
- S.md research findings on Open-RMF architecture and VDA5050 integration patterns
- Industry standards for multi-robot warehouse coordination
- Fleet state management and task allocation patterns

## Pattern 1: Open-RMF Fleet Adapter Interface

### Overview
Open-RMF provides standardized message interfaces for fleet coordination, enabling heterogeneous robot fleets to work together under centralized orchestration.

### Message Definitions

#### FleetState.msg
```msg
# Fleet identifier and robot states
string name
RobotState[] robots
```

**Purpose**: Aggregate state for entire fleet, published periodically by fleet adapter.

#### RobotState.msg
```msg
string name                    # Robot identifier
string model                   # Robot model/type
string task_id                 # Current task ID from Request messages
uint64 seq                     # Sequence number (increment per message)
RobotMode mode                 # Current operational mode
float32 battery_percent        # Battery level 0-100
Location location              # Current position
Location[] path                # Planned trajectory
```

**Purpose**: Individual robot state within fleet, updated continuously.

#### Location.msg
```msg
builtin_interfaces/Time t                  # Timestamp
float32 x                                  # X coordinate (meters)
float32 y                                  # Y coordinate (meters)
float32 yaw                                # Orientation (radians)
bool obey_approach_speed_limit false       # Speed limit enforcement
float32 approach_speed_limit               # Speed limit value (m/s)
string level_name                          # Map/floor identifier
uint64 index                               # Waypoint index
```

**Purpose**: Position and navigation waypoint representation.

#### RobotMode.msg
```msg
uint32 mode                    # Current mode (see constants)
uint64 mode_request_id         # Mode request identifier
string performing_action       # Action description (MODE_PERFORMING_ACTION only)

# Mode constants
uint32 MODE_IDLE = 0              # Robot at rest
uint32 MODE_CHARGING = 1          # Battery charging
uint32 MODE_MOVING = 2            # In transit
uint32 MODE_PAUSED = 3            # Operation suspended
uint32 MODE_WAITING = 4           # Awaiting input
uint32 MODE_EMERGENCY = 5         # Emergency state
uint32 MODE_GOING_HOME = 6        # Returning to base
uint32 MODE_DOCKING = 7           # Docking procedure
uint32 MODE_ADAPTER_ERROR = 8     # Command error, needs recompute
uint32 MODE_CLEANING = 9          # Cleaning operation
uint32 MODE_PERFORMING_ACTION = 10  # Executing action (simulation)
uint32 MODE_ACTION_COMPLETED = 11   # Action finished (simulation)
```

**Purpose**: Standardized robot operational modes for fleet coordination.

#### BidNotice.msg
```msg
string request                              # Task request details (JSON)
string task_id                              # Task identifier
builtin_interfaces/Duration time_window     # Bidding window duration
bool dry_run false                          # Test mode (no assignment)
```

**Purpose**: Task Dispatcher broadcasts this to initiate competitive bidding among fleet adapters.

#### BidProposal.msg
```msg
string fleet_name                      # Fleet Adapter name
string expected_robot_name             # Robot that will execute
float64 prev_cost                      # Cost before new task
float64 new_cost                       # Cost after new task
builtin_interfaces/Time finish_time    # Estimated completion time
```

**Purpose**: Fleet Adapter response to BidNotice with cost estimate and completion time.

### Implementation Pattern

#### Fleet Adapter Node Structure (Python)
```python
import rclpy
from rclpy.node import Node
from rmf_fleet_msgs.msg import FleetState, RobotState, RobotMode, Location
from rmf_task_msgs.msg import BidNotice, BidProposal
from builtin_interfaces.msg import Time, Duration

class WarehouserFleetAdapter(Node):
    def __init__(self):
        super().__init__('warehouser_fleet_adapter')

        # Publishers
        self.fleet_state_pub = self.create_publisher(
            FleetState,
            '/fleet_states',
            10
        )

        self.bid_proposal_pub = self.create_publisher(
            BidProposal,
            '/bid_proposals',
            10
        )

        # Subscribers
        self.bid_notice_sub = self.create_subscription(
            BidNotice,
            '/bid_notices',
            self.handle_bid_notice,
            10
        )

        # Fleet state tracking
        self.fleet_name = 'warehouser_fleet'
        self.robots = {}  # robot_name -> RobotState

        # State publishing timer (10 Hz recommended)
        self.state_timer = self.create_timer(0.1, self.publish_fleet_state)

    def publish_fleet_state(self):
        """Publish current fleet state to RMF"""
        msg = FleetState()
        msg.name = self.fleet_name
        msg.robots = list(self.robots.values())
        self.fleet_state_pub.publish(msg)

    def update_robot_state(self, robot_name: str, position: tuple,
                          battery: float, mode: int, task_id: str = ''):
        """Update individual robot state"""
        if robot_name not in self.robots:
            self.robots[robot_name] = RobotState()
            self.robots[robot_name].name = robot_name
            self.robots[robot_name].model = 'warehouser_v1'
            self.robots[robot_name].seq = 0

        robot = self.robots[robot_name]
        robot.seq += 1
        robot.task_id = task_id
        robot.battery_percent = battery

        # Update mode
        robot.mode.mode = mode

        # Update location
        robot.location.x = position[0]
        robot.location.y = position[1]
        robot.location.yaw = position[2]
        robot.location.level_name = 'warehouse_floor_1'
        robot.location.t = self.get_clock().now().to_msg()

    def handle_bid_notice(self, msg: BidNotice):
        """Handle task bidding request"""
        # Parse task request (typically JSON)
        task_data = json.loads(msg.request)

        # Find best robot for task
        best_robot, cost_delta, finish_time = self.compute_best_bid(
            task_data, msg.task_id
        )

        if best_robot is None:
            return  # Cannot handle this task

        # Submit bid proposal
        proposal = BidProposal()
        proposal.fleet_name = self.fleet_name
        proposal.expected_robot_name = best_robot
        proposal.prev_cost = self.current_fleet_cost
        proposal.new_cost = self.current_fleet_cost + cost_delta
        proposal.finish_time = finish_time

        self.bid_proposal_pub.publish(proposal)

    def compute_best_bid(self, task_data: dict, task_id: str):
        """Calculate optimal robot assignment and cost"""
        best_robot = None
        min_cost = float('inf')
        best_finish_time = None

        for robot_name, robot_state in self.robots.items():
            # Skip if robot unavailable (charging, emergency, etc.)
            if robot_state.mode.mode not in [RobotMode.MODE_IDLE,
                                             RobotMode.MODE_WAITING]:
                continue

            # Calculate cost: distance + battery penalty
            distance = self.calculate_distance(
                robot_state.location,
                task_data['pickup_location']
            )
            battery_penalty = max(0, 30 - robot_state.battery_percent) * 10
            cost = distance + battery_penalty

            # Estimate completion time
            travel_time = distance / 1.0  # 1 m/s average speed
            task_duration = task_data.get('estimated_duration', 60)
            finish_time = self.get_clock().now().to_msg()
            finish_time.sec += int(travel_time + task_duration)

            if cost < min_cost:
                min_cost = cost
                best_robot = robot_name
                best_finish_time = finish_time

        return best_robot, min_cost, best_finish_time

    def calculate_distance(self, loc1: Location, loc2: dict) -> float:
        """Euclidean distance between locations"""
        dx = loc1.x - loc2['x']
        dy = loc1.y - loc2['y']
        return (dx*dx + dy*dy) ** 0.5
```

### Application to Warehouser

**Integration Points:**
1. Query robot states from `ros_simulation` WorldManager
2. Subscribe to `ros_rl_bridge` multi-robot observation topics
3. Translate RMF task requests to RL training episodes
4. Publish fleet state at 10 Hz for traffic scheduling

**New Message Definitions (warehouser_msgs):**
```msg
# FleetState.msg
string fleet_name
RobotStatus[] robots

# RobotStatus.msg
string robot_id
float32 x
float32 y
float32 theta
float32 battery_percent
uint8 mode
string current_task_id
float32[] planned_path_x
float32[] planned_path_y
```

---

## Pattern 2: VDA5050 Protocol Implementation

### Overview
VDA5050 is the European standard for AGV/AMR communication using MQTT with JSON messages. It defines vehicle-level control protocol complementary to facility-level orchestration (Open-RMF).

### Message Schemas

#### Order Message (Master → Vehicle)
```json
{
  "headerId": 12345,
  "timestamp": "2026-02-12T22:00:00.000Z",
  "version": "2.0.0",
  "manufacturer": "warehouser",
  "serialNumber": "robot_001",
  "orderId": "order_2026_001",
  "orderUpdateId": 0,
  "zoneSetId": "warehouse_zones_v1",
  "nodes": [
    {
      "nodeId": "pickup_station_a",
      "sequenceId": 0,
      "nodeDescription": "Package pickup location",
      "released": true,
      "nodePosition": {
        "x": 10.5,
        "y": 20.3,
        "theta": 1.57,
        "allowedDeviationXY": 0.5,
        "allowedDeviationTheta": 0.1,
        "mapId": "warehouse_floor_1",
        "mapDescription": "Main warehouse floor"
      },
      "actions": [
        {
          "actionId": "pick_001",
          "actionType": "pick",
          "actionDescription": "Pick package from shelf",
          "blockingType": "HARD",
          "actionParameters": [
            {"key": "shelf_height", "value": 1.2},
            {"key": "package_id", "value": "PKG_12345"}
          ]
        }
      ]
    },
    {
      "nodeId": "delivery_zone_b",
      "sequenceId": 2,
      "nodeDescription": "Package delivery zone",
      "released": true,
      "nodePosition": {
        "x": 45.8,
        "y": 12.1,
        "theta": 0.0,
        "mapId": "warehouse_floor_1"
      },
      "actions": [
        {
          "actionId": "drop_001",
          "actionType": "drop",
          "blockingType": "HARD",
          "actionParameters": [
            {"key": "drop_height", "value": 0.5}
          ]
        }
      ]
    }
  ],
  "edges": [
    {
      "edgeId": "edge_a_to_b",
      "sequenceId": 1,
      "edgeDescription": "Path from pickup to delivery",
      "released": true,
      "startNodeId": "pickup_station_a",
      "endNodeId": "delivery_zone_b",
      "maxSpeed": 2.0,
      "orientation": 0.0,
      "orientationType": "GLOBAL",
      "rotationAllowed": false,
      "actions": []
    }
  ]
}
```

#### State Message (Vehicle → Master)
```json
{
  "headerId": 12346,
  "timestamp": "2026-02-12T22:00:01.500Z",
  "version": "2.0.0",
  "manufacturer": "warehouser",
  "serialNumber": "robot_001",
  "orderId": "order_2026_001",
  "orderUpdateId": 0,
  "lastNodeId": "pickup_station_a",
  "lastNodeSequenceId": 0,
  "nodeStates": [
    {
      "nodeId": "delivery_zone_b",
      "sequenceId": 2,
      "released": true,
      "nodePosition": {
        "x": 45.8,
        "y": 12.1,
        "theta": 0.0,
        "mapId": "warehouse_floor_1"
      }
    }
  ],
  "edgeStates": [
    {
      "edgeId": "edge_a_to_b",
      "sequenceId": 1,
      "released": true
    }
  ],
  "agvPosition": {
    "positionInitialized": true,
    "localizationScore": 0.95,
    "x": 28.5,
    "y": 16.2,
    "theta": 0.0,
    "mapId": "warehouse_floor_1"
  },
  "velocity": {
    "vx": 1.5,
    "vy": 0.0,
    "omega": 0.0
  },
  "driving": true,
  "paused": false,
  "distanceSinceLastNode": 18.3,
  "actionStates": [
    {
      "actionId": "pick_001",
      "actionType": "pick",
      "actionStatus": "FINISHED",
      "resultDescription": "Package picked successfully"
    }
  ],
  "batteryState": {
    "batteryCharge": 78.5,
    "batteryVoltage": 48.2,
    "batteryHealth": 95,
    "charging": false,
    "reach": 2500
  },
  "operatingMode": "AUTOMATIC",
  "errors": [],
  "information": [
    {
      "infoType": "performance",
      "infoDescription": "Average speed 1.5 m/s",
      "infoLevel": "INFO"
    }
  ],
  "safetyState": {
    "eStop": "NONE",
    "fieldViolation": false
  }
}
```

#### Visualization Message (Vehicle → Master, High Frequency)
```json
{
  "headerId": 12347,
  "timestamp": "2026-02-12T22:00:01.600Z",
  "version": "2.0.0",
  "manufacturer": "warehouser",
  "serialNumber": "robot_001",
  "agvPosition": {
    "positionInitialized": true,
    "x": 28.7,
    "y": 16.1,
    "theta": 0.05,
    "mapId": "warehouse_floor_1"
  },
  "velocity": {
    "vx": 1.5,
    "vy": 0.0,
    "omega": 0.02
  }
}
```

#### InstantActions Message (Master → Vehicle, Emergency)
```json
{
  "headerId": 12348,
  "timestamp": "2026-02-12T22:00:02.000Z",
  "version": "2.0.0",
  "manufacturer": "warehouser",
  "serialNumber": "robot_001",
  "actions": [
    {
      "actionId": "emergency_stop_001",
      "actionType": "stopPause",
      "actionDescription": "Emergency stop due to obstacle",
      "blockingType": "HARD",
      "actionParameters": []
    }
  ]
}
```

### MQTT Topic Structure
```
<interface>/<version>/<manufacturer>/<serialNumber>/<topic>

Examples:
uagv/v2/warehouser/robot_001/order          (Master → Vehicle, QoS 0)
uagv/v2/warehouser/robot_001/instantActions (Master → Vehicle, QoS 0)
uagv/v2/warehouser/robot_001/state          (Vehicle → Master, QoS 0)
uagv/v2/warehouser/robot_001/visualization  (Vehicle → Master, QoS 0)
uagv/v2/warehouser/robot_001/connection     (Vehicle → Master, QoS 1)
```

### TypeScript Implementation Pattern

#### VDA5050 Controller Setup
```typescript
import { AgvController, AgvClient, Topic, BlockingType } from 'vda-5050';
import type { Order, State, Headerless } from 'vda-5050';

// Configuration
const agvId = { manufacturer: 'warehouser', serialNumber: 'robot_001' };
const clientOptions = {
  interfaceName: 'uagv',
  transport: {
    brokerUrl: 'mqtt://warehouse-mqtt-broker:1883',
    protocolVersion: 5
  },
  vdaVersion: '2.0.0'
};

// Initialize AGV controller
const agvController = new AgvController(
  agvId,
  clientOptions,
  { agvAdapterType: WarehouserAgvAdapter },  // Custom adapter
  { simulationSpeed: 1.0 }
);

await agvController.start();

// Order subscription
agvController.subscribe(Topic.Order, (order) => {
  console.log(`Received order ${order.orderId}`);
  executeOrder(order);
});

// Instant action handler
agvController.subscribe(Topic.InstantActions, (actions) => {
  actions.actions.forEach(action => {
    if (action.actionType === 'stopPause') {
      emergencyStop();
    }
  });
});

// State publishing (1 Hz for state, 10 Hz for visualization)
setInterval(() => {
  const state = getCurrentState();
  agvController.publish(Topic.State, state);
}, 1000);

setInterval(() => {
  const viz = getCurrentVisualization();
  agvController.publish(Topic.Visualization, viz, { dropIfOffline: true });
}, 100);
```

#### Custom AGV Adapter for Warehouser
```typescript
import { AgvAdapter, AgvAdapterOptions } from 'vda-5050';
import type { Node, Edge, Action, AgvPosition, Velocity } from 'vda-5050';

interface WarehouserAdapterOptions extends AgvAdapterOptions {
  rosNodeUrl: string;
  robotNamespace: string;
}

class WarehouserAgvAdapter extends AgvAdapter {
  private rosClient: ROSClient;
  private currentPosition: AgvPosition;
  private currentVelocity: Velocity;

  constructor(options: WarehouserAdapterOptions) {
    super(options);

    // Connect to ROS2 bridge
    this.rosClient = new ROSClient(options.rosNodeUrl);

    // Subscribe to robot state
    this.rosClient.subscribe(
      `/${options.robotNamespace}/robot_state`,
      (msg) => this.updateState(msg)
    );
  }

  async navigateToNode(node: Node, edge?: Edge): Promise<void> {
    // Convert VDA5050 node to ROS navigation goal
    const goal = {
      x: node.nodePosition.x,
      y: node.nodePosition.y,
      theta: node.nodePosition.theta || 0.0
    };

    // Send to ROS navigation stack
    await this.rosClient.callService(
      `/${this.options.robotNamespace}/navigate_to_pose`,
      goal
    );

    // Wait for arrival
    await this.waitForArrival(node.nodePosition);
  }

  async executeAction(action: Action): Promise<void> {
    const actionType = action.actionType;
    const params = this.parseActionParameters(action.actionParameters);

    switch (actionType) {
      case 'pick':
        await this.rosClient.callService(
          `/${this.options.robotNamespace}/pick_object`,
          { height: params.shelf_height, package_id: params.package_id }
        );
        break;

      case 'drop':
        await this.rosClient.callService(
          `/${this.options.robotNamespace}/drop_object`,
          { height: params.drop_height }
        );
        break;

      case 'charge':
        await this.navigateToChargingStation();
        await this.rosClient.callService(
          `/${this.options.robotNamespace}/start_charging`,
          {}
        );
        break;

      default:
        throw new Error(`Unknown action type: ${actionType}`);
    }
  }

  getCurrentPosition(): AgvPosition {
    return this.currentPosition;
  }

  getCurrentVelocity(): Velocity {
    return this.currentVelocity;
  }

  private updateState(msg: any): void {
    this.currentPosition = {
      positionInitialized: true,
      localizationScore: msg.localization_quality || 1.0,
      x: msg.x,
      y: msg.y,
      theta: msg.theta,
      mapId: 'warehouse_floor_1'
    };

    this.currentVelocity = {
      vx: msg.velocity_x || 0.0,
      vy: msg.velocity_y || 0.0,
      omega: msg.angular_velocity || 0.0
    };
  }

  private parseActionParameters(params: Array<{key: string, value: any}>): any {
    return params.reduce((acc, p) => ({ ...acc, [p.key]: p.value }), {});
  }
}
```

### Application to Warehouser

**Integration Strategy:**
1. Create `warehouser_vda5050_connector` ROS2 package
2. MQTT bridge translates VDA5050 JSON to ROS2 messages
3. Map VDA5050 actions to Warehouser task primitives
4. Coordinate system alignment (VDA5050 uses right-hand rule, matches REP 103)

**Action Mapping:**
| VDA5050 Action | Warehouser Implementation |
|---------------|---------------------------|
| `pick` | Call `/pick_object` service |
| `drop` | Call `/drop_object` service |
| `charge` | Navigate to charging station + charging service |
| `wait` | Pause navigation, set MODE_WAITING |
| `stopPause` | Emergency stop via instant action |

**ROS2 Package Structure:**
```
warehouser_vda5050_connector/
├── package.xml
├── CMakeLists.txt
├── config/
│   └── vda5050_config.yaml
├── src/
│   ├── vda5050_connector_node.cpp
│   ├── mqtt_client.cpp
│   ├── order_translator.cpp
│   └── state_publisher.cpp
└── include/
    └── warehouser_vda5050_connector/
        ├── vda5050_connector.hpp
        └── message_schemas.hpp
```

---

## Pattern 3: Fleet State Aggregation

### Overview
Fleet management requires aggregating individual robot states into unified fleet metrics for monitoring, optimization, and decision-making.

### State Aggregation Pattern (Python)

```python
from dataclasses import dataclass
from typing import Dict, List
from enum import Enum

class RobotMode(Enum):
    IDLE = 0
    CHARGING = 1
    MOVING = 2
    PAUSED = 3
    EMERGENCY = 5
    PERFORMING_TASK = 10

@dataclass
class RobotState:
    robot_id: str
    x: float
    y: float
    theta: float
    battery_percent: float
    mode: RobotMode
    task_id: str
    velocity: float
    last_update: float

@dataclass
class FleetMetrics:
    total_robots: int
    active_robots: int
    idle_robots: int
    charging_robots: int
    emergency_robots: int
    average_battery: float
    tasks_in_progress: int
    tasks_completed_last_hour: int
    fleet_utilization: float  # 0.0 to 1.0
    average_velocity: float

class FleetStateAggregator:
    def __init__(self):
        self.robots: Dict[str, RobotState] = {}
        self.tasks_completed = 0
        self.tasks_completed_timestamp = time.time()

    def update_robot(self, robot_id: str, state: RobotState):
        """Update individual robot state"""
        self.robots[robot_id] = state

    def get_fleet_metrics(self) -> FleetMetrics:
        """Compute aggregate fleet metrics"""
        if not self.robots:
            return FleetMetrics(0, 0, 0, 0, 0, 0.0, 0, 0, 0.0, 0.0)

        total = len(self.robots)
        active = sum(1 for r in self.robots.values()
                    if r.mode == RobotMode.MOVING)
        idle = sum(1 for r in self.robots.values()
                  if r.mode == RobotMode.IDLE)
        charging = sum(1 for r in self.robots.values()
                      if r.mode == RobotMode.CHARGING)
        emergency = sum(1 for r in self.robots.values()
                       if r.mode == RobotMode.EMERGENCY)

        avg_battery = sum(r.battery_percent for r in self.robots.values()) / total

        tasks_active = sum(1 for r in self.robots.values()
                          if r.task_id != '')

        # Utilization: percentage of robots doing productive work
        utilization = (active + tasks_active) / (2 * total)

        # Average velocity of moving robots
        moving_robots = [r for r in self.robots.values()
                        if r.mode == RobotMode.MOVING]
        avg_velocity = (sum(r.velocity for r in moving_robots) / len(moving_robots)
                       if moving_robots else 0.0)

        return FleetMetrics(
            total_robots=total,
            active_robots=active,
            idle_robots=idle,
            charging_robots=charging,
            emergency_robots=emergency,
            average_battery=avg_battery,
            tasks_in_progress=tasks_active,
            tasks_completed_last_hour=self.get_tasks_last_hour(),
            fleet_utilization=utilization,
            average_velocity=avg_velocity
        )

    def get_robot_states_by_mode(self, mode: RobotMode) -> List[RobotState]:
        """Filter robots by operational mode"""
        return [r for r in self.robots.values() if r.mode == mode]

    def get_low_battery_robots(self, threshold: float = 20.0) -> List[RobotState]:
        """Find robots needing charge"""
        return [r for r in self.robots.values()
                if r.battery_percent < threshold and r.mode != RobotMode.CHARGING]

    def find_nearest_robot(self, x: float, y: float,
                          mode_filter: RobotMode = RobotMode.IDLE) -> RobotState:
        """Find nearest available robot to location"""
        candidates = [r for r in self.robots.values() if r.mode == mode_filter]
        if not candidates:
            return None

        return min(candidates,
                  key=lambda r: ((r.x - x)**2 + (r.y - y)**2)**0.5)
```

### Application to Warehouser

**Integration with Existing System:**
```python
# In ros_rl_bridge or new warehouser_fleet_manager package
class WarehouserFleetManager(Node):
    def __init__(self):
        super().__init__('warehouser_fleet_manager')

        self.aggregator = FleetStateAggregator()

        # Subscribe to multi-robot observations
        self.obs_sub = self.create_subscription(
            MultiRobotObservation,
            '/warehouser/observations',
            self.handle_observations,
            10
        )

        # Publish fleet metrics
        self.metrics_pub = self.create_publisher(
            FleetMetrics,
            '/fleet/metrics',
            10
        )

        self.timer = self.create_timer(1.0, self.publish_metrics)

    def handle_observations(self, msg):
        """Update fleet state from RL observations"""
        for i, robot_obs in enumerate(msg.observations):
            state = RobotState(
                robot_id=f'robot_{i}',
                x=robot_obs.position[0],
                y=robot_obs.position[1],
                theta=robot_obs.position[2],
                battery_percent=robot_obs.battery * 100.0,
                mode=self.infer_mode(robot_obs),
                task_id=robot_obs.current_task,
                velocity=robot_obs.velocity,
                last_update=self.get_clock().now().nanoseconds / 1e9
            )
            self.aggregator.update_robot(state.robot_id, state)

    def publish_metrics(self):
        """Publish aggregated fleet metrics"""
        metrics = self.aggregator.get_fleet_metrics()
        self.metrics_pub.publish(metrics)
```

---

## Pattern 4: Task Allocation and Bidding

### Overview
Task allocation in multi-robot systems requires cost-based bidding to achieve optimal fleet-wide resource utilization.

### Bidding Algorithm Pattern

```python
from typing import Tuple, Optional
from dataclasses import dataclass
import numpy as np

@dataclass
class Task:
    task_id: str
    pickup_location: Tuple[float, float]
    delivery_location: Tuple[float, float]
    priority: int  # 1-10, higher is more urgent
    estimated_duration: float  # seconds
    deadline: Optional[float]  # timestamp or None
    payload_weight: float  # kg

@dataclass
class TaskBid:
    robot_id: str
    task_id: str
    cost: float
    estimated_completion_time: float
    confidence: float  # 0.0 to 1.0

class TaskBiddingSystem:
    def __init__(self, fleet_aggregator: FleetStateAggregator):
        self.fleet = fleet_aggregator
        self.active_tasks: Dict[str, Task] = {}

        # Cost weights (tunable)
        self.DISTANCE_WEIGHT = 1.0
        self.BATTERY_WEIGHT = 5.0
        self.TIME_WEIGHT = 0.5
        self.PRIORITY_WEIGHT = 10.0

    def request_bids(self, task: Task) -> List[TaskBid]:
        """Request bids from all capable robots"""
        bids = []

        # Filter available robots (idle or waiting)
        available = [r for r in self.fleet.robots.values()
                    if r.mode in [RobotMode.IDLE, RobotMode.WAITING]]

        for robot in available:
            bid = self.calculate_bid(robot, task)
            if bid is not None:
                bids.append(bid)

        # Sort by cost (lower is better)
        return sorted(bids, key=lambda b: b.cost)

    def calculate_bid(self, robot: RobotState, task: Task) -> Optional[TaskBid]:
        """Calculate cost for robot to complete task"""

        # Check battery feasibility
        distance_total = (
            self.distance(robot.x, robot.y, *task.pickup_location) +
            self.distance(*task.pickup_location, *task.delivery_location)
        )
        battery_required = self.estimate_battery_usage(distance_total, task.payload_weight)

        if robot.battery_percent < battery_required + 10.0:  # 10% safety margin
            return None  # Cannot complete task

        # Cost components
        distance_cost = distance_total * self.DISTANCE_WEIGHT

        # Battery penalty (prefer higher battery robots)
        battery_cost = (100.0 - robot.battery_percent) * self.BATTERY_WEIGHT

        # Time cost (distance / speed)
        travel_time = distance_total / 1.5  # assume 1.5 m/s
        time_cost = (travel_time + task.estimated_duration) * self.TIME_WEIGHT

        # Priority bonus (higher priority = lower cost)
        priority_cost = (10 - task.priority) * self.PRIORITY_WEIGHT

        # Deadline urgency
        deadline_cost = 0.0
        if task.deadline is not None:
            time_to_deadline = task.deadline - time.time()
            if time_to_deadline < travel_time + task.estimated_duration:
                deadline_cost = 1000.0  # Large penalty if cannot meet deadline

        total_cost = (distance_cost + battery_cost + time_cost +
                     priority_cost + deadline_cost)

        # Estimate completion time
        completion_time = time.time() + travel_time + task.estimated_duration

        # Confidence based on battery and distance
        confidence = min(1.0, robot.battery_percent / 100.0) * \
                    (1.0 - min(1.0, distance_total / 100.0))

        return TaskBid(
            robot_id=robot.robot_id,
            task_id=task.task_id,
            cost=total_cost,
            estimated_completion_time=completion_time,
            confidence=confidence
        )

    def assign_task(self, task: Task) -> Optional[str]:
        """Assign task to best bidder"""
        bids = self.request_bids(task)

        if not bids:
            return None  # No capable robots

        # Select best bid (lowest cost)
        winner = bids[0]

        # Record assignment
        self.active_tasks[task.task_id] = task

        return winner.robot_id

    @staticmethod
    def distance(x1: float, y1: float, x2: float, y2: float) -> float:
        """Euclidean distance"""
        return ((x2 - x1)**2 + (y2 - y1)**2)**0.5

    @staticmethod
    def estimate_battery_usage(distance: float, payload: float) -> float:
        """Estimate battery percentage needed"""
        # Simple model: 1% per 10 meters + 0.5% per kg payload
        return (distance / 10.0) + (payload * 0.5)
```

### Multi-Task Optimization Pattern

```python
from typing import List
import numpy as np
from scipy.optimize import linear_sum_assignment

class MultiTaskOptimizer:
    """Optimize assignment of multiple tasks to multiple robots"""

    def __init__(self, bidding_system: TaskBiddingSystem):
        self.bidding = bidding_system

    def optimize_assignments(self, tasks: List[Task]) -> Dict[str, str]:
        """
        Optimal task assignment using Hungarian algorithm
        Returns: {task_id: robot_id}
        """
        if not tasks:
            return {}

        # Get all available robots
        available = [r for r in self.bidding.fleet.robots.values()
                    if r.mode in [RobotMode.IDLE, RobotMode.WAITING]]

        if not available:
            return {}

        # Build cost matrix: robots × tasks
        n_robots = len(available)
        n_tasks = len(tasks)
        cost_matrix = np.full((n_robots, n_tasks), np.inf)

        for i, robot in enumerate(available):
            for j, task in enumerate(tasks):
                bid = self.bidding.calculate_bid(robot, task)
                if bid is not None:
                    cost_matrix[i, j] = bid.cost

        # Solve assignment problem
        robot_indices, task_indices = linear_sum_assignment(cost_matrix)

        # Build assignment map
        assignments = {}
        for robot_idx, task_idx in zip(robot_indices, task_indices):
            if cost_matrix[robot_idx, task_idx] < np.inf:
                robot_id = available[robot_idx].robot_id
                task_id = tasks[task_idx].task_id
                assignments[task_id] = robot_id

        return assignments
```

### Application to Warehouser

**Integration Points:**
1. Receive task requests from web frontend or task manager
2. Query fleet state from aggregator
3. Run bidding algorithm
4. Publish task assignments to robots via ROS2 topics
5. Track completion and update metrics

**ROS2 Service Definition:**
```msg
# RequestTaskAllocation.srv
Task[] tasks
---
TaskAssignment[] assignments
bool success
string message

# TaskAssignment.msg
string task_id
string robot_id
float32 estimated_cost
float32 estimated_completion_time
```

---

## Pattern 5: Fleet Monitoring Dashboard

### Overview
Real-time visualization of fleet state, task progress, and performance metrics.

### WebSocket State Streaming (TypeScript)

```typescript
// Backend: Fleet state WebSocket server
import WebSocket from 'ws';
import { FleetMetrics, RobotState } from './types';

class FleetStateStreamer {
  private wss: WebSocket.Server;
  private clients: Set<WebSocket>;
  private updateInterval: NodeJS.Timer;

  constructor(port: number) {
    this.wss = new WebSocket.Server({ port });
    this.clients = new Set();

    this.wss.on('connection', (ws) => {
      console.log('Client connected');
      this.clients.add(ws);

      // Send initial state
      this.sendFleetState(ws);

      ws.on('close', () => {
        this.clients.delete(ws);
      });
    });

    // Stream updates at 10 Hz
    this.updateInterval = setInterval(() => {
      this.broadcastFleetState();
    }, 100);
  }

  private async sendFleetState(ws: WebSocket): Promise<void> {
    const state = await this.fetchFleetState();
    ws.send(JSON.stringify({
      type: 'fleet_state',
      timestamp: Date.now(),
      data: state
    }));
  }

  private async broadcastFleetState(): Promise<void> {
    const state = await this.fetchFleetState();
    const message = JSON.stringify({
      type: 'fleet_state',
      timestamp: Date.now(),
      data: state
    });

    this.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    });
  }

  private async fetchFleetState(): Promise<{
    metrics: FleetMetrics,
    robots: RobotState[]
  }> {
    // Fetch from ROS2 bridge or database
    // This would integrate with your existing ROS2 topics
    return {
      metrics: await this.getMetrics(),
      robots: await this.getRobotStates()
    };
  }
}
```

### React Dashboard Component

```typescript
// Frontend: Fleet monitoring dashboard
import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../hooks/useWebSocket';

interface FleetState {
  metrics: FleetMetrics;
  robots: RobotState[];
}

export const FleetDashboard: React.FC = () => {
  const [fleetState, setFleetState] = useState<FleetState | null>(null);
  const ws = useWebSocket('ws://localhost:8080');

  useEffect(() => {
    if (!ws) return;

    ws.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.type === 'fleet_state') {
        setFleetState(message.data);
      }
    };
  }, [ws]);

  if (!fleetState) {
    return <div>Loading fleet state...</div>;
  }

  const { metrics, robots } = fleetState;

  return (
    <div className="fleet-dashboard">
      <h1>Fleet Management Dashboard</h1>

      <div className="metrics-grid">
        <MetricCard
          title="Total Robots"
          value={metrics.total_robots}
          icon="robots"
        />
        <MetricCard
          title="Active"
          value={metrics.active_robots}
          color="green"
        />
        <MetricCard
          title="Idle"
          value={metrics.idle_robots}
          color="blue"
        />
        <MetricCard
          title="Charging"
          value={metrics.charging_robots}
          color="yellow"
        />
        <MetricCard
          title="Emergency"
          value={metrics.emergency_robots}
          color="red"
        />
        <MetricCard
          title="Avg Battery"
          value={`${metrics.average_battery.toFixed(1)}%`}
          icon="battery"
        />
        <MetricCard
          title="Fleet Utilization"
          value={`${(metrics.fleet_utilization * 100).toFixed(1)}%`}
          icon="utilization"
        />
        <MetricCard
          title="Tasks/Hour"
          value={metrics.tasks_completed_last_hour}
          icon="tasks"
        />
      </div>

      <div className="robot-list">
        <h2>Robot Status</h2>
        <table>
          <thead>
            <tr>
              <th>Robot ID</th>
              <th>Position</th>
              <th>Battery</th>
              <th>Mode</th>
              <th>Task</th>
              <th>Velocity</th>
            </tr>
          </thead>
          <tbody>
            {robots.map(robot => (
              <tr key={robot.robot_id}>
                <td>{robot.robot_id}</td>
                <td>({robot.x.toFixed(1)}, {robot.y.toFixed(1)})</td>
                <td>
                  <BatteryIndicator percent={robot.battery_percent} />
                </td>
                <td>
                  <ModeIndicator mode={robot.mode} />
                </td>
                <td>{robot.task_id || '-'}</td>
                <td>{robot.velocity.toFixed(2)} m/s</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="fleet-map">
        <h2>Fleet Visualization</h2>
        <FleetMap robots={robots} />
      </div>
    </div>
  );
};

const MetricCard: React.FC<{
  title: string;
  value: number | string;
  color?: string;
  icon?: string;
}> = ({ title, value, color, icon }) => (
  <div className={`metric-card ${color}`}>
    <div className="metric-icon">{icon}</div>
    <div className="metric-value">{value}</div>
    <div className="metric-title">{title}</div>
  </div>
);

const BatteryIndicator: React.FC<{ percent: number }> = ({ percent }) => {
  const color = percent > 50 ? 'green' : percent > 20 ? 'yellow' : 'red';
  return (
    <div className="battery-indicator">
      <div
        className={`battery-fill ${color}`}
        style={{ width: `${percent}%` }}
      />
      <span>{percent.toFixed(0)}%</span>
    </div>
  );
};

const ModeIndicator: React.FC<{ mode: string }> = ({ mode }) => {
  const colorMap: Record<string, string> = {
    IDLE: 'blue',
    MOVING: 'green',
    CHARGING: 'yellow',
    EMERGENCY: 'red',
    PAUSED: 'orange'
  };

  return (
    <span className={`mode-badge ${colorMap[mode] || 'gray'}`}>
      {mode}
    </span>
  );
};

const FleetMap: React.FC<{ robots: RobotState[] }> = ({ robots }) => {
  const canvasRef = React.useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Clear canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw robots
    robots.forEach(robot => {
      const x = robot.x * 5 + canvas.width / 2;  // Scale and center
      const y = canvas.height / 2 - robot.y * 5;  // Flip Y for canvas

      // Draw robot circle
      ctx.beginPath();
      ctx.arc(x, y, 8, 0, 2 * Math.PI);
      ctx.fillStyle = getModeColor(robot.mode);
      ctx.fill();

      // Draw orientation arrow
      ctx.beginPath();
      ctx.moveTo(x, y);
      const arrowLen = 15;
      const endX = x + arrowLen * Math.cos(robot.theta);
      const endY = y - arrowLen * Math.sin(robot.theta);  // Flip Y
      ctx.lineTo(endX, endY);
      ctx.strokeStyle = '#000';
      ctx.lineWidth = 2;
      ctx.stroke();

      // Draw robot ID
      ctx.fillStyle = '#000';
      ctx.font = '10px monospace';
      ctx.fillText(robot.robot_id, x + 12, y + 4);
    });
  }, [robots]);

  return (
    <canvas
      ref={canvasRef}
      width={800}
      height={600}
      className="fleet-map-canvas"
    />
  );
};

function getModeColor(mode: string): string {
  const colors: Record<string, string> = {
    IDLE: '#4A90E2',
    MOVING: '#7ED321',
    CHARGING: '#F5A623',
    EMERGENCY: '#D0021B',
    PAUSED: '#F8E71C'
  };
  return colors[mode] || '#9B9B9B';
}
```

### Application to Warehouser

**Integration Strategy:**
1. Add WebSocket server to `web_frontend` backend
2. Subscribe to ROS2 fleet topics via rclpy
3. Stream state updates to browser clients
4. Extend existing React visualization with fleet dashboard
5. Add historical data tracking (TimescaleDB or InfluxDB)

**Directory Structure:**
```
web_frontend/
├── src/
│   ├── components/
│   │   ├── FleetDashboard.tsx        (new)
│   │   ├── FleetMap.tsx              (new)
│   │   └── MetricCard.tsx            (new)
│   ├── hooks/
│   │   └── useWebSocket.ts           (new)
│   ├── stores/
│   │   └── fleetStore.ts             (new, Zustand)
│   └── types/
│       └── fleet.ts                  (new)
└── server/
    ├── websocket.ts                  (new)
    └── ros_bridge.ts                 (new)
```

---

## Summary

### Templates Available

**None found in `.delegate/templates/`**

However, extensive patterns extracted from:
1. Open-RMF reference implementations
2. VDA5050 protocol specifications
3. Industry-standard fleet management systems

### Patterns Identified

1. **Open-RMF Fleet Adapter**: ROS2-based fleet state publishing and task bidding
2. **VDA5050 Protocol**: MQTT/JSON vehicle-level communication
3. **Fleet State Aggregation**: Metrics calculation and monitoring
4. **Task Allocation**: Cost-based bidding and optimization
5. **Fleet Dashboard**: Real-time visualization and monitoring

### Direct Application to Warehouser

All patterns are directly applicable:

- **Message definitions** can be added to `warehouser_msgs`
- **Fleet adapter** integrates with existing `ros_rl_bridge` multi-robot support
- **VDA5050 connector** provides external fleet management compatibility
- **State aggregation** extends current observation system
- **Dashboard** builds on existing React frontend

### Recommended Next Steps

1. Implement Open-RMF message definitions in `warehouser_msgs`
2. Create `warehouser_fleet_adapter` package with state publishing
3. Add task bidding system with cost-based allocation
4. Build fleet metrics aggregator
5. Extend web frontend with fleet dashboard
6. (Optional) Add VDA5050 connector for external integration

### Code Reusability

High reusability across patterns:
- Message schemas are copy-paste ready
- Algorithm implementations require minimal adaptation
- TypeScript patterns integrate directly with existing frontend
- ROS2 patterns follow Warehouser's existing architecture (C++23, single-threaded nodes)

### Estimated Implementation Effort

- **Phase 1** (Fleet Adapter Foundation): 2-3 weeks
- **Phase 2** (Task Management): 2-3 weeks
- **Phase 3** (VDA5050 Interface): 2 weeks
- **Phase 4** (Monitoring/Visualization): 2-3 weeks

**Total**: 8-11 weeks for complete fleet management system

### Standards Compliance

Dual-standard approach recommended:
- **Open-RMF**: Facility-level orchestration
- **VDA5050**: Vehicle-level protocol compliance

This provides maximum flexibility for both internal Warehouser deployments and external integrations with industry-standard fleet management systems.
