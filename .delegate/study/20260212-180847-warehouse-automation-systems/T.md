# Template

Created: 2026-02-12T18:15:00Z

## Source

Analysis of multiple open-source warehouse automation systems:
- OpenBoxes WMS (https://github.com/openboxes/openboxes)
- Odoo WMS (https://github.com/OCA/wms)
- RWARE Multi-Robot Warehouse Environment (https://github.com/semitable/robotic-warehouse)
- OR-Gym Operations Research Environments (https://github.com/hubbs5/or-gym)
- MAPF Benchmarks (https://movingai.com/benchmarks/mapf.html)
- ROS 2 Multi-Robot Book RMF Tasks (https://osrf.github.io/ros2multirobotbook/task.html)
- Warehouse Layout Generator (https://j4n1k.com/posts/layout/)

## Pattern

### 1. WMS REST API Patterns (OpenBoxes, Odoo)

**Generic CRUD Pattern:**
```
GET    /api/generic/{resource}           # List all
GET    /api/generic/{resource}/{id}      # Read one
POST   /api/generic/{resource}           # Create
PUT    /api/generic/{resource}/{id}      # Update
DELETE /api/generic/{resource}/{id}      # Delete
```

**Core Resources:**
- `shipment` / `shipmentItem`
- `requisition` / `requisitionItem` (order)
- `product` (SKU)
- `inventoryItem`
- `transaction` / `transactionEntry`
- `location`

**JSON Schema Example (Shipment):**
```json
{
  "id": "uuid",
  "name": "string",
  "status": "PENDING|IN_PROGRESS|COMPLETED",
  "origin": { "id": "location-uuid", "name": "Warehouse A" },
  "destination": { "id": "location-uuid", "name": "Packing Zone 1" },
  "expectedShippingDate": "2026-02-12T10:00:00Z",
  "actualShippingDate": "2026-02-12T10:15:32Z",
  "shipmentItems": [
    {
      "id": "item-uuid",
      "inventoryItem": { "id": "sku-123", "product": { "name": "Widget" } },
      "quantity": 5,
      "container": { "id": "bin-uuid", "name": "BIN-A-001" }
    }
  ]
}
```

**Order/Requisition Pattern:**
```json
{
  "id": "order-uuid",
  "orderNumber": "ORD-2026-001234",
  "status": "PENDING|PICKING|PACKING|SHIPPED",
  "priority": "NORMAL|HIGH|URGENT",
  "createdAt": "2026-02-12T09:00:00Z",
  "dueBy": "2026-02-12T17:00:00Z",
  "items": [
    {
      "sku": "SKU-12345",
      "productName": "Widget Pro",
      "quantity": 10,
      "location": { "zone": "A", "aisle": "3", "shelf": "2", "bin": "5" }
    }
  ]
}
```

### 2. Location Data Models (Odoo Pattern)

**Location Types:**
- `VIEW`: Organizational hierarchy (e.g., WH groups all internal locations)
- `INTERNAL`: Physical storage locations (counted in inventory)
- `CUSTOMER`: Sold items destination (not in stock)
- `SUPPLIER`: Vendor locations
- `INVENTORY_LOSS`: Adjustment/scrap counterpart
- `TRANSIT`: Inter-warehouse movement tracking

**Location Schema:**
```json
{
  "id": "loc-uuid",
  "name": "A-03-02-05",
  "type": "INTERNAL",
  "parent": { "id": "zone-a-uuid", "name": "Zone A" },
  "warehouse": { "id": "wh-uuid", "name": "Main Warehouse" },
  "coordinates": { "x": 12.5, "y": 8.3, "z": 2.0 },
  "capacity": { "volume_m3": 0.5, "weight_kg": 50 },
  "currentOccupancy": 0.7,
  "removalStrategy": "FIFO|FEFO|NEAREST|LEFO"
}
```

**Warehouse Structure:**
```
Warehouse (physical building)
├── Zone A (receiving)
│   ├── Dock 1
│   └── Staging Area
├── Zone B (active picking)
│   ├── Aisle 1
│   │   ├── Shelf 1-1 (locations: A-01-01-01 to A-01-01-10)
│   │   └── Shelf 1-2
│   └── Aisle 2
└── Zone C (packing)
    ├── Packing Station 1
    └── Packing Station 2
```

### 3. Task/Order Message Patterns (RMF + WES)

**Task Types (from RMF):**
- `PICK`: Retrieve item from storage location
- `TRANSPORT`: Move item between locations
- `DELIVERY`: Complete pick + transport to destination
- `PUTAWAY`: Store received items
- `CHARGE`: Battery charging task
- `CLEAN`: Floor cleaning (specialized)
- `LOOP`: Repeated navigation between locations

**Task Message Structure:**
```json
{
  "task_id": "task-uuid",
  "task_type": "DELIVERY",
  "status": "PENDING|ASSIGNED|IN_PROGRESS|COMPLETED|FAILED|CANCELED",
  "priority": 1,
  "assigned_robot": "robot-001",
  "created_at": "2026-02-12T10:00:00Z",
  "started_at": "2026-02-12T10:00:15Z",
  "completed_at": "2026-02-12T10:05:42Z",
  "estimated_duration_sec": 300,
  "actual_duration_sec": 327,
  "payload": {
    "pickup_location": { "x": 10.5, "y": 8.2, "zone": "A" },
    "dropoff_location": { "x": 45.3, "y": 12.1, "zone": "C" },
    "item": { "sku": "SKU-12345", "quantity": 1 }
  }
}
```

**Bidding System Pattern (RMF):**
```
1. Task Request arrives at Dispatcher
2. Dispatcher broadcasts BidNotice to all fleet adapters
3. Each fleet adapter:
   - Evaluates robot availability
   - Estimates cost (travel time, battery, current tasks)
   - Sends BidProposal back
4. Dispatcher selects best bid (lowest cost or fastest completion)
5. Dispatcher sends DispatchRequest to winning fleet
6. Fleet adapter assigns task to specific robot
```

**Multi-Agent Task Messages (ROS 2 compatible):**
```python
# TaskRequest.msg
string task_id
uint8 task_type  # PICK=0, TRANSPORT=1, DELIVERY=2, PUTAWAY=3, CHARGE=4
uint8 priority   # 0=low, 1=normal, 2=high, 3=urgent
geometry_msgs/Pose pickup_location
geometry_msgs/Pose dropoff_location
string item_sku
uint32 quantity
time deadline

# TaskStatus.msg
string task_id
string robot_id
uint8 status  # PENDING=0, ASSIGNED=1, IN_PROGRESS=2, COMPLETED=3, FAILED=4
float32 progress_pct
time estimated_completion
string[] error_messages

# BidProposal.msg
string task_id
string robot_id
float32 cost_estimate  # seconds to complete
float32 battery_remaining_pct
geometry_msgs/Pose current_position
uint32 current_task_queue_length
```

### 4. Multi-Robot Warehouse Environment (RWARE)

**Environment Creation:**
```python
import gymnasium as gym
import rware

# Pre-configured sizes: tiny, small, medium, large
# Agent counts: 2ag, 4ag, 8ag, etc.
env = gym.make("rware-tiny-2ag-v2")

# Custom configuration
env = gym.make(
    "rware-tiny-2ag-v2",
    sensor_range=3,           # 3x3 observation window
    request_queue_size=6,     # Max 6 shelf delivery requests
)

# Custom layout (. = floor, x = shelf, g = goal)
layout = """
.......
...x...
..x.x..
.x...x.
..x.x..
...x...
.g...g.
"""
env = gym.make("rware:rware-tiny-2ag-v2", layout=layout)
```

**Action Space (Discrete):**
```python
# 5 actions per agent
0: Turn Left
1: Turn Right
2: Forward
3: Load Shelf (if at shelf location)
4: Unload Shelf (if carrying shelf and at goal)

# For multi-agent
actions = env.action_space.sample()  # Returns: (action_robot1, action_robot2)
```

**Observation Space:**
```python
# Per-agent partial observation
# 3x3 grid centered on agent (configurable via sensor_range)
# Each cell encodes:
# - Empty floor
# - Wall/obstacle
# - Other robot
# - Shelf (requested vs. non-requested)
# - Goal location

obs_shape = (sensor_range * 2 + 1, sensor_range * 2 + 1, n_features)
```

**Reward Structure:**
```python
# +1.0 for successfully delivering requested shelf to goal
# 0.0 for other actions
# Shared reward: all agents receive same reward

# Step returns
observations, rewards, done, truncated, info = env.step(actions)
# rewards = [0.0, 1.0]  # robot 0 idle, robot 1 delivered shelf
```

### 5. Warehouse Grid Layout Generation

**Layout Generator (Numpy-based):**
```python
import numpy as np

def gen_layout(n_rows=10, n_columns=10, n_levels=3, double_deep=True):
    """
    Generate warehouse grid layout.

    Args:
        n_rows: Length of warehouse (meters or cells)
        n_columns: Width of warehouse
        n_levels: Height (vertical storage levels)
        double_deep: If True, use double-deep storage (more compact)

    Returns:
        numpy array where:
        - -1 = storage location
        - 0 = walkable aisle
    """
    if double_deep:
        # Every 3rd column is aisle, rest are storage
        layout_grid = np.ones((n_rows, n_columns, n_levels)) * -1
        for i in range(n_levels):
            layout_grid[:, ::3, i] = 0  # Aisles every 3 columns
    else:
        # Alternating columns: aisle, storage, aisle, storage
        layout_grid = np.zeros((n_rows, n_columns, n_levels))
        layout_grid[:, ::2] = -1  # Storage in even columns

    # First row always walkable (main aisle)
    layout_grid[0, :] = 0
    return layout_grid

def gen_storage_locs(layout_grid):
    """Extract all storage location coordinates."""
    storage_locs = []
    all_locations = np.where(layout_grid == -1)
    for loc in range(len(all_locations[0])):
        x_storage = all_locations[0][loc]
        y_storage = all_locations[1][loc]
        z_storage = all_locations[2][loc]
        storage_locs.append((x_storage, y_storage, z_storage))
    return storage_locs

def gen_access_mapping(layout_grid):
    """
    Map each storage location to its nearest aisle access point.
    Used for calculating pick path distances.
    """
    access_mapping = {}
    all_locations = np.where(layout_grid == -1)

    for loc in range(len(all_locations[0])):
        x_storage = all_locations[0][loc]
        y_storage = all_locations[1][loc]
        z_storage = all_locations[2][loc]

        # Check left and right for nearest aisle
        if layout_grid[x_storage, y_storage - 1, 0] == 0:
            access = (x_storage, y_storage - 1, 0)
            aisle = access[1]
        elif layout_grid[x_storage, y_storage + 1, 0] == 0:
            access = (x_storage, y_storage + 1, 0)
            aisle = access[1]

        access_mapping[(x_storage, y_storage, z_storage)] = {
            "aisle": aisle,
            "access": access
        }
    return access_mapping
```

**Usage Example:**
```python
# Generate 20m x 15m warehouse with 3 shelf levels
layout = gen_layout(n_rows=20, n_columns=15, n_levels=3, double_deep=True)

# Get all 450 storage locations
storage_locs = gen_storage_locs(layout)
print(f"Total storage locations: {len(storage_locs)}")

# Map locations to aisles for pathfinding
access_map = gen_access_mapping(layout)

# Example: Find aisle for location (5, 4, 2)
loc = (5, 4, 2)
aisle_info = access_map[loc]
print(f"Location {loc} accessed via aisle {aisle_info['aisle']}")
print(f"Access point: {aisle_info['access']}")
```

### 6. Performance Metrics (Industry KPIs)

**Message Definition for Real-Time KPIs:**
```python
# PerformanceMetrics.msg (ROS 2)
# Header
std_msgs/Header header

# Throughput Metrics
float32 picks_per_hour              # Current: items picked/hour
float32 orders_per_hour             # Orders completed/hour
float32 avg_order_cycle_time_sec    # Time from order creation to completion

# Efficiency Metrics
float32 robot_utilization_pct       # % time robots productive (not idle/charging)
float32 travel_distance_ratio       # Actual travel / optimal travel (1.0 = perfect)
float32 task_completion_rate        # Completed tasks / assigned tasks

# Quality Metrics
float32 pick_accuracy_pct           # Correct picks / total picks (target: 99.8%)
float32 on_time_delivery_pct        # Orders completed before deadline

# Fleet Status
uint32 active_robots                # Robots currently working
uint32 idle_robots                  # Robots available but unassigned
uint32 charging_robots              # Robots charging
uint32 failed_robots                # Robots with errors

# Queue Depths
uint32 pending_tasks                # Tasks waiting for assignment
uint32 pending_orders               # Orders waiting for task decomposition
float32 avg_task_wait_time_sec      # Time tasks wait before assignment

# Benchmarks (from industry research)
# - Pick rate: 400-600 picks/hour (automated), 30-300 units/labor-hour
# - Robot utilization: >80% (target <20% downtime)
# - Pick accuracy: 99.8%+ (best-in-class: 99.9%)
# - Order cycle time: same-day or next-day
```

### 7. MAPF Benchmark Integration

**Warehouse Map Formats (Moving AI Lab):**
```
# .map file format (ASCII grid)
type octile
height 33
width 57
map
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@.....................................................@
@..@@@@..@@@@..@@@@..@@@@..@@@@..@@@@..@@@@..@@@@..@@.@
@..@@@@..@@@@..@@@@..@@@@..@@@@..@@@@..@@@@..@@@@..@@.@
@.....................................................@
@.....................................................@
...

Legend:
@ = obstacle/shelf
. = free space/aisle
T = traversable (agents can pass)
```

**Scenario File (.scen):**
```
version 1
bucket X  # difficulty bucket
map_width start_x start_y goal_x goal_y optimal_length
```

**Integration with Warehouser:**
- Use MAPF benchmarks for multi-robot coordination testing
- Validate path planning against 25x2 benchmark sets per map
- Test scaling: 4 warehouse maps with up to 1,000 agents (38.9% density)
- Measure: sum of costs, makespan, conflict counts

### 8. Workflow Patterns

**Zone Picking (Pick-and-Pass):**
```python
class ZonePickingWorkflow:
    def __init__(self, zones, robots_per_zone):
        self.zones = zones  # ["A", "B", "C", "D"]
        self.assignments = {zone: [] for zone in zones}

    def assign_order(self, order):
        """
        Break order into tasks per zone.
        Robot in Zone A picks items, passes to Zone B, etc.
        """
        tasks_by_zone = self._decompose_by_zone(order)

        for zone, tasks in tasks_by_zone.items():
            # Assign to least-busy robot in zone
            robot = self._get_available_robot(zone)
            robot.assign_tasks(tasks)

    def _decompose_by_zone(self, order):
        tasks = {}
        for item in order.items:
            zone = item.location.zone
            if zone not in tasks:
                tasks[zone] = []
            tasks[zone].append({
                "type": "PICK",
                "sku": item.sku,
                "location": item.location,
                "quantity": item.quantity
            })
        return tasks
```

**Wave Picking (Time-Based Batching):**
```python
class WavePickingScheduler:
    def __init__(self, wave_interval_minutes=30):
        self.wave_interval = wave_interval_minutes
        self.current_wave = []
        self.next_release_time = None

    def add_order(self, order):
        """Add order to current wave."""
        self.current_wave.append(order)

    def release_wave(self):
        """
        Release all orders in wave at scheduled time.
        Typically aligned with carrier pickup times.
        """
        if time.now() >= self.next_release_time:
            # Batch all orders for optimal routing
            batched_tasks = self._optimize_batch(self.current_wave)

            # Distribute to robots
            for robot_id, tasks in batched_tasks.items():
                self._dispatch_to_robot(robot_id, tasks)

            # Reset wave
            self.current_wave = []
            self.next_release_time = time.now() + timedelta(minutes=self.wave_interval)

    def _optimize_batch(self, orders):
        """
        Group orders with common SKUs/locations.
        Minimize total travel distance across fleet.
        """
        # Implementation: TSP, clustering, or RL-based optimization
        pass
```

**Batch Picking (Multi-Order):**
```python
def create_batch_pick_tasks(orders, max_batch_size=8):
    """
    Group multiple orders to reduce trips.

    Example: Pick SKU-001 x10 for 10 different orders in one trip,
    then sort at packing station.
    """
    # Group by SKU
    sku_requests = defaultdict(list)
    for order in orders:
        for item in order.items:
            sku_requests[item.sku].append({
                "order_id": order.id,
                "quantity": item.quantity,
                "location": item.location
            })

    # Create pick tasks
    tasks = []
    for sku, requests in sku_requests.items():
        total_qty = sum(r["quantity"] for r in requests)
        location = requests[0]["location"]  # Assume SKUs stored together

        tasks.append({
            "type": "BATCH_PICK",
            "sku": sku,
            "location": location,
            "total_quantity": total_qty,
            "orders": [r["order_id"] for r in requests]
        })

    return tasks
```

### 9. SKU/Inventory Data Models

**Product Schema:**
```json
{
  "id": "sku-uuid",
  "sku": "SKU-12345",
  "name": "Widget Pro",
  "description": "High-performance widget",
  "category": "Electronics",
  "dimensions": {
    "length_cm": 20,
    "width_cm": 15,
    "height_cm": 10,
    "weight_kg": 0.5,
    "volume_m3": 0.003
  },
  "storage_requirements": {
    "temperature_range_c": { "min": 15, "max": 25 },
    "humidity_max_pct": 60,
    "stackable": true,
    "fragile": false
  },
  "velocity_class": "A",  // A = fast-moving, B = medium, C = slow
  "unit_cost": 12.50,
  "barcode": "123456789012",
  "hazmat": false
}
```

**Inventory Item Schema:**
```json
{
  "id": "inv-item-uuid",
  "sku": "SKU-12345",
  "location": "A-03-02-05",
  "quantity": 24,
  "lot_number": "LOT-2026-001",
  "expiration_date": "2027-02-12",
  "received_date": "2026-01-15",
  "status": "AVAILABLE|RESERVED|DAMAGED|QUARANTINE",
  "reserved_quantity": 5,  // Reserved for pending orders
  "available_quantity": 19
}
```

## Application

### For Warehouser Project

#### 1. Implement WMS-like REST API

Add to `warehouser_msgs`:
```python
# warehouser_msgs/msg/Order.msg
std_msgs/Header header
string order_id
string order_number
uint8 status  # PENDING=0, PICKING=1, PACKING=2, SHIPPED=3
uint8 priority  # NORMAL=0, HIGH=1, URGENT=2
time created_at
time due_by
OrderItem[] items

# warehouser_msgs/msg/OrderItem.msg
string sku
string product_name
uint32 quantity
StorageLocation location

# warehouser_msgs/msg/StorageLocation.msg
string location_id
string zone
uint32 aisle
uint32 shelf
uint32 bin
geometry_msgs/Point coordinates

# warehouser_msgs/msg/Task.msg
string task_id
uint8 task_type  # PICK=0, TRANSPORT=1, DELIVERY=2, PUTAWAY=3, CHARGE=4
uint8 status  # PENDING=0, ASSIGNED=1, IN_PROGRESS=2, COMPLETED=3, FAILED=4
string assigned_robot
time created_at
time started_at
time completed_at
float32 estimated_duration_sec
TaskPayload payload

# warehouser_msgs/msg/TaskPayload.msg
geometry_msgs/Pose pickup_location
geometry_msgs/Pose dropoff_location
string item_sku
uint32 quantity

# warehouser_msgs/msg/PerformanceMetrics.msg
std_msgs/Header header
float32 picks_per_hour
float32 robot_utilization_pct
float32 avg_order_cycle_time_sec
float32 pick_accuracy_pct
uint32 active_robots
uint32 idle_robots
uint32 charging_robots
uint32 pending_tasks
```

#### 2. Create Warehouse Layout Manager

```cpp
// ros_simulation/include/ros_simulation/warehouse_layout.hpp
#pragma once
#include <vector>
#include <unordered_map>
#include <Eigen/Dense>

namespace warehouser {

enum class CellType : uint8_t {
    OBSTACLE = 0,
    AISLE = 1,
    STORAGE = 2,
    GOAL = 3,
    CHARGING = 4
};

struct StorageLocation {
    std::string id;
    Eigen::Vector3f position;  // x, y, z (level)
    std::string zone;
    uint32_t aisle;
    uint32_t shelf;
    uint32_t bin;
    float capacity_m3;
    float occupied_m3;
};

class WarehouseLayout {
public:
    WarehouseLayout(uint32_t rows, uint32_t columns, uint32_t levels, bool double_deep);

    void generate();
    std::vector<StorageLocation> get_storage_locations() const;
    std::unordered_map<std::string, Eigen::Vector2f> get_access_mapping() const;
    CellType get_cell_type(uint32_t row, uint32_t col, uint32_t level) const;

    // For ROS publishing
    nav_msgs::msg::OccupancyGrid to_occupancy_grid() const;

private:
    uint32_t rows_;
    uint32_t cols_;
    uint32_t levels_;
    bool double_deep_;
    std::vector<std::vector<std::vector<CellType>>> grid_;
    std::vector<StorageLocation> storage_locations_;
};

} // namespace warehouser
```

#### 3. Add WMS Node

```python
# ros_simulation/ros_simulation/wms_node.py
import rclpy
from rclpy.node import Node
from warehouser_msgs.msg import Order, Task, PerformanceMetrics
from warehouser_msgs.srv import CreateOrder, AssignTask
import random
from datetime import datetime, timedelta

class WMSNode(Node):
    """
    Simplified Warehouse Management System node.
    Generates orders and tracks performance metrics.
    """
    def __init__(self):
        super().__init__('wms_node')

        # Publishers
        self.metrics_pub = self.create_publisher(PerformanceMetrics, '/wms/metrics', 10)

        # Services
        self.create_order_srv = self.create_service(
            CreateOrder, '/wms/create_order', self.create_order_callback
        )

        # State
        self.active_orders = {}
        self.completed_orders = []
        self.metrics = PerformanceMetrics()

        # Timers
        self.create_timer(10.0, self.generate_random_order)
        self.create_timer(1.0, self.publish_metrics)

    def generate_random_order(self):
        """Generate random order every 10 seconds for simulation."""
        order = Order()
        order.order_id = f"ORD-{datetime.now().strftime('%Y%m%d%H%M%S')}"
        order.order_number = order.order_id
        order.status = 0  # PENDING
        order.priority = random.choice([0, 1, 2])
        order.created_at = self.get_clock().now().to_msg()
        order.due_by = (datetime.now() + timedelta(hours=2)).timestamp()

        # Add 1-5 random items
        num_items = random.randint(1, 5)
        for i in range(num_items):
            item = OrderItem()
            item.sku = f"SKU-{random.randint(10000, 99999)}"
            item.product_name = f"Product {i}"
            item.quantity = random.randint(1, 10)
            # Location would be assigned from actual warehouse layout
            order.items.append(item)

        self.active_orders[order.order_id] = order
        self.get_logger().info(f"Created order {order.order_id} with {num_items} items")

    def publish_metrics(self):
        """Publish performance metrics every second."""
        self.metrics.header.stamp = self.get_clock().now().to_msg()
        # Calculate real metrics based on completed orders, robot states, etc.
        self.metrics_pub.publish(self.metrics)
```

#### 4. Extend RL Environment with Order-Based Rewards

```python
# training/training/envs/ros_env.py (additions)
class WarehouseEnv(gym.Env):
    def __init__(self, config):
        # Existing init...

        # Subscribe to orders and tasks
        self.order_subscriber = self.node.create_subscription(
            Order, '/wms/orders', self.order_callback, 10
        )
        self.active_orders = {}
        self.order_start_times = {}

    def order_callback(self, msg):
        """Track new orders for cycle time rewards."""
        self.active_orders[msg.order_id] = msg
        self.order_start_times[msg.order_id] = time.time()

    def _compute_reward(self, obs, action, next_obs):
        """
        Reward structure aligned with industry KPIs:
        - Order completion: +10.0
        - Pick success: +1.0
        - Minimize order cycle time: +(1.0 / cycle_time_minutes)
        - Idle penalty: -0.01 per step
        - Collision: -1.0
        """
        reward = 0.0

        # Check for completed orders
        for order_id, order in self.active_orders.items():
            if self._is_order_complete(order):
                cycle_time = time.time() - self.order_start_times[order_id]
                reward += 10.0  # Base completion reward
                reward += (1.0 / (cycle_time / 60.0))  # Faster = better
                del self.active_orders[order_id]

        # Penalize idle robots
        if self._is_robot_idle():
            reward -= 0.01

        return reward
```

#### 5. Integrate RWARE-style Multi-Agent Coordination

```python
# training/training/envs/multi_robot_env.py
from pettingzoo import ParallelEnv
import numpy as np

class MultiRobotWarehouseEnv(ParallelEnv):
    """
    PettingZoo environment similar to RWARE but using Warehouser simulation.
    """
    def __init__(self, num_robots=4, sensor_range=3):
        super().__init__()
        self.possible_agents = [f"robot_{i}" for i in range(num_robots)]
        self.sensor_range = sensor_range

        # Action space: 5 actions like RWARE
        # 0: turn left, 1: turn right, 2: forward, 3: pick, 4: drop
        self.action_spaces = {
            agent: spaces.Discrete(5) for agent in self.possible_agents
        }

        # Observation space: 3x3 grid around robot
        grid_size = sensor_range * 2 + 1
        self.observation_spaces = {
            agent: spaces.Box(
                low=0, high=255,
                shape=(grid_size, grid_size, 4),  # 4 channels: robots, shelves, goals, walls
                dtype=np.uint8
            )
            for agent in self.possible_agents
        }

    def reset(self, seed=None):
        # Reset ROS simulation
        observations = {}
        for agent in self.agents:
            observations[agent] = self._get_local_observation(agent)
        return observations, {}

    def step(self, actions):
        # Execute all actions in parallel
        # Return per-agent observations, rewards, dones
        pass

    def _get_local_observation(self, agent):
        """
        Extract 3x3 grid around agent, similar to RWARE.
        Encode: walls, other robots, shelves (carrying vs. requested), goals.
        """
        pass
```

#### 6. Add Warehouse Visualization to Frontend

```typescript
// web_frontend/src/components/WarehouseGrid.tsx
import React from 'react';
import { Canvas } from '@react-three/fiber';
import { OrbitControls, Box } from '@react-three/drei';

interface StorageLocation {
  id: string;
  position: [number, number, number];
  occupancy: number;
  zone: string;
}

interface WarehouseGridProps {
  layout: number[][][];  // 3D grid from layout generator
  storageLocations: StorageLocation[];
  robots: RobotState[];
}

export const WarehouseGrid: React.FC<WarehouseGridProps> = ({
  layout, storageLocations, robots
}) => {
  return (
    <Canvas camera={{ position: [50, 50, 50], fov: 50 }}>
      <ambientLight intensity={0.5} />
      <pointLight position={[10, 10, 10]} />
      <OrbitControls />

      {/* Render aisles */}
      {layout.map((row, i) =>
        row.map((col, j) =>
          col.map((cell, k) => {
            if (cell === 0) {  // Aisle
              return (
                <Box
                  key={`${i}-${j}-${k}`}
                  position={[i, k, j]}
                  args={[0.9, 0.1, 0.9]}
                >
                  <meshStandardMaterial color="#cccccc" />
                </Box>
              );
            }
            return null;
          })
        )
      )}

      {/* Render storage locations */}
      {storageLocations.map(loc => (
        <Box
          key={loc.id}
          position={loc.position}
          args={[0.8, 2.0, 0.8]}
        >
          <meshStandardMaterial
            color={`hsl(${120 * (1 - loc.occupancy)}, 70%, 50%)`}
            opacity={0.7}
            transparent
          />
        </Box>
      ))}

      {/* Render robots */}
      {robots.map(robot => (
        <RobotMesh key={robot.id} robot={robot} />
      ))}
    </Canvas>
  );
};
```

#### 7. Performance Dashboard

```typescript
// web_frontend/src/components/PerformanceMetrics.tsx
import React from 'react';
import { useMetrics } from '../hooks/useMetrics';

export const PerformanceMetrics: React.FC = () => {
  const metrics = useMetrics('/wms/metrics');

  const benchmarks = {
    picksPerHour: { target: 500, current: metrics.picks_per_hour },
    robotUtilization: { target: 80, current: metrics.robot_utilization_pct },
    pickAccuracy: { target: 99.8, current: metrics.pick_accuracy_pct },
    orderCycleTime: { target: 300, current: metrics.avg_order_cycle_time_sec }
  };

  return (
    <div className="metrics-dashboard">
      <MetricCard
        title="Picks/Hour"
        value={benchmarks.picksPerHour.current}
        target={benchmarks.picksPerHour.target}
        unit="picks/hr"
        benchmark="Industry: 400-600"
      />
      <MetricCard
        title="Robot Utilization"
        value={benchmarks.robotUtilization.current}
        target={benchmarks.robotUtilization.target}
        unit="%"
        benchmark="Target: >80%"
      />
      <MetricCard
        title="Pick Accuracy"
        value={benchmarks.pickAccuracy.current}
        target={benchmarks.pickAccuracy.target}
        unit="%"
        benchmark="Best-in-Class: 99.8%"
      />
      <MetricCard
        title="Order Cycle Time"
        value={benchmarks.orderCycleTime.current}
        target={benchmarks.orderCycleTime.target}
        unit="sec"
        benchmark="Target: <5min"
      />

      <FleetStatus
        active={metrics.active_robots}
        idle={metrics.idle_robots}
        charging={metrics.charging_robots}
      />
    </div>
  );
};
```

### Direct Code Snippets for Integration

**Immediate next steps:**

1. Copy `gen_layout()` function to create `ros_simulation/scripts/generate_warehouse.py`
2. Add message definitions from Section 1 to `warehouser_msgs/msg/`
3. Create `wms_node.py` as simple order generator for testing
4. Update reward function in `training/training/envs/ros_env.py` to use order-based rewards
5. Add `PerformanceMetrics` subscriber to `web_frontend` for dashboard

**Testing against industry patterns:**
- Generate warehouse with 100+ storage locations
- Spawn 4 robots, generate 10 orders/minute
- Measure picks/hour, compare to 400-600 benchmark
- Visualize zone utilization, identify bottlenecks
- Test wave picking: batch 20 orders, release every 5 minutes

**Reference implementations to study:**
- RWARE: Multi-agent coordination patterns
- OpenBoxes: REST API design for warehouse operations
- Odoo: Location hierarchy and removal strategies
- MAPF benchmarks: Scalability testing with 100s of robots

## Sources

- [OpenBoxes GitHub Repository](https://github.com/openboxes/openboxes)
- [OpenBoxes API Guide](https://github.com/openboxes/openboxes/blob/develop/docs/api-guide/getting-started.md)
- [Odoo WMS Documentation](https://www.odoo.com/documentation/19.0/applications/inventory_and_mrp/inventory.html)
- [OCA WMS GitHub](https://github.com/OCA/wms)
- [RWARE Multi-Robot Warehouse Environment](https://github.com/semitable/robotic-warehouse)
- [OR-Gym PyPI](https://pypi.org/project/or-gym/)
- [OR-Gym GitHub](https://github.com/hubbs5/or-gym)
- [MAPF Benchmarks - Moving AI](https://movingai.com/benchmarks/mapf.html)
- [Multi-Agent Pathfinding Paper](https://arxiv.org/pdf/1906.08291)
- [RMF Tasks Documentation](https://osrf.github.io/ros2multirobotbook/task.html)
- [Warehouse Layout Generator](https://j4n1k.com/posts/layout/)
- [Warehouse Layout Generator (Medium)](https://medium.com/@beware-sim/how-to-generate-warehouse-layouts-for-simulation-with-python-and-plotly-fast-a615ec0e1b8d)
- [Erply WMS API](https://learn-api.erply.com/new-apis/wms-api)
- [Microsoft Dynamics 365 Inventory API](https://learn.microsoft.com/en-us/dynamics365/supply-chain/inventory/inventory-visibility-api)
- [Shopify Inventory API](https://www.prediko.io/blog/shopify-inventory-api)
