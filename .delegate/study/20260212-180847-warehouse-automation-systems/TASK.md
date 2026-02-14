# TASK: Implement Warehouse Automation System Integration

Created: 2026-02-12T18:30:00Z
Build: UNKNOWN (C++ build not tested in agent context)
Tests: FAIL (Python tests failing - dependency issues with pydantic)

## Summary

Transform Warehouser from a basic robot navigation simulator into a comprehensive warehouse automation system that reflects real-world industry patterns. This involves adding storage infrastructure (shelves, aisles, bins), multi-item order management, task scheduling for multi-robot coordination, WMS integration layer, and industry-standard performance metrics (throughput, accuracy, cycle time).

## Current State

### Entity Model
Warehouser currently defines 4 entity types:
- Robot: Differential drive with pose (x, y, theta), velocities, binary carrying state
- Object: Simple pickable items with color, pickup_radius, is_picked flag
- Wall: Static rectangular obstacles
- Zone: Circular areas with zone_name and radius

**Critical Gaps:**
- No shelf/rack/bin entities for structured storage
- No storage hierarchy (aisle → shelf → bin → item)
- No station types (charging, packing, sorting)
- Objects lack properties (weight, dimensions, SKU, fragility)

### Task System
Current task workflow:
- Single task execution (pick-and-place)
- States: IDLE → NAVIGATING_TO_PICK → PICKING → NAVIGATING_TO_PLACE → PLACING → COMPLETED
- Commands: `pick {color}`, `goto {x,y}`, `pick_and_place {color} {zone}`

**Gaps:**
- No multi-item orders or pick lists
- No task queue or batching
- No task priority levels
- No resource reservation (multiple robots could target same object)
- No batch/wave/zone picking strategies

### Workflow Implementation
Simple pick-and-place flow:
1. Command reception (JSON parsing)
2. Object resolution by color
3. Navigation to object
4. Pick action
5. Navigation to zone
6. Place action

**Missing Stages:**
- Receiving/putaway workflows
- Packing/shipping stages
- Order fulfillment lifecycle
- Exception handling (item not found, damage, wrong pick)

### Performance Metrics
Current telemetry:
- Sim time
- Robot position
- Carrying status
- Task state

**Industry Gaps:**
- No throughput tracking (picks/hour, orders/hour)
- No accuracy metrics (pick errors)
- No cycle time measurement (order-to-ship duration)
- No robot utilization (idle time, charging time)
- No zone congestion or heat maps

## Industry Context

### Real Warehouse Systems

**Amazon Robotics (Kiva):**
- Goods-to-person (GTP) architecture: robots transport pods to human operators
- Performance: 300-400 items/hour per worker (vs. 100-200 traditional)
- Fleet types: Hercules, Titan, Sequoia, Proteus (autonomous among humans)
- AI integration: DeepFleet for fleet optimization, Project Eluna for workflow efficiency

**Ocado Technology ("The Hive"):**
- 3D grid storage with swarm robotics on top
- Vertical capacity: Up to 21 totes high in modular grid
- Robot movement: 4 m/sec, communicates with AI 10x/second
- Performance: 50-item grocery order in 5 minutes
- Digital twin for testing without disrupting operations

**System Integration Stack:**
```
ERP (Enterprise Resource Planning)
  ↓
WMS (Warehouse Management System) - Inventory & order management
  ↓
WES (Warehouse Execution System) - Real-time workflow coordination
  ↓
WCS (Warehouse Control System) - Equipment control (millisecond responses)
```

**Picking Strategies:**
- Batch Picking: Pick multiple SKUs from multiple orders simultaneously
- Wave Picking: Time-based batches aligned with carrier schedules
- Zone Picking: Warehouse divided into zones, workers specialize per zone
  - Pick-and-Pass: Orders move through zones sequentially
  - Parallel Picking: Independent completion per zone
- Hybrid: Combines batch, zone, wave methods for large warehouses

**Industry KPIs (2025 Benchmarks):**
- Pick Rate: 400-600 picks/hour (automated), 30-300 units/labor-hour
- Robot Utilization: >80% (target <20% downtime)
- Pick Accuracy: 99.8%+ (best-in-class: 99.9%)
- Order Cycle Time: Same-day or next-day processing
- Perfect Order Rate: 97-98% (best-in-class)
- Dock-to-Stock: <2 hours (best-in-class)

## Implementation Plan

### Phase 1: Storage Infrastructure (Foundation)

**Priority:** HIGH | **Effort:** MEDIUM | **Duration:** 1-2 weeks

#### 1.1 Extend Entity Model

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\Entity.msg`

Add shelf entity type:
```
uint8 TYPE_SHELF = 4
uint8 TYPE_STATION = 5

# For TYPE_SHELF
uint32 num_rows        # Number of vertical rows
uint32 num_columns     # Number of horizontal columns
float32 slot_width     # Width of each storage slot
float32 slot_height    # Height of each storage slot
string zone_id         # Zone this shelf belongs to
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\StorageLocation.msg` (NEW)

```
string location_id       # e.g., "A-03-02-05"
string zone              # e.g., "A"
uint32 aisle             # Aisle number
uint32 shelf             # Shelf number
uint32 bin               # Bin number
geometry_msgs/Point coordinates
float32 capacity_m3
float32 occupied_m3
uint8 location_type      # STORAGE=0, PICKING=1, PACKING=2, SHIPPING=3
```

#### 1.2 Warehouse Layout Generator

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\include\warehouser_simulation\warehouse_layout.hpp` (NEW)

```cpp
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

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_simulation\src\warehouse_layout.cpp` (NEW)

Implement layout generation based on RWARE/template patterns:
- Double-deep storage: Every 3rd column is aisle
- Single-deep: Alternating aisle/storage columns
- First row always walkable (main aisle)
- Return storage locations with (x,y,z) → (aisle, shelf, bin) mapping

#### 1.3 Update World Configuration

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_bringup\config\world.yaml`

Replace simple 10x10m world with structured warehouse:
```yaml
warehouse:
  layout:
    rows: 20
    columns: 15
    levels: 3
    double_deep: true

  zones:
    - name: "receiving"
      type: "RECEIVING"
      position: [2.0, 2.0]
      radius: 3.0

    - name: "zone_a"
      type: "STORAGE"
      aisles: [1, 2, 3, 4, 5]
      position: [10.0, 5.0]

    - name: "packing"
      type: "PACKING"
      position: [18.0, 12.0]
      radius: 2.0

    - name: "shipping"
      type: "SHIPPING"
      position: [18.0, 2.0]
      radius: 2.0

  shelves:
    # Auto-generated from layout
    - zone: "zone_a"
      aisle: 1
      shelves_per_aisle: 10

  robot_spawns:
    - position: [1.0, 1.0, 0.0]
    - position: [1.0, 2.0, 0.0]
    - position: [1.0, 3.0, 0.0]
    - position: [1.0, 4.0, 0.0]
```

### Phase 2: Multi-Item Order System

**Priority:** HIGH | **Effort:** HIGH | **Duration:** 2-3 weeks

#### 2.1 Order Message Definitions

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\Order.msg` (NEW)

```
std_msgs/Header header
string order_id
string order_number
uint8 status  # PENDING=0, PICKING=1, PACKING=2, SHIPPED=3, CANCELLED=4
uint8 priority  # NORMAL=0, HIGH=1, URGENT=2
builtin_interfaces/Time created_at
builtin_interfaces/Time due_by
OrderItem[] items
string destination_zone
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\OrderItem.msg` (NEW)

```
string sku
string product_name
uint32 quantity
StorageLocation location
float32 weight_kg
bool is_picked
string picked_by_robot
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\Task.msg` (NEW)

```
string task_id
uint8 task_type  # PICK=0, TRANSPORT=1, DELIVERY=2, PUTAWAY=3, CHARGE=4
uint8 status  # PENDING=0, ASSIGNED=1, IN_PROGRESS=2, COMPLETED=3, FAILED=4, CANCELLED=5
string assigned_robot
string order_id
builtin_interfaces/Time created_at
builtin_interfaces/Time started_at
builtin_interfaces/Time completed_at
float32 estimated_duration_sec
TaskPayload payload
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\TaskPayload.msg` (NEW)

```
geometry_msgs/Pose pickup_location
geometry_msgs/Pose dropoff_location
string item_sku
uint32 quantity
float32 item_weight_kg
```

#### 2.2 WMS Node (Order Generator)

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_wms\warehouser_wms\wms_node.py` (NEW PACKAGE)

```python
import rclpy
from rclpy.node import Node
from warehouser_msgs.msg import Order, OrderItem, Task, PerformanceMetrics
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
        self.order_pub = self.create_publisher(Order, '/wms/orders', 10)
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
        self.create_timer(10.0, self.generate_random_order)  # Every 10 seconds
        self.create_timer(1.0, self.publish_metrics)

        self.get_logger().info('WMS Node initialized')

    def generate_random_order(self):
        """Generate random order for simulation."""
        order = Order()
        order.order_id = f"ORD-{datetime.now().strftime('%Y%m%d%H%M%S')}"
        order.order_number = order.order_id
        order.status = 0  # PENDING
        order.priority = random.choice([0, 1, 2])
        order.created_at = self.get_clock().now().to_msg()
        order.due_by = (datetime.now() + timedelta(hours=2)).timestamp()
        order.destination_zone = "packing"

        # Add 1-5 random items
        num_items = random.randint(1, 5)
        for i in range(num_items):
            item = OrderItem()
            item.sku = f"SKU-{random.randint(10000, 99999)}"
            item.product_name = f"Product {i}"
            item.quantity = random.randint(1, 10)
            item.weight_kg = random.uniform(0.1, 5.0)
            item.is_picked = False
            # Location assigned from warehouse layout
            order.items.append(item)

        self.active_orders[order.order_id] = order
        self.order_pub.publish(order)
        self.get_logger().info(f"Created order {order.order_id} with {num_items} items")

    def publish_metrics(self):
        """Publish performance metrics."""
        self.metrics.header.stamp = self.get_clock().now().to_msg()
        self.metrics_pub.publish(self.metrics)
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_wms\package.xml` (NEW)
**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_wms\setup.py` (NEW)

#### 2.3 Update Task Manager

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\include\warehouser_task\task_state_machine.hpp`

Extend Task struct:
```cpp
struct Task {
    std::string task_id;
    std::string order_id;  // NEW: Link to parent order
    std::string intent;
    uint8_t task_type;  // NEW: PICK, TRANSPORT, DELIVERY, PUTAWAY, CHARGE
    uint8_t priority;   // NEW: 0=normal, 1=high, 2=urgent
    std::string target_object_id;
    std::string target_sku;  // NEW: SKU instead of just color
    float object_x, object_y, pickup_radius;
    float dest_x, dest_y, place_radius;
    std::string failure_reason;
    std::chrono::time_point<std::chrono::steady_clock> created_at;  // NEW
    std::chrono::time_point<std::chrono::steady_clock> started_at;  // NEW
};
```

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\include\warehouser_task\task_manager_node.hpp`

Add order queue:
```cpp
class TaskManagerNode : public rclcpp::Node {
private:
    std::queue<warehouser_msgs::msg::Order> order_queue_;  // NEW
    std::unordered_map<std::string, std::vector<Task>> order_tasks_;  // NEW

    void order_callback(const warehouser_msgs::msg::Order::SharedPtr msg);  // NEW
    void decompose_order_to_tasks(const warehouser_msgs::msg::Order& order);  // NEW
    Task select_next_task();  // NEW: Priority-based selection
};
```

### Phase 3: Task Scheduling & Multi-Robot Coordination

**Priority:** MEDIUM | **Effort:** HIGH | **Duration:** 2-3 weeks

#### 3.1 Task Scheduler Package

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_scheduler\include\warehouser_scheduler\task_auction.hpp` (NEW PACKAGE)

Implement RMF-style bidding system:
```cpp
#pragma once
#include <warehouser_msgs/msg/task.hpp>
#include <warehouser_msgs/msg/bid_proposal.hpp>

namespace warehouser {

struct BidProposal {
    std::string task_id;
    std::string robot_id;
    float cost_estimate_sec;
    float battery_remaining_pct;
    Eigen::Vector2f current_position;
    uint32_t current_task_queue_length;
};

class TaskAuction {
public:
    void broadcast_task(const warehouser_msgs::msg::Task& task);
    void receive_bid(const BidProposal& bid);
    std::string select_winner(const std::string& task_id);

private:
    std::unordered_map<std::string, std::vector<BidProposal>> bids_by_task_;
};

} // namespace warehouser
```

#### 3.2 Fleet Coordinator Node

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_scheduler\warehouser_scheduler\fleet_coordinator.py` (NEW)

```python
class FleetCoordinator(Node):
    """
    Coordinates task assignment across multiple robots using auction-based system.
    Similar to RMF dispatcher pattern.
    """
    def __init__(self):
        super().__init__('fleet_coordinator')

        # Subscribe to tasks and robot status
        self.task_sub = self.create_subscription(Task, '/wms/tasks', self.task_callback, 10)
        self.robot_status_sub = self.create_subscription(
            RobotStatus, '/robots/status', self.robot_status_callback, 10
        )

        # Publish bid notices and dispatch requests
        self.bid_notice_pub = self.create_publisher(BidNotice, '/fleet/bid_notice', 10)
        self.dispatch_pub = self.create_publisher(DispatchRequest, '/fleet/dispatch', 10)

        # State
        self.robot_fleet = {}
        self.pending_tasks = {}
        self.bid_proposals = {}

    def task_callback(self, msg):
        """New task arrived, initiate bidding."""
        self.pending_tasks[msg.task_id] = msg
        self.broadcast_bid_notice(msg)

    def broadcast_bid_notice(self, task):
        """Ask all robots to bid on task."""
        notice = BidNotice()
        notice.task_id = task.task_id
        notice.task_type = task.task_type
        notice.pickup_location = task.payload.pickup_location
        notice.dropoff_location = task.payload.dropoff_location
        self.bid_notice_pub.publish(notice)

        # Set timeout for bid collection (e.g., 1 second)
        self.create_timer(1.0, lambda: self.select_winner(task.task_id))

    def select_winner(self, task_id):
        """Select best bid (lowest cost) and dispatch task."""
        if task_id not in self.bid_proposals:
            self.get_logger().warn(f"No bids for task {task_id}")
            return

        bids = self.bid_proposals[task_id]
        winner = min(bids, key=lambda b: b.cost_estimate)

        dispatch = DispatchRequest()
        dispatch.task_id = task_id
        dispatch.robot_id = winner.robot_id
        dispatch.task = self.pending_tasks[task_id]
        self.dispatch_pub.publish(dispatch)

        self.get_logger().info(
            f"Assigned task {task_id} to {winner.robot_id} "
            f"(cost: {winner.cost_estimate:.1f}s)"
        )
```

#### 3.3 Robot Bidding Logic

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_task\src\robot_bidder.cpp` (NEW)

Each robot evaluates task cost:
```cpp
float RobotBidder::estimate_task_cost(const Task& task) {
    // Calculate travel time to pickup
    float distance_to_pickup = calculate_distance(current_position_, task.pickup_location);
    float travel_time = distance_to_pickup / kRobotSpeed;

    // Add current task queue delay
    float queue_delay = current_task_queue_length_ * kAvgTaskDuration;

    // Penalty if battery low (need to charge soon)
    float battery_penalty = (battery_pct_ < 20.0f) ? 100.0f : 0.0f;

    return travel_time + queue_delay + battery_penalty;
}
```

### Phase 4: WMS Integration & REST API

**Priority:** MEDIUM | **Effort:** MEDIUM | **Duration:** 1-2 weeks

#### 4.1 WMS REST API Server

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_wms\warehouser_wms\api_server.py` (NEW)

```python
from flask import Flask, request, jsonify
from flask_cors import CORS
import rclpy
from warehouser_msgs.msg import Order
from warehouser_msgs.srv import CreateOrder

app = Flask(__name__)
CORS(app)

# ROS 2 node runs in background thread
ros_node = None

@app.route('/api/orders', methods=['GET'])
def get_orders():
    """List all orders."""
    # Query from WMS node state
    return jsonify(ros_node.get_all_orders())

@app.route('/api/orders', methods=['POST'])
def create_order():
    """
    Create new order.
    Body: {
        "priority": 0-2,
        "items": [
            {"sku": "SKU-12345", "quantity": 5}
        ],
        "destination_zone": "packing"
    }
    """
    data = request.json

    # Create ROS service request
    req = CreateOrder.Request()
    req.priority = data.get('priority', 0)
    req.items = data['items']
    req.destination_zone = data['destination_zone']

    response = ros_node.create_order_client.call(req)

    return jsonify({
        "order_id": response.order_id,
        "status": "PENDING"
    }), 201

@app.route('/api/inventory', methods=['GET'])
def get_inventory():
    """Query current inventory state."""
    return jsonify(ros_node.get_inventory())

@app.route('/api/robots', methods=['GET'])
def get_robots():
    """List all robots and their status."""
    return jsonify(ros_node.get_robot_fleet_status())

@app.route('/api/metrics', methods=['GET'])
def get_metrics():
    """Get current performance metrics."""
    return jsonify(ros_node.get_current_metrics())

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
```

#### 4.2 Inventory Manager Node

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_wms\warehouser_wms\inventory_manager.py` (NEW)

Track item locations and quantities:
```python
class InventoryManager(Node):
    """
    Tracks inventory locations and quantities.
    Updates on pick/place events.
    """
    def __init__(self):
        super().__init__('inventory_manager')

        # Subscribe to world state for inventory updates
        self.world_sub = self.create_subscription(
            WorldState, '/world/state', self.world_callback, 10
        )

        # Subscribe to task completion for inventory movements
        self.task_sub = self.create_subscription(
            Task, '/tasks/completed', self.task_completed_callback, 10
        )

        # Inventory database (in-memory, could be Redis/PostgreSQL)
        self.inventory = {}  # sku -> list of InventoryItem
        self.location_contents = {}  # location_id -> list of items

        # Service for inventory queries
        self.query_srv = self.create_service(
            QueryInventory, '/wms/query_inventory', self.query_callback
        )

    def task_completed_callback(self, msg):
        """Update inventory when task completes (pick or putaway)."""
        if msg.task_type == TASK_PICK:
            # Remove from source location
            self.remove_from_location(msg.payload.pickup_location, msg.payload.item_sku)
        elif msg.task_type == TASK_PUTAWAY:
            # Add to destination location
            self.add_to_location(msg.payload.dropoff_location, msg.payload.item_sku)
```

### Phase 5: Performance Metrics & KPI Dashboard

**Priority:** MEDIUM | **Effort:** MEDIUM | **Duration:** 1-2 weeks

#### 5.1 Metrics Message Definition

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_msgs\msg\PerformanceMetrics.msg` (NEW)

```
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

# Industry Benchmarks (for comparison)
# - Pick rate: 400-600 picks/hour (automated)
# - Robot utilization: >80% (target <20% downtime)
# - Pick accuracy: 99.8%+ (best-in-class: 99.9%)
# - Order cycle time: same-day or next-day
```

#### 5.2 Analytics Node

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_analytics\warehouser_analytics\analytics_node.py` (NEW PACKAGE)

```python
class AnalyticsNode(Node):
    """
    Tracks and publishes performance metrics.
    """
    def __init__(self):
        super().__init__('analytics_node')

        # Publishers
        self.metrics_pub = self.create_publisher(PerformanceMetrics, '/analytics/metrics', 10)

        # Subscribers
        self.task_sub = self.create_subscription(Task, '/tasks/completed', self.task_callback, 10)
        self.order_sub = self.create_subscription(Order, '/wms/orders', self.order_callback, 10)
        self.robot_sub = self.create_subscription(
            RobotStatus, '/robots/status', self.robot_status_callback, 10
        )

        # State tracking
        self.completed_picks_last_hour = []
        self.completed_orders_last_hour = []
        self.order_start_times = {}
        self.robot_states = {}

        # Timer for metrics calculation
        self.create_timer(1.0, self.calculate_and_publish_metrics)

    def calculate_and_publish_metrics(self):
        """Calculate all KPIs and publish."""
        now = time.time()

        metrics = PerformanceMetrics()
        metrics.header.stamp = self.get_clock().now().to_msg()

        # Throughput
        picks_last_hour = [p for p in self.completed_picks_last_hour if now - p < 3600]
        metrics.picks_per_hour = len(picks_last_hour)

        orders_last_hour = [o for o in self.completed_orders_last_hour if now - o < 3600]
        metrics.orders_per_hour = len(orders_last_hour)

        # Cycle time
        if self.completed_orders_last_hour:
            cycle_times = [
                self.order_completion_times[oid] - self.order_start_times[oid]
                for oid in self.completed_orders_last_hour[-10:]  # Last 10 orders
            ]
            metrics.avg_order_cycle_time_sec = sum(cycle_times) / len(cycle_times)

        # Robot utilization
        total_robots = len(self.robot_states)
        if total_robots > 0:
            active = sum(1 for r in self.robot_states.values() if r['status'] == 'WORKING')
            metrics.active_robots = active
            metrics.idle_robots = sum(1 for r in self.robot_states.values() if r['status'] == 'IDLE')
            metrics.charging_robots = sum(1 for r in self.robot_states.values() if r['status'] == 'CHARGING')
            metrics.robot_utilization_pct = (active / total_robots) * 100.0

        self.metrics_pub.publish(metrics)
```

#### 5.3 Frontend KPI Dashboard

**File:** `C:\Users\costa\src\warehouser\web_frontend\src\components\KPIPanel.tsx` (NEW)

```typescript
import React from 'react';
import { useMetricsStore } from '../store/metricsStore';

interface MetricCardProps {
  title: string;
  value: number;
  target: number;
  unit: string;
  benchmark: string;
}

const MetricCard: React.FC<MetricCardProps> = ({ title, value, target, unit, benchmark }) => {
  const percentage = (value / target) * 100;
  const status = percentage >= 100 ? 'excellent' : percentage >= 80 ? 'good' : 'needs-improvement';

  return (
    <div className={`metric-card metric-${status}`}>
      <h3>{title}</h3>
      <div className="metric-value">
        <span className="value">{value.toFixed(1)}</span>
        <span className="unit">{unit}</span>
      </div>
      <div className="metric-target">Target: {target} {unit}</div>
      <div className="metric-benchmark">{benchmark}</div>
      <div className="metric-progress">
        <div className="progress-bar" style={{ width: `${Math.min(percentage, 100)}%` }} />
      </div>
    </div>
  );
};

export const KPIPanel: React.FC = () => {
  const metrics = useMetricsStore(state => state.metrics);

  const benchmarks = {
    picksPerHour: { target: 500, current: metrics.picks_per_hour },
    robotUtilization: { target: 80, current: metrics.robot_utilization_pct },
    pickAccuracy: { target: 99.8, current: metrics.pick_accuracy_pct },
    orderCycleTime: { target: 300, current: metrics.avg_order_cycle_time_sec }
  };

  return (
    <div className="kpi-dashboard">
      <h2>Performance Metrics</h2>

      <div className="metrics-grid">
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
      </div>

      <div className="fleet-status">
        <h3>Fleet Status</h3>
        <div className="status-bars">
          <div className="status-item">
            <span>Active: {metrics.active_robots}</span>
            <div className="bar active" />
          </div>
          <div className="status-item">
            <span>Idle: {metrics.idle_robots}</span>
            <div className="bar idle" />
          </div>
          <div className="status-item">
            <span>Charging: {metrics.charging_robots}</span>
            <div className="bar charging" />
          </div>
        </div>
      </div>

      <div className="queue-status">
        <h3>Queue Status</h3>
        <p>Pending Tasks: {metrics.pending_tasks}</p>
        <p>Pending Orders: {metrics.pending_orders}</p>
        <p>Avg Wait Time: {metrics.avg_task_wait_time_sec.toFixed(1)}s</p>
      </div>
    </div>
  );
};
```

**File:** `C:\Users\costa\src\warehouser\web_frontend\src\store\metricsStore.ts` (NEW)

```typescript
import { create } from 'zustand';

interface PerformanceMetrics {
  picks_per_hour: number;
  orders_per_hour: number;
  avg_order_cycle_time_sec: number;
  robot_utilization_pct: number;
  travel_distance_ratio: number;
  task_completion_rate: number;
  pick_accuracy_pct: number;
  on_time_delivery_pct: number;
  active_robots: number;
  idle_robots: number;
  charging_robots: number;
  failed_robots: number;
  pending_tasks: number;
  pending_orders: number;
  avg_task_wait_time_sec: number;
}

interface MetricsStore {
  metrics: PerformanceMetrics;
  updateMetrics: (newMetrics: Partial<PerformanceMetrics>) => void;
}

export const useMetricsStore = create<MetricsStore>((set) => ({
  metrics: {
    picks_per_hour: 0,
    orders_per_hour: 0,
    avg_order_cycle_time_sec: 0,
    robot_utilization_pct: 0,
    travel_distance_ratio: 0,
    task_completion_rate: 0,
    pick_accuracy_pct: 0,
    on_time_delivery_pct: 0,
    active_robots: 0,
    idle_robots: 0,
    charging_robots: 0,
    failed_robots: 0,
    pending_tasks: 0,
    pending_orders: 0,
    avg_task_wait_time_sec: 0,
  },
  updateMetrics: (newMetrics) =>
    set((state) => ({
      metrics: { ...state.metrics, ...newMetrics },
    })),
}));
```

### Phase 6: RL Training Integration

**Priority:** MEDIUM | **Effort:** MEDIUM | **Duration:** 1-2 weeks

#### 6.1 Update Reward Strategy

**File:** `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_strategy.hpp`

Add order-based reward strategy:
```cpp
class OrderCompletionRewardStrategy : public RewardStrategy {
public:
    OrderCompletionRewardStrategy(
        float order_completion_bonus = 100.0f,
        float cycle_time_weight = 10.0f,
        float throughput_weight = 1.0f
    ) : order_completion_bonus_(order_completion_bonus),
        cycle_time_weight_(cycle_time_weight),
        throughput_weight_(throughput_weight) {}

    float compute_reward(const RewardContext& context) override {
        float reward = 0.0f;

        // Large bonus for completing entire order
        if (context.order_completed) {
            reward += order_completion_bonus_;

            // Bonus inversely proportional to cycle time (faster = better)
            float cycle_time_minutes = context.order_cycle_time_sec / 60.0f;
            reward += cycle_time_weight_ / cycle_time_minutes;
        }

        // Continuous reward for throughput (picks per minute)
        reward += throughput_weight_ * context.picks_last_minute;

        return reward;
    }

private:
    float order_completion_bonus_;
    float cycle_time_weight_;
    float throughput_weight_;
};
```

#### 6.2 Multi-Robot Environment

**File:** `C:\Users\costa\src\warehouser\training\training\envs\multi_robot_env.py`

Update PettingZoo environment:
```python
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

        # Observation space: local 3x3 grid + order context
        grid_size = sensor_range * 2 + 1
        self.observation_spaces = {
            agent: spaces.Dict({
                'local_grid': spaces.Box(
                    low=0, high=255,
                    shape=(grid_size, grid_size, 4),  # robots, shelves, goals, walls
                    dtype=np.uint8
                ),
                'order_context': spaces.Box(
                    low=0, high=100,
                    shape=(8,),  # [current_item_idx, items_remaining, next_item_bearing, ...]
                    dtype=np.float32
                )
            })
            for agent in self.possible_agents
        }

    def _compute_rewards(self, observations, actions, next_observations):
        """
        Reward structure:
        - Individual: +1.0 for successful pick
        - Shared: +10.0 for completing entire order (all agents benefit)
        - Penalty: -0.5 for collision with other robot
        """
        rewards = {agent: 0.0 for agent in self.agents}

        # Individual pick rewards
        for agent in self.agents:
            if self._did_pick_success(agent):
                rewards[agent] += 1.0

        # Shared order completion reward
        if self._is_order_complete():
            order_reward = 10.0
            for agent in self.agents:
                rewards[agent] += order_reward

        # Collision penalties
        for agent in self.agents:
            if self._did_collide_with_robot(agent):
                rewards[agent] -= 0.5

        return rewards
```

## Interface Definitions

### ROS 2 Messages

```
warehouser_msgs/msg/Order.msg
warehouser_msgs/msg/OrderItem.msg
warehouser_msgs/msg/StorageLocation.msg
warehouser_msgs/msg/Task.msg
warehouser_msgs/msg/TaskPayload.msg
warehouser_msgs/msg/PerformanceMetrics.msg
warehouser_msgs/msg/BidNotice.msg
warehouser_msgs/msg/BidProposal.msg
warehouser_msgs/msg/DispatchRequest.msg
```

### ROS 2 Services

```
warehouser_msgs/srv/CreateOrder.srv
warehouser_msgs/srv/AssignTask.srv
warehouser_msgs/srv/QueryInventory.srv
warehouser_msgs/srv/GetMetrics.srv
```

### REST API Endpoints

```
GET    /api/orders                 # List all orders
POST   /api/orders                 # Create new order
GET    /api/orders/{id}            # Get order details
PATCH  /api/orders/{id}            # Update order status

GET    /api/inventory              # Query inventory
GET    /api/inventory/{sku}        # Get SKU details
POST   /api/inventory/adjust       # Adjust inventory (putaway)

GET    /api/robots                 # List all robots
GET    /api/robots/{id}            # Get robot status
POST   /api/robots/{id}/assign     # Manually assign task

GET    /api/tasks                  # List all tasks
GET    /api/tasks/{id}             # Get task details
POST   /api/tasks/{id}/cancel      # Cancel task

GET    /api/metrics                # Get current metrics
GET    /api/metrics/history        # Historical metrics
```

## New Packages to Create

| Package | Purpose | Key Interfaces |
|---------|---------|----------------|
| `warehouser_wms` | Warehouse Management System node | Order generation, inventory tracking, REST API server |
| `warehouser_scheduler` | Task assignment & coordination | Auction-based bidding, fleet coordination |
| `warehouser_analytics` | Performance metrics tracking | KPI calculation, metrics publishing |

## Files to Create

| File | Purpose |
|------|---------|
| `warehouser_msgs/msg/Order.msg` | Multi-item order definition |
| `warehouser_msgs/msg/OrderItem.msg` | Individual item in order |
| `warehouser_msgs/msg/StorageLocation.msg` | Warehouse location addressing |
| `warehouser_msgs/msg/Task.msg` | Enhanced task with order linkage |
| `warehouser_msgs/msg/TaskPayload.msg` | Task execution details |
| `warehouser_msgs/msg/PerformanceMetrics.msg` | Industry KPIs |
| `warehouser_msgs/msg/BidNotice.msg` | Auction bid request |
| `warehouser_msgs/msg/BidProposal.msg` | Robot bid response |
| `warehouser_msgs/srv/CreateOrder.srv` | Order creation service |
| `warehouser_simulation/include/warehouser_simulation/warehouse_layout.hpp` | Grid layout generator |
| `warehouser_simulation/src/warehouse_layout.cpp` | Layout implementation |
| `warehouser_wms/warehouser_wms/wms_node.py` | WMS node (order gen) |
| `warehouser_wms/warehouser_wms/inventory_manager.py` | Inventory tracking |
| `warehouser_wms/warehouser_wms/api_server.py` | REST API server |
| `warehouser_scheduler/warehouser_scheduler/fleet_coordinator.py` | Multi-robot coordinator |
| `warehouser_scheduler/include/warehouser_scheduler/task_auction.hpp` | Auction logic |
| `warehouser_analytics/warehouser_analytics/analytics_node.py` | Metrics calculation |
| `web_frontend/src/components/KPIPanel.tsx` | Performance dashboard |
| `web_frontend/src/components/WarehouseGrid.tsx` | 3D warehouse visualization |
| `web_frontend/src/store/metricsStore.ts` | Metrics state management |
| `web_frontend/src/hooks/useMetrics.ts` | ROS metrics subscription |

## Files to Modify

| File | Change |
|------|--------|
| `warehouser_msgs/msg/Entity.msg` | Add TYPE_SHELF=4, TYPE_STATION=5, shelf properties |
| `warehouser_task/include/warehouser_task/task_state_machine.hpp` | Extend Task struct with order_id, priority, timestamps |
| `warehouser_task/include/warehouser_task/task_manager_node.hpp` | Add order queue, order decomposition logic |
| `warehouser_rl_bridge/include/warehouser_rl_bridge/reward_strategy.hpp` | Add OrderCompletionRewardStrategy |
| `warehouser_bringup/config/world.yaml` | Replace simple world with structured warehouse layout |
| `training/training/envs/multi_robot_env.py` | Update with order-based rewards, shared completion bonus |
| `web_frontend/src/App.tsx` | Add KPIPanel component |

## Architecture Notes

### Modularity Principles

1. **Separation of Concerns:**
   - WMS layer handles business logic (orders, inventory)
   - Scheduler layer handles resource allocation (task assignment)
   - Simulation layer handles physics and entity management
   - Analytics layer handles metrics and reporting

2. **Message-Driven Architecture:**
   - All components communicate via ROS 2 topics/services
   - Enables distributed testing (run WMS on different machine)
   - Easy to swap implementations (mock WMS for testing)

3. **Industry Alignment:**
   - REST API follows OpenBoxes/Odoo patterns
   - Task bidding follows RMF dispatcher pattern
   - Layout generation follows RWARE/OR-Gym patterns
   - KPIs match 2025 industry benchmarks

4. **Extensibility:**
   - Strategy Pattern for rewards (easy to add new strategies)
   - Plugin architecture for picking strategies (batch/wave/zone)
   - Configurable layout generator (warehouse sizes, aisle patterns)

### Integration Points

```
┌─────────────────────────────────────────────────────────────┐
│  External WMS (via REST API)                                │
└─────────────────┬───────────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────────┐
│  warehouser_wms (Order Management, Inventory)               │
│  - Order generation/ingestion                               │
│  - Inventory tracking                                       │
│  - REST API server                                          │
└─────────────────┬───────────────────────────────────────────┘
                  │ /wms/orders, /wms/tasks
┌─────────────────▼───────────────────────────────────────────┐
│  warehouser_scheduler (Task Assignment)                     │
│  - Auction-based bidding                                    │
│  - Fleet coordination                                       │
│  - Deadlock prevention                                      │
└─────────────────┬───────────────────────────────────────────┘
                  │ /fleet/dispatch
┌─────────────────▼───────────────────────────────────────────┐
│  warehouser_task (Task Execution)                           │
│  - Task state machine                                       │
│  - Robot bidding logic                                      │
│  - Task queue management                                    │
└─────────────────┬───────────────────────────────────────────┘
                  │ /robots/goals
┌─────────────────▼───────────────────────────────────────────┐
│  warehouser_rl_bridge (RL Environment)                      │
│  - Step/reset services                                      │
│  - Reward computation                                       │
│  - Observation building                                     │
└─────────────────┬───────────────────────────────────────────┘
                  │ /world/state
┌─────────────────▼───────────────────────────────────────────┐
│  warehouser_simulation (Physics, Entities)                  │
│  - Robot kinematics                                         │
│  - Collision detection                                      │
│  - Warehouse layout                                         │
└─────────────────────────────────────────────────────────────┘

                  Metrics Flow
┌─────────────────────────────────────────────────────────────┐
│  warehouser_analytics                                       │
│  - Subscribe to task completions, robot status              │
│  - Calculate KPIs                                           │
│  - Publish /analytics/metrics                               │
└─────────────────┬───────────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────────┐
│  web_frontend (Visualization)                               │
│  - KPI dashboard                                            │
│  - 3D warehouse view                                        │
│  - Order management UI                                      │
└─────────────────────────────────────────────────────────────┘
```

## Verification

### Phase 1 Verification (Storage Infrastructure)
- [ ] Generate warehouse layout with 100+ storage locations
- [ ] Visualize layout in RViz2 (occupancy grid)
- [ ] Verify location addressing (A-03-02-05 maps to correct x,y)
- [ ] Confirm objects can be placed in specific bins
- [ ] Test access mapping (each storage location has aisle access point)

### Phase 2 Verification (Multi-Item Orders)
- [ ] Create order with 5 items in different locations
- [ ] Verify task decomposition (1 order → 5 pick tasks)
- [ ] Confirm task sequencing (pick all items before delivery)
- [ ] Test order status tracking (PENDING → PICKING → COMPLETED)
- [ ] Measure order cycle time (should be <5 minutes for 5 items)

### Phase 3 Verification (Multi-Robot Coordination)
- [ ] Spawn 4 robots, create 10 simultaneous orders
- [ ] Verify auction system assigns tasks optimally (lowest cost wins)
- [ ] Confirm no resource conflicts (two robots don't pick same item)
- [ ] Test deadlock prevention (robots don't block each other)
- [ ] Measure throughput (picks/hour with 4 robots > 1 robot)

### Phase 4 Verification (WMS Integration)
- [ ] Create order via REST API (POST /api/orders)
- [ ] Query inventory via API (GET /api/inventory)
- [ ] Verify order status updates via API (GET /api/orders/{id})
- [ ] Test robot status queries (GET /api/robots)
- [ ] Confirm metrics endpoint works (GET /api/metrics)

### Phase 5 Verification (Performance Metrics)
- [ ] Run simulation for 10 minutes
- [ ] Verify picks/hour metric (target: 400-600 with 4 robots)
- [ ] Confirm robot utilization >80% during peak load
- [ ] Measure pick accuracy (should be 100% in simulation)
- [ ] Visualize KPI dashboard in web frontend

### Phase 6 Verification (RL Training)
- [ ] Train multi-robot policy for 1M steps
- [ ] Verify order completion reward drives behavior
- [ ] Confirm shared rewards improve coordination
- [ ] Test policy against wave picking scenario (20 orders released at once)
- [ ] Benchmark against industry KPIs (compare to 400-600 picks/hour)

### Integration Tests
- [ ] End-to-end test: Create order via API → assign via auction → execute with RL → complete → update metrics
- [ ] Stress test: 100 orders/minute, 10 robots
- [ ] Failure recovery: Robot fails mid-task, task reassigned
- [ ] Priority handling: Urgent order preempts normal orders
- [ ] Multi-warehouse: Test with multiple warehouse zones

### Industry Benchmark Comparison
- [ ] Compare Warehouser throughput to Amazon Robotics (300-400 items/hour)
- [ ] Measure against RWARE baseline (shelf delivery time)
- [ ] Validate against Ocado performance (50-item order in 5 minutes)
- [ ] Document gaps between simulation and real systems

## Success Criteria

1. **Functional:**
   - Generate structured warehouse layout (aisles, shelves, bins)
   - Create and execute multi-item orders
   - Coordinate 4+ robots without conflicts
   - Track industry-standard KPIs (throughput, accuracy, cycle time)

2. **Performance:**
   - Achieve >400 picks/hour with 4 robots
   - Maintain >80% robot utilization
   - Complete 5-item orders in <5 minutes
   - Zero pick errors (100% accuracy in simulation)

3. **Integration:**
   - REST API works with external clients (Postman, web frontend)
   - ROS 2 components communicate correctly
   - RL training improves KPIs over time
   - Metrics dashboard displays real-time data

4. **Code Quality:**
   - All new code has unit tests (>80% coverage)
   - C++ follows style guide (C++23, std::expected, PascalCase/camelCase)
   - Python fully typed (mypy --strict passes)
   - TypeScript strict mode (no `any` types)

5. **Documentation:**
   - README explains industry context (comparison to Amazon, Ocado)
   - API documentation (OpenAPI/Swagger spec)
   - Architecture diagram (component relationships)
   - Glossary of warehouse terms (SKU, WMS, GTP, etc.)

## References

### Industry Research (from S.md)
- Amazon Robotics Kiva system: Goods-to-person, 300-400 items/hour
- Ocado Technology: 3D grid, 50-item order in 5 minutes
- WMS/WES/WCS integration architecture
- Picking strategies: Batch, Wave, Zone, Hybrid
- KPI benchmarks: 400-600 picks/hour, 99.8% accuracy, <5min cycle time

### Template Implementations (from T.md)
- OpenBoxes/Odoo: WMS REST API patterns, location hierarchies
- RWARE: Multi-robot warehouse environment, action/observation spaces
- RMF Tasks: Bidding system, task types (PICK, TRANSPORT, DELIVERY)
- Warehouse Layout Generator: Grid-based layout, aisle mapping
- OR-Gym: Inventory management, performance metrics

### Current Implementation (from I.md)
- Entity model: Robot, Object, Wall, Zone
- Task state machine: 8 states for pick-and-place
- Reward strategies: Navigation, Collision, Time, PickPlace, Exploration
- Observation versions: V1_Position, V2_Lidar, V3_MultiRobot
- Frontend: StatusPanel, ObjectivePanel (basic)

## Notes

- **Incremental Development:** Implement phases sequentially, test each before moving to next
- **Backwards Compatibility:** Keep existing simple pick-and-place working during development
- **Testing Strategy:** Unit tests for individual components, integration tests for workflows
- **Performance Profiling:** Use ROS 2 tools (ros2 topic hz, ros2 node list) to verify message rates
- **Future Extensions:** Digital twin (record/replay), battery management, vertical storage, seasonal adaptation
