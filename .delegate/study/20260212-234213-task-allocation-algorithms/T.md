# Template: Task Allocation Algorithms

Created: 2026-02-12T23:47:00Z

## Overview

This document provides copy-paste-ready templates and reference implementations for multi-robot task allocation algorithms applicable to the Warehouser project. Focus areas include Hungarian algorithm (optimal centralized), auction-based allocation (distributed), task queue patterns, and RL-based allocation.

---

## 1. Hungarian Algorithm (Optimal Centralized Allocation)

### Source

- `scipy.optimize.linear_sum_assignment` (Python standard library for Hungarian algorithm)
- Based on Jonker-Volgenant algorithm implementation
- Documentation: https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.linear_sum_assignment.html

### Pattern: Cost-Optimal Assignment

The Hungarian algorithm solves the linear sum assignment problem optimally in O(n³) time. It assigns n robots to n tasks such that the total cost is minimized.

**Key Concepts:**
- **Cost Matrix C[i,j]**: Cost of assigning robot i to task j
- **Result**: Optimal assignment minimizing total cost
- **Constraints**: Handles rectangular matrices (unequal robots/tasks)

### Application to Warehouser

#### Basic Hungarian Implementation

```python
"""Task allocator using Hungarian algorithm for optimal assignment."""

import numpy as np
from scipy.optimize import linear_sum_assignment
from typing import List, Tuple
from dataclasses import dataclass

@dataclass
class Robot:
    """Robot state for allocation."""
    id: int
    x: float
    y: float
    battery: float  # 0.0 to 1.0
    is_carrying: bool
    current_load: int  # Number of tasks assigned

@dataclass
class Task:
    """Task to be allocated."""
    id: int
    pickup_x: float
    pickup_y: float
    dropoff_x: float
    dropoff_y: float
    priority: float  # Higher = more urgent (0.0 to 1.0)


class HungarianTaskAllocator:
    """Optimal task allocation using Hungarian algorithm."""

    def __init__(
        self,
        distance_weight: float = 1.0,
        battery_weight: float = 0.5,
        load_weight: float = 0.3,
        priority_weight: float = 2.0,
    ):
        """Initialize allocator with cost function weights.

        Args:
            distance_weight: Weight for distance cost
            battery_weight: Weight for low battery penalty
            load_weight: Weight for load imbalance penalty
            priority_weight: Weight for task priority urgency
        """
        self.w_distance = distance_weight
        self.w_battery = battery_weight
        self.w_load = load_weight
        self.w_priority = priority_weight

    def allocate(
        self, robots: List[Robot], tasks: List[Task]
    ) -> List[Tuple[int, int]]:
        """Allocate tasks to robots optimally.

        Args:
            robots: List of available robots
            tasks: List of tasks to allocate

        Returns:
            List of (robot_id, task_id) assignments
        """
        if not robots or not tasks:
            return []

        # Build cost matrix
        cost_matrix = self._build_cost_matrix(robots, tasks)

        # Solve assignment problem
        robot_indices, task_indices = linear_sum_assignment(cost_matrix)

        # Convert indices to IDs
        assignments = []
        for r_idx, t_idx in zip(robot_indices, task_indices):
            # Only include valid assignments (skip dummy tasks/robots)
            if r_idx < len(robots) and t_idx < len(tasks):
                assignments.append((robots[r_idx].id, tasks[t_idx].id))

        return assignments

    def _build_cost_matrix(
        self, robots: List[Robot], tasks: List[Task]
    ) -> np.ndarray:
        """Build cost matrix for assignment problem.

        Args:
            robots: List of robots
            tasks: List of tasks

        Returns:
            Cost matrix C[i,j] = cost of assigning robot i to task j
        """
        n_robots = len(robots)
        n_tasks = len(tasks)

        # Handle rectangular matrix by padding with dummy rows/columns
        size = max(n_robots, n_tasks)
        cost_matrix = np.full((size, size), 1e6, dtype=np.float32)

        # Fill actual costs
        for i, robot in enumerate(robots):
            for j, task in enumerate(tasks):
                cost_matrix[i, j] = self._compute_cost(robot, task)

        return cost_matrix

    def _compute_cost(self, robot: Robot, task: Task) -> float:
        """Compute cost of assigning a specific robot to a specific task.

        Args:
            robot: Robot to assign
            task: Task to assign

        Returns:
            Cost scalar (lower is better)
        """
        # Distance cost (Manhattan for warehouse grid)
        distance = abs(robot.x - task.pickup_x) + abs(robot.y - task.pickup_y)
        distance_cost = self.w_distance * distance

        # Battery penalty (exponential for low battery)
        battery_penalty = self.w_battery * np.exp(-5.0 * robot.battery)

        # Load imbalance penalty (prefer distributing work evenly)
        load_penalty = self.w_load * robot.current_load

        # Priority urgency (high priority tasks should be assigned)
        # Note: Subtract from cost to prefer high-priority assignments
        priority_bonus = -self.w_priority * task.priority

        # Carrying penalty (prefer unloaded robots for pickup tasks)
        carrying_penalty = 10.0 if robot.is_carrying else 0.0

        total_cost = (
            distance_cost + battery_penalty + load_penalty
            + priority_bonus + carrying_penalty
        )

        return total_cost


# Example Usage
def example_allocation():
    """Example of Hungarian allocation."""
    # Create robots
    robots = [
        Robot(id=0, x=0.0, y=0.0, battery=0.9, is_carrying=False, current_load=0),
        Robot(id=1, x=5.0, y=5.0, battery=0.6, is_carrying=False, current_load=1),
        Robot(id=2, x=10.0, y=2.0, battery=0.3, is_carrying=True, current_load=2),
    ]

    # Create tasks
    tasks = [
        Task(id=0, pickup_x=2.0, pickup_y=1.0, dropoff_x=8.0, dropoff_y=8.0, priority=0.9),
        Task(id=1, pickup_x=6.0, pickup_y=6.0, dropoff_x=3.0, dropoff_y=3.0, priority=0.5),
        Task(id=2, pickup_x=9.0, pickup_y=3.0, dropoff_x=1.0, dropoff_y=7.0, priority=0.7),
    ]

    # Allocate
    allocator = HungarianTaskAllocator()
    assignments = allocator.allocate(robots, tasks)

    print("Optimal Assignments:")
    for robot_id, task_id in assignments:
        print(f"  Robot {robot_id} -> Task {task_id}")

```

#### Integration with ROS2 Task Manager

```cpp
// warehouser_task/include/warehouser_task/hungarian_allocator.hpp
#pragma once

#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>

namespace warehouser_task {

struct RobotState {
    int id;
    float x;
    float y;
    float battery;
    bool is_carrying;
    int current_load;
};

struct TaskRequest {
    int id;
    float pickup_x;
    float pickup_y;
    float dropoff_x;
    float dropoff_y;
    float priority;
};

class HungarianAllocator {
public:
    HungarianAllocator(
        float distance_weight = 1.0f,
        float battery_weight = 0.5f,
        float load_weight = 0.3f,
        float priority_weight = 2.0f
    ) : w_distance_(distance_weight),
        w_battery_(battery_weight),
        w_load_(load_weight),
        w_priority_(priority_weight) {}

    // Allocate tasks to robots
    std::vector<std::pair<int, int>> allocate(
        const std::vector<RobotState>& robots,
        const std::vector<TaskRequest>& tasks
    );

private:
    float computeCost(const RobotState& robot, const TaskRequest& task) const;
    std::vector<std::vector<float>> buildCostMatrix(
        const std::vector<RobotState>& robots,
        const std::vector<TaskRequest>& tasks
    ) const;

    // Hungarian algorithm implementation (or call external library)
    std::vector<std::pair<int, int>> hungarianSolve(
        const std::vector<std::vector<float>>& cost_matrix
    );

    float w_distance_;
    float w_battery_;
    float w_load_;
    float w_priority_;
};

} // namespace warehouser_task
```

**Note:** For C++, consider using existing libraries:
- `libhungarian-cpp` (header-only)
- `munkres-cpp` (Boost-style implementation)
- Or call Python via subprocess/IPC for `scipy.optimize.linear_sum_assignment`

---

## 2. Auction-Based Allocation (Distributed)

### Source

- ROS2-based Distributed Task Allocation Framework (2025)
- Contract Net Protocol (FIPA standard)
- Reference: https://link.springer.com/article/10.1007/s12555-025-0530-7

### Pattern: Bidding and Auctioning

Auction-based methods allow robots to bid on tasks based on local cost estimates. An auctioneer (centralized or rotating) awards tasks to the lowest bidders.

**Key Concepts:**
- **Bidding:** Robots compute bids (cost estimates) for available tasks
- **Auction:** Auctioneer collects bids and awards task to best bidder
- **Re-auction:** Failed tasks are re-auctioned to other robots
- **Distributed:** No global coordinator required (peer-to-peer)

### Application to Warehouser

#### ROS2 Auction Service Pattern

```python
"""Auction-based task allocation using ROS2 services."""

from typing import Dict, List, Optional
import rclpy
from rclpy.node import Node
from dataclasses import dataclass
import numpy as np

# Assuming custom message types
# from warehouser_msgs.srv import TaskBid, TaskAward
# from warehouser_msgs.msg import TaskDescription, BidResponse


@dataclass
class Bid:
    """A robot's bid on a task."""
    robot_id: int
    task_id: int
    cost: float
    timestamp: float


class AuctionTaskAllocator(Node):
    """Centralized auctioneer node for task allocation."""

    def __init__(self):
        super().__init__('auction_allocator')

        # Auction parameters
        self.auction_timeout = 2.0  # seconds
        self.current_auctions: Dict[int, List[Bid]] = {}

        # ROS2 interfaces
        self.task_publisher = self.create_publisher(
            # TaskDescription,
            'auction/new_task',
            10
        )
        self.bid_service = self.create_service(
            # TaskBid,
            'auction/submit_bid',
            self.handle_bid
        )
        self.award_publisher = self.create_publisher(
            # TaskAward,
            'auction/task_awarded',
            10
        )

        # Timer for auction resolution
        self.auction_timer = self.create_timer(0.5, self.resolve_auctions)

        self.pending_tasks: List[int] = []
        self.task_start_times: Dict[int, float] = {}

    def announce_task(self, task_id: int, task_description: dict):
        """Announce new task for bidding.

        Args:
            task_id: Unique task identifier
            task_description: Task parameters (pickup, dropoff, priority)
        """
        # Publish task announcement
        # msg = TaskDescription()
        # msg.task_id = task_id
        # msg.pickup_x = task_description['pickup_x']
        # ... (fill message)
        # self.task_publisher.publish(msg)

        # Initialize auction
        self.current_auctions[task_id] = []
        self.task_start_times[task_id] = self.get_clock().now().nanoseconds / 1e9
        self.get_logger().info(f"Announced task {task_id} for auction")

    def handle_bid(self, request, response):
        """Handle bid submission from a robot.

        Args:
            request: TaskBid.Request with robot_id, task_id, bid_cost
            response: TaskBid.Response with acceptance status
        """
        # Extract bid info
        robot_id = request.robot_id
        task_id = request.task_id
        cost = request.bid_cost

        if task_id not in self.current_auctions:
            response.accepted = False
            response.message = "Task auction closed or invalid"
            return response

        # Record bid
        bid = Bid(
            robot_id=robot_id,
            task_id=task_id,
            cost=cost,
            timestamp=self.get_clock().now().nanoseconds / 1e9
        )
        self.current_auctions[task_id].append(bid)

        self.get_logger().info(f"Received bid from Robot {robot_id}: {cost:.2f}")

        response.accepted = True
        response.message = "Bid accepted"
        return response

    def resolve_auctions(self):
        """Resolve auctions that have timed out."""
        current_time = self.get_clock().now().nanoseconds / 1e9

        to_resolve = []
        for task_id, start_time in self.task_start_times.items():
            if current_time - start_time >= self.auction_timeout:
                to_resolve.append(task_id)

        for task_id in to_resolve:
            self.award_task(task_id)

    def award_task(self, task_id: int):
        """Award task to lowest bidder.

        Args:
            task_id: Task to award
        """
        if task_id not in self.current_auctions:
            return

        bids = self.current_auctions[task_id]

        if not bids:
            self.get_logger().warn(f"No bids received for task {task_id}")
            # Re-auction or assign to fallback
            return

        # Find lowest bid
        winning_bid = min(bids, key=lambda b: b.cost)

        # Publish award
        # award_msg = TaskAward()
        # award_msg.task_id = task_id
        # award_msg.winner_robot_id = winning_bid.robot_id
        # award_msg.winning_cost = winning_bid.cost
        # self.award_publisher.publish(award_msg)

        self.get_logger().info(
            f"Awarded task {task_id} to Robot {winning_bid.robot_id} "
            f"(cost: {winning_bid.cost:.2f})"
        )

        # Clean up auction
        del self.current_auctions[task_id]
        del self.task_start_times[task_id]


class BiddingRobotNode(Node):
    """Robot node that bids on tasks."""

    def __init__(self, robot_id: int):
        super().__init__(f'robot_{robot_id}')
        self.robot_id = robot_id

        # Robot state
        self.x = 0.0
        self.y = 0.0
        self.battery = 1.0
        self.current_load = 0

        # ROS2 interfaces
        self.task_subscriber = self.create_subscription(
            # TaskDescription,
            'auction/new_task',
            self.handle_task_announcement,
            10
        )
        self.bid_client = self.create_client(
            # TaskBid,
            'auction/submit_bid'
        )
        self.award_subscriber = self.create_subscription(
            # TaskAward,
            'auction/task_awarded',
            self.handle_task_award,
            10
        )

    def handle_task_announcement(self, msg):
        """Compute bid and submit to auctioneer.

        Args:
            msg: TaskDescription message
        """
        task_id = msg.task_id
        pickup_x = msg.pickup_x
        pickup_y = msg.pickup_y

        # Compute bid (local cost estimate)
        distance = np.sqrt((self.x - pickup_x)**2 + (self.y - pickup_y)**2)
        battery_penalty = np.exp(-5.0 * self.battery)
        load_penalty = 0.3 * self.current_load

        bid_cost = distance + battery_penalty + load_penalty

        # Submit bid
        request = TaskBid.Request()
        request.robot_id = self.robot_id
        request.task_id = task_id
        request.bid_cost = bid_cost

        future = self.bid_client.call_async(request)
        future.add_done_callback(
            lambda f: self.get_logger().info(f"Bid submitted for task {task_id}")
        )

    def handle_task_award(self, msg):
        """Handle task award notification.

        Args:
            msg: TaskAward message
        """
        if msg.winner_robot_id == self.robot_id:
            self.get_logger().info(f"Won task {msg.task_id}!")
            # Start executing task
            self.current_load += 1
        else:
            self.get_logger().debug(f"Task {msg.task_id} awarded to another robot")
```

#### Distributed Peer-to-Peer Auction (No Central Auctioneer)

```python
"""Distributed auction without central coordinator."""

class DistributedAuctionNode(Node):
    """Robot node with distributed auction capability."""

    def __init__(self, robot_id: int, all_robot_ids: List[int]):
        super().__init__(f'robot_{robot_id}')
        self.robot_id = robot_id
        self.all_robot_ids = all_robot_ids

        # Auction state
        self.local_bids: Dict[int, float] = {}  # task_id -> my bid
        self.peer_bids: Dict[int, Dict[int, float]] = {}  # task_id -> {robot_id -> bid}

        # Publishers/subscribers for peer communication
        self.bid_publisher = self.create_publisher(
            # PeerBid,
            'distributed/bids',
            10
        )
        self.bid_subscriber = self.create_subscription(
            # PeerBid,
            'distributed/bids',
            self.handle_peer_bid,
            10
        )

        # Timer for consensus resolution
        self.consensus_timer = self.create_timer(1.0, self.resolve_consensus)

    def bid_on_task(self, task_id: int, cost: float):
        """Broadcast bid to all peers.

        Args:
            task_id: Task identifier
            cost: Computed bid cost
        """
        self.local_bids[task_id] = cost

        # Broadcast to all peers
        # msg = PeerBid()
        # msg.robot_id = self.robot_id
        # msg.task_id = task_id
        # msg.bid_cost = cost
        # self.bid_publisher.publish(msg)

        # Initialize peer tracking
        if task_id not in self.peer_bids:
            self.peer_bids[task_id] = {}
        self.peer_bids[task_id][self.robot_id] = cost

    def handle_peer_bid(self, msg):
        """Record peer bids.

        Args:
            msg: PeerBid message from another robot
        """
        task_id = msg.task_id
        peer_id = msg.robot_id
        peer_cost = msg.bid_cost

        if task_id not in self.peer_bids:
            self.peer_bids[task_id] = {}
        self.peer_bids[task_id][peer_id] = peer_cost

    def resolve_consensus(self):
        """Resolve auction via distributed consensus.

        Each robot independently determines winner based on collected bids.
        """
        for task_id, bids in self.peer_bids.items():
            # Check if all robots have bid
            if len(bids) < len(self.all_robot_ids):
                continue  # Wait for more bids

            # Find minimum bid
            winner_id = min(bids.items(), key=lambda x: x[1])[0]

            if winner_id == self.robot_id:
                self.get_logger().info(f"Consensus: I won task {task_id}")
                # Start executing task
            else:
                self.get_logger().debug(f"Consensus: Robot {winner_id} won task {task_id}")

            # Clean up resolved auction
            del self.peer_bids[task_id]
            if task_id in self.local_bids:
                del self.local_bids[task_id]
```

---

## 3. Task Queue with Priority Scheduling

### Source

- Python `queue.PriorityQueue` (standard library)
- Multi-level feedback queue pattern
- Documentation: https://docs.python.org/3/library/queue.html

### Pattern: Priority-Based Task Scheduling

Use priority queues to manage task assignment based on urgency, robot availability, and system load.

**Key Concepts:**
- **Priority Queue:** Tasks ordered by priority (urgent tasks first)
- **FIFO within priority:** Tasks with same priority use FIFO order
- **Dynamic re-prioritization:** Update priorities based on waiting time
- **Multi-level queues:** Separate queues for different task types

### Application to Warehouser

#### Priority Queue Task Manager

```python
"""Priority queue task manager for warehouse operations."""

import heapq
from dataclasses import dataclass, field
from typing import List, Optional
import time
from enum import IntEnum


class TaskPriority(IntEnum):
    """Task priority levels (lower number = higher priority)."""
    CRITICAL = 0    # Emergency/failure recovery
    HIGH = 1        # Express orders
    NORMAL = 2      # Standard orders
    LOW = 3         # Background/optimization tasks


@dataclass(order=True)
class PrioritizedTask:
    """Task with priority for queue insertion."""
    priority: int
    timestamp: float = field(compare=True)  # For FIFO within same priority
    task_id: int = field(compare=False)
    pickup_x: float = field(compare=False)
    pickup_y: float = field(compare=False)
    dropoff_x: float = field(compare=False)
    dropoff_y: float = field(compare=False)
    retries: int = field(default=0, compare=False)


class TaskQueue:
    """Thread-safe priority queue for task allocation."""

    def __init__(self, aging_factor: float = 0.1):
        """Initialize task queue.

        Args:
            aging_factor: Factor for priority aging (prevents starvation)
        """
        self.queue: List[PrioritizedTask] = []
        self.aging_factor = aging_factor
        self.task_index = 0

    def add_task(
        self,
        task_id: int,
        pickup: tuple[float, float],
        dropoff: tuple[float, float],
        priority: TaskPriority = TaskPriority.NORMAL
    ):
        """Add task to queue.

        Args:
            task_id: Unique task identifier
            pickup: Pickup location (x, y)
            dropoff: Dropoff location (x, y)
            priority: Task priority level
        """
        task = PrioritizedTask(
            priority=priority,
            timestamp=time.time(),
            task_id=task_id,
            pickup_x=pickup[0],
            pickup_y=pickup[1],
            dropoff_x=dropoff[0],
            dropoff_y=dropoff[1]
        )
        heapq.heappush(self.queue, task)

    def get_next_task(self) -> Optional[PrioritizedTask]:
        """Get highest priority task from queue.

        Returns:
            Next task to execute, or None if queue empty
        """
        if not self.queue:
            return None
        return heapq.heappop(self.queue)

    def peek_next_task(self) -> Optional[PrioritizedTask]:
        """Peek at highest priority task without removing.

        Returns:
            Next task, or None if queue empty
        """
        if not self.queue:
            return None
        return self.queue[0]

    def apply_aging(self):
        """Apply priority aging to prevent starvation.

        Tasks waiting longer gradually increase in priority.
        """
        current_time = time.time()
        aged_queue = []

        for task in self.queue:
            wait_time = current_time - task.timestamp
            age_boost = int(wait_time * self.aging_factor)

            # Decrease priority value (increase urgency)
            new_priority = max(0, task.priority - age_boost)

            aged_task = PrioritizedTask(
                priority=new_priority,
                timestamp=task.timestamp,
                task_id=task.task_id,
                pickup_x=task.pickup_x,
                pickup_y=task.pickup_y,
                dropoff_x=task.dropoff_x,
                dropoff_y=task.dropoff_y,
                retries=task.retries
            )
            aged_queue.append(aged_task)

        heapq.heapify(aged_queue)
        self.queue = aged_queue

    def re_queue_failed_task(self, task: PrioritizedTask):
        """Re-queue a failed task with increased priority.

        Args:
            task: Failed task to re-queue
        """
        task.retries += 1
        task.priority = max(0, task.priority - 1)  # Increase priority
        task.timestamp = time.time()  # Update timestamp
        heapq.heappush(self.queue, task)

    def size(self) -> int:
        """Return number of tasks in queue."""
        return len(self.queue)

    def clear(self):
        """Clear all tasks from queue."""
        self.queue.clear()


class MultiQueueTaskManager:
    """Task manager with separate queues for task types."""

    def __init__(self):
        # Separate queues for different operations
        self.pickup_queue = TaskQueue()
        self.delivery_queue = TaskQueue()
        self.charging_queue = TaskQueue()
        self.maintenance_queue = TaskQueue()

    def add_pickup_task(self, task_id: int, location: tuple[float, float], priority: TaskPriority):
        """Add pickup task."""
        self.pickup_queue.add_task(
            task_id,
            pickup=location,
            dropoff=(0.0, 0.0),  # Placeholder
            priority=priority
        )

    def add_delivery_task(self, task_id: int, pickup: tuple[float, float], dropoff: tuple[float, float], priority: TaskPriority):
        """Add delivery task."""
        self.delivery_queue.add_task(task_id, pickup, dropoff, priority)

    def get_next_task_for_robot(self, robot_state: dict) -> Optional[PrioritizedTask]:
        """Get next task for robot based on state.

        Args:
            robot_state: Dict with robot state (battery, is_carrying, etc.)

        Returns:
            Next task, or None
        """
        battery = robot_state.get('battery', 1.0)
        is_carrying = robot_state.get('is_carrying', False)

        # Priority logic based on robot state
        if battery < 0.2:
            # Low battery: charging task
            return self.charging_queue.get_next_task()
        elif is_carrying:
            # Carrying item: delivery task
            return self.delivery_queue.get_next_task()
        else:
            # Available: pickup task
            return self.pickup_queue.get_next_task()
```

---

## 4. RL-Based Task Allocation

### Source

- PettingZoo ParallelEnv (existing Warehouser implementation)
- Centralized allocation policy with decentralized execution
- File: `C:\Users\costa\src\warehouser\training\training\envs\pettingzoo_env.py`

### Pattern: Learned Allocation Policy

Train a centralized allocation policy using multi-agent reinforcement learning. The policy learns to allocate tasks based on fleet state, task urgency, and historical performance.

**Key Concepts:**
- **Centralized Critic:** Global value function for coordination
- **Decentralized Actors:** Each robot executes independently
- **Allocation as Action:** Treat allocation decision as part of action space
- **Reward Shaping:** Fleet-level rewards for efficient allocation

### Application to Warehouser

#### Extending PettingZoo for Allocation

```python
"""Extended PettingZoo environment with task allocation."""

from typing import Any, Dict, List
import numpy as np
from gymnasium import spaces
from numpy.typing import NDArray
from pettingzoo import ParallelEnv

from training.models.config import MultiAgentConfig


class WarehouseAllocationEnv(ParallelEnv):
    """Multi-agent warehouse with centralized task allocation.

    Architecture:
    - Allocator agent: Assigns tasks to robots (centralized)
    - Robot agents: Execute allocated tasks (decentralized)
    """

    metadata = {"render_modes": ["human"], "name": "warehouse_allocation_v1"}

    def __init__(self, config: MultiAgentConfig | None = None):
        super().__init__()
        self.config = config or MultiAgentConfig()

        # Agents: allocator + robots
        self.possible_agents = ["allocator"] + [
            f"robot_{i}" for i in range(self.config.num_agents)
        ]
        self.agents = self.possible_agents.copy()

        # Allocator observation: global state
        # [num_robots, num_tasks, robot_states..., task_states...]
        allocator_obs_dim = (
            2 +  # num_robots, num_tasks
            self.config.num_agents * 5 +  # robot states (x, y, battery, load, carrying)
            10 * 4  # up to 10 tasks (pickup_x, pickup_y, dropoff_x, dropoff_y)
        )

        # Allocator action: assignment matrix
        # Action[i] = task_id assigned to robot_i (-1 = no assignment)
        allocator_action_dim = self.config.num_agents

        # Define spaces
        self._observation_spaces = {
            "allocator": spaces.Box(
                low=-np.inf, high=np.inf,
                shape=(allocator_obs_dim,), dtype=np.float32
            )
        }
        self._action_spaces = {
            "allocator": spaces.MultiDiscrete([11] * self.config.num_agents)  # 10 tasks + no-op
        }

        # Robot spaces (navigation actions)
        for robot_id in [f"robot_{i}" for i in range(self.config.num_agents)]:
            self._observation_spaces[robot_id] = spaces.Box(
                low=-np.inf, high=np.inf,
                shape=(self.config.obs_dim,), dtype=np.float32
            )
            self._action_spaces[robot_id] = spaces.Box(
                low=-1.0, high=1.0,
                shape=(self.config.action_dim,), dtype=np.float32
            )

        # Environment state
        self.current_tasks: List[Dict] = []
        self.task_assignments: Dict[int, int] = {}  # robot_id -> task_id
        self._step_count = 0

    def reset(self, seed=None, options=None):
        """Reset environment with new tasks."""
        self.agents = self.possible_agents.copy()
        self._step_count = 0

        # Generate random tasks
        self.current_tasks = self._generate_tasks(num_tasks=5)
        self.task_assignments = {}

        # Build observations
        observations = {}
        infos = {}

        # Allocator observation (global state)
        observations["allocator"] = self._build_allocator_observation()
        infos["allocator"] = {}

        # Robot observations (local state)
        for i in range(self.config.num_agents):
            robot_id = f"robot_{i}"
            observations[robot_id] = self._build_robot_observation(i)
            infos[robot_id] = {}

        return observations, infos

    def step(self, actions: Dict[str, Any]):
        """Execute allocation and robot actions."""
        observations = {}
        rewards = {}
        terminations = {}
        truncations = {}
        infos = {}

        # Step 1: Allocator assigns tasks
        if "allocator" in actions:
            allocation_action = actions["allocator"]
            self._apply_allocation(allocation_action)

            # Allocator reward: based on allocation quality
            allocation_reward = self._compute_allocation_reward()
            rewards["allocator"] = allocation_reward
            observations["allocator"] = self._build_allocator_observation()
            terminations["allocator"] = False
            truncations["allocator"] = False
            infos["allocator"] = {}

        # Step 2: Robots execute navigation actions
        total_task_reward = 0.0
        for i in range(self.config.num_agents):
            robot_id = f"robot_{i}"
            robot_action = actions.get(robot_id, np.zeros(self.config.action_dim))

            # Execute robot action (navigation)
            obs, reward, done, truncated, info = self._step_robot(i, robot_action)

            observations[robot_id] = obs
            rewards[robot_id] = reward
            terminations[robot_id] = done
            truncations[robot_id] = truncated
            infos[robot_id] = info

            total_task_reward += reward

        # Shared reward: allocator gets credit for robot performance
        if "allocator" in rewards:
            rewards["allocator"] += 0.1 * total_task_reward  # Scaled fleet reward

        self._step_count += 1

        # Global truncation
        if self._step_count >= self.config.max_steps:
            for agent in self.agents:
                truncations[agent] = True

        return observations, rewards, terminations, truncations, infos

    def _apply_allocation(self, allocation_action: np.ndarray):
        """Apply allocator's assignment decisions.

        Args:
            allocation_action: Array of task IDs for each robot
        """
        for robot_idx, task_idx in enumerate(allocation_action):
            if task_idx < len(self.current_tasks):
                self.task_assignments[robot_idx] = task_idx

    def _compute_allocation_reward(self) -> float:
        """Compute reward for allocation quality.

        Returns:
            Reward scalar (higher = better allocation)
        """
        if not self.task_assignments:
            return -1.0  # Penalty for no allocation

        # Reward factors:
        # 1. Load balance: prefer even distribution
        assigned_counts = {}
        for robot_id in self.task_assignments.values():
            assigned_counts[robot_id] = assigned_counts.get(robot_id, 0) + 1

        load_variance = np.var(list(assigned_counts.values())) if assigned_counts else 0.0
        load_balance_reward = -0.1 * load_variance

        # 2. Distance efficiency: prefer close assignments
        # (Compute based on robot positions and task locations)
        distance_penalty = 0.0  # Placeholder

        # 3. Coverage: reward for assigning all tasks
        coverage_reward = 1.0 if len(self.task_assignments) == len(self.current_tasks) else 0.0

        total_reward = load_balance_reward + distance_penalty + coverage_reward
        return total_reward

    def _build_allocator_observation(self) -> NDArray[np.float32]:
        """Build global observation for allocator."""
        # Flatten robot states and task states into single vector
        # Placeholder implementation
        obs = np.zeros(200, dtype=np.float32)  # Fixed size
        return obs

    def _build_robot_observation(self, robot_id: int) -> NDArray[np.float32]:
        """Build local observation for robot."""
        # Include assigned task in observation
        assigned_task = self.task_assignments.get(robot_id, -1)
        obs = np.zeros(self.config.obs_dim, dtype=np.float32)
        # Fill with robot state + assigned task info
        return obs

    def _step_robot(self, robot_id: int, action: np.ndarray):
        """Execute robot navigation action."""
        # Placeholder: Call ROS2 step service
        obs = np.zeros(self.config.obs_dim, dtype=np.float32)
        reward = 0.0
        done = False
        truncated = False
        info = {}
        return obs, reward, done, truncated, info

    def _generate_tasks(self, num_tasks: int) -> List[Dict]:
        """Generate random tasks for episode."""
        tasks = []
        for i in range(num_tasks):
            tasks.append({
                'id': i,
                'pickup_x': np.random.uniform(0, 20),
                'pickup_y': np.random.uniform(0, 20),
                'dropoff_x': np.random.uniform(0, 20),
                'dropoff_y': np.random.uniform(0, 20),
                'priority': np.random.uniform(0.3, 1.0)
            })
        return tasks
```

#### Training Script for Allocation Policy

```python
"""Train allocation policy with PPO."""

from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv
from training.envs.allocation_env import WarehouseAllocationEnv


def train_allocation_policy():
    """Train centralized allocation policy."""

    # Create environment
    env = WarehouseAllocationEnv()

    # Wrap for SB3 (requires custom wrapper for multi-agent)
    # vec_env = DummyVecEnv([lambda: env])

    # Create PPO model
    model = PPO(
        "MlpPolicy",
        env,
        learning_rate=3e-4,
        n_steps=2048,
        batch_size=64,
        n_epochs=10,
        gamma=0.99,
        gae_lambda=0.95,
        clip_range=0.2,
        verbose=1
    )

    # Train
    model.learn(total_timesteps=1_000_000)

    # Save
    model.save("allocation_policy")
    print("Allocation policy saved!")


if __name__ == "__main__":
    train_allocation_policy()
```

---

## 5. Comparison and Recommendations

### Algorithm Comparison Matrix

| Algorithm | Optimality | Complexity | Scalability | Dynamic | Distributed | Best For |
|-----------|-----------|-----------|-------------|---------|-------------|----------|
| **Hungarian** | Optimal | O(n³) | Small fleets (<20) | Poor | No | Baseline, benchmarking |
| **Auction** | Sub-optimal | O(n log n) | Large fleets (20+) | Good | Yes | Real-time, distributed |
| **Priority Queue** | Heuristic | O(log n) | Any | Excellent | Partial | High-throughput, simple |
| **RL-Based** | Learned | O(1) inference | Large fleets | Excellent | Yes* | Complex objectives, long-term |

*Depends on architecture (centralized critic common)

### Implementation Roadmap for Warehouser

#### Phase 1: Baseline (Immediate)

**Algorithm:** Hungarian (Centralized)

**Why:** Establishes optimal baseline for comparison

**Implementation:**
1. Add `scipy` dependency to `training/pyproject.toml`
2. Implement `HungarianAllocator` class in `training/training/allocator/`
3. Create ROS2 service interface in `warehouser_task/task_manager_node.hpp`
4. Add periodic re-allocation timer (every 5-10 seconds)

**Integration Points:**
- Call from `TaskManagerNode` when new tasks arrive
- Cost matrix based on robot positions from `WorldState`
- Publish allocations as `Goal` messages to individual robots

#### Phase 2: Dynamic Re-allocation (Medium-term)

**Algorithm:** Auction-based (Distributed)

**Why:** Handles robot failures and dynamic task arrival

**Implementation:**
1. Create `AuctionAllocatorNode` in `warehouser_task/`
2. Add bidding capability to `TaskManagerNode` per robot
3. Define custom messages: `TaskBid.srv`, `TaskAward.msg`
4. Implement re-auction on task timeout

**Integration Points:**
- Trigger auction on new task arrival
- Re-auction on robot failure (detected via heartbeat)
- Priority boost for timed-out tasks

#### Phase 3: Learning-Based (Long-term)

**Algorithm:** Multi-agent RL allocation policy

**Why:** Optimizes for complex objectives and learns from experience

**Implementation:**
1. Extend `WarehouseParallelEnv` with allocator agent
2. Train centralized allocation policy with PPO
3. Export allocation policy to ONNX
4. Load policy in `warehouser_task/allocation_node`

**Integration Points:**
- Allocator observes global fleet state
- Outputs task assignments as discrete actions
- Robots execute assigned tasks via navigation policy
- Reward shaping for fleet efficiency metrics

---

## 6. Copy-Paste Integration Snippets

### Add Hungarian Allocator to Task Manager (C++)

```cpp
// warehouser_task/src/task_manager_node.cpp

#include "warehouser_task/hungarian_allocator.hpp"

// In TaskManagerNode constructor
allocator_ = std::make_unique<HungarianAllocator>(
    /*distance_weight=*/1.0f,
    /*battery_weight=*/0.5f,
    /*load_weight=*/0.3f,
    /*priority_weight=*/2.0f
);

// Periodic allocation callback
void TaskManagerNode::periodicAllocation() {
    // Collect robot states
    std::vector<RobotState> robots;
    for (const auto& robot : world_state_.robots) {
        robots.push_back(RobotState{
            .id = robot.id,
            .x = robot.x,
            .y = robot.y,
            .battery = robot.battery,
            .is_carrying = robot.is_carrying,
            .current_load = robot.current_load
        });
    }

    // Collect pending tasks
    std::vector<TaskRequest> tasks;
    for (const auto& task : pending_tasks_) {
        tasks.push_back(TaskRequest{
            .id = task.id,
            .pickup_x = task.pickup_x,
            .pickup_y = task.pickup_y,
            .dropoff_x = task.dropoff_x,
            .dropoff_y = task.dropoff_y,
            .priority = task.priority
        });
    }

    // Allocate
    auto assignments = allocator_->allocate(robots, tasks);

    // Publish goals
    for (const auto& [robot_id, task_id] : assignments) {
        publishGoalToRobot(robot_id, tasks[task_id]);
    }
}
```

### Python Service for Allocation Request

```python
# warehouser_task/scripts/allocation_service.py

import rclpy
from rclpy.node import Node
from warehouser_msgs.srv import AllocateTasks
from training.allocator.hungarian import HungarianTaskAllocator


class AllocationServiceNode(Node):
    def __init__(self):
        super().__init__('allocation_service')
        self.allocator = HungarianTaskAllocator()

        self.service = self.create_service(
            AllocateTasks,
            'allocate_tasks',
            self.handle_allocation_request
        )

    def handle_allocation_request(self, request, response):
        """Handle task allocation request."""
        robots = [
            Robot(
                id=r.id,
                x=r.x,
                y=r.y,
                battery=r.battery,
                is_carrying=r.is_carrying,
                current_load=r.current_load
            )
            for r in request.robots
        ]

        tasks = [
            Task(
                id=t.id,
                pickup_x=t.pickup_x,
                pickup_y=t.pickup_y,
                dropoff_x=t.dropoff_x,
                dropoff_y=t.dropoff_y,
                priority=t.priority
            )
            for t in request.tasks
        ]

        assignments = self.allocator.allocate(robots, tasks)

        response.assignments = [
            Assignment(robot_id=r_id, task_id=t_id)
            for r_id, t_id in assignments
        ]
        response.success = True
        return response


def main():
    rclpy.init()
    node = AllocationServiceNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
```

---

## Sources and References

### Academic Papers
- [A Systematic Literature Review on Multi-Robot Task Allocation | ACM Computing Surveys](https://dl.acm.org/doi/10.1145/3700591)
- [ROS2-based Distributed Task Allocation Framework | Springer 2025](https://link.springer.com/article/10.1007/s12555-025-0530-7)
- [Distributed Hungarian Method | Springer](https://link.springer.com/chapter/10.1007/978-3-642-13022-9_72)
- [Warehouse Operations Using Heterogeneous Robotic Systems | EAI](https://publications.eai.eu/index.php/airo/article/view/9913)

### Documentation
- [scipy.optimize.linear_sum_assignment](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.linear_sum_assignment.html)
- [Python Priority Queue Guide](https://docs.python.org/3/library/queue.html)
- [PettingZoo Documentation](https://pettingzoo.farama.org/index.html)

### Code References
- Warehouser `training/training/envs/pettingzoo_env.py` (multi-agent environment)
- Warehouser `ros_ws/src/warehouser_task/` (task management architecture)

---

## Next Steps

1. **Implement Hungarian allocator** in `training/training/allocator/hungarian.py`
2. **Create ROS2 service interface** in `warehouser_msgs/srv/AllocateTasks.srv`
3. **Integrate with TaskManagerNode** for periodic allocation
4. **Benchmark allocation quality** against greedy baseline
5. **Add auction-based allocator** for dynamic scenarios
6. **Train RL-based allocation policy** for long-term optimization

**Priority:** Start with Hungarian algorithm as optimal baseline, then transition to auction-based for scalability.
