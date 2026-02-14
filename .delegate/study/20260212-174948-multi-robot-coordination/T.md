# Template Analysis: Multi-Robot Coordination Patterns

Created: 2026-02-12T17:51:00Z

## Source

Templates derived from research in S.md covering:
- Open-RMF fleet adapter patterns
- PettingZoo multi-agent RL frameworks
- CBS/MAPF algorithm implementations
- ROS2 multi-robot communication patterns
- VDA5050 protocol specifications
- Traffic management systems

## Pattern 1: ROS2 Multi-Robot Namespace Pattern

### Source
ROS2 multi-robot communication best practices (Clearpath Robotics, Husarion)

### Pattern

**Per-Robot Namespace Launch Pattern:**

```xml
<!-- launch/multi_robot.launch.py -->
from launch import LaunchDescription
from launch.actions import GroupAction
from launch_ros.actions import Node, PushRosNamespace
from launch.substitutions import LaunchConfiguration
import os

def generate_launch_description():
    num_robots = LaunchConfiguration('num_robots', default=3)

    robot_nodes = []

    for i in range(int(num_robots)):
        robot_namespace = f'robot{i}'

        # Group all nodes for this robot under namespace
        robot_group = GroupAction([
            PushRosNamespace(robot_namespace),

            # Simulation entity
            Node(
                package='warehouser_simulation',
                executable='robot_entity',
                name='entity',
                parameters=[{
                    'robot_id': i,
                    'initial_x': float(i * 2.0),
                    'initial_y': 0.0,
                }]
            ),

            # RL bridge
            Node(
                package='warehouser_rl_bridge',
                executable='rl_bridge',
                name='rl_bridge',
                parameters=[{
                    'robot_id': i,
                }]
            ),

            # Observations
            Node(
                package='warehouser_observations',
                executable='observation_builder',
                name='observations',
                parameters=[{
                    'robot_id': i,
                    'sensor_range': 5.0,
                }]
            ),
        ])

        robot_nodes.append(robot_group)

    # Add global nodes (no namespace)
    global_nodes = [
        Node(
            package='warehouser_simulation',
            executable='world_manager',
            name='world_manager',
            parameters=[{
                'num_robots': num_robots,
            }]
        ),
        Node(
            package='warehouser_traffic',
            executable='traffic_manager',
            name='traffic_manager',
            parameters=[{
                'num_robots': num_robots,
            }]
        ),
    ]

    return LaunchDescription(robot_nodes + global_nodes)
```

**Discovery Server Configuration:**

```bash
# config/fastdds_discovery_server.xml
<?xml version="1.0" encoding="UTF-8" ?>
<profiles xmlns="http://www.eprosima.com/XMLSchemas/fastRTPS_Profiles">

    <!-- Discovery Server -->
    <participant profile_name="discovery_server" is_default_profile="true">
        <rtps>
            <builtin>
                <discovery_config>
                    <discoveryProtocol>SERVER</discoveryProtocol>
                    <discoveryServersList>
                        <RemoteServer prefix="44.53.00.5f.45.50.52.4f.53.49.4d.41">
                            <metatrafficUnicastLocatorList>
                                <locator>
                                    <udpv4>
                                        <address>127.0.0.1</address>
                                        <port>11811</port>
                                    </udpv4>
                                </locator>
                            </metatrafficUnicastLocatorList>
                        </RemoteServer>
                    </discoveryServersList>
                </discovery_config>
            </builtin>
        </rtps>
    </participant>

    <!-- Client participant -->
    <participant profile_name="client" is_default_profile="false">
        <rtps>
            <builtin>
                <discovery_config>
                    <discoveryProtocol>CLIENT</discoveryProtocol>
                    <discoveryServersList>
                        <RemoteServer prefix="44.53.00.5f.45.50.52.4f.53.49.4d.41">
                            <metatrafficUnicastLocatorList>
                                <locator>
                                    <udpv4>
                                        <address>127.0.0.1</address>
                                        <port>11811</port>
                                    </udpv4>
                                </locator>
                            </metatrafficUnicastLocatorList>
                        </RemoteServer>
                    </discoveryServersList>
                </discovery_config>
            </builtin>
        </rtps>
    </participant>

</profiles>
```

**Environment Setup:**

```bash
# Start discovery server
export FASTDDS_DEFAULT_PROFILES_FILE=/path/to/fastdds_discovery_server.xml
fastdds discovery --server-id 0 --port 11811

# Launch clients
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTDDS_DEFAULT_PROFILES_FILE=/path/to/fastdds_discovery_server.xml
ros2 launch warehouser_bringup multi_robot.launch.py num_robots:=5
```

### Application to Warehouser

1. **Create `ros_ws/src/warehouser_bringup/launch/multi_robot.launch.py`**:
   - Use GroupAction with PushRosNamespace for per-robot isolation
   - Launch all robot-specific nodes under `/robot{id}/` namespace
   - Keep world_manager and traffic_manager in global namespace

2. **Add Discovery Server config to `ros_ws/config/`**:
   - Reduces network overhead for >10 robots
   - Faster discovery, more predictable behavior
   - Configure in fastdds_discovery_server.xml

3. **Update existing nodes to be namespace-aware**:
   - Topic names use relative paths (e.g., `cmd_vel` becomes `/robot0/cmd_vel`)
   - Services include robot_id parameter
   - World manager subscribes to all robot namespaces

## Pattern 2: MAPPO (Multi-Agent PPO) Implementation

### Source
PettingZoo ParallelEnv + MAPPO research papers + CleanRL implementations

### Pattern

**Centralized Critic, Decentralized Actor Architecture:**

```python
# training/training/algorithms/mappo.py
from dataclasses import dataclass
import numpy as np
import torch
import torch.nn as nn
from typing import Dict, List, Tuple
from pydantic import BaseModel

class MAPPOConfig(BaseModel):
    """Configuration for MAPPO algorithm."""
    n_agents: int
    obs_dim: int
    global_state_dim: int
    action_dim: int
    hidden_dim: int = 256
    lr_actor: float = 3e-4
    lr_critic: float = 1e-3
    gamma: float = 0.99
    gae_lambda: float = 0.95
    clip_ratio: float = 0.2
    value_clip: float = 0.2
    entropy_coef: float = 0.01
    max_grad_norm: float = 0.5
    parameter_sharing: bool = True  # Share actor across agents


class Actor(nn.Module):
    """Decentralized actor - uses local observations."""

    def __init__(self, obs_dim: int, action_dim: int, hidden_dim: int):
        super().__init__()
        self.network = nn.Sequential(
            nn.Linear(obs_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, action_dim),
        )

    def forward(self, obs: torch.Tensor) -> torch.Tensor:
        """
        Args:
            obs: (batch, obs_dim) local observations
        Returns:
            action_logits: (batch, action_dim)
        """
        return self.network(obs)

    def get_action(self, obs: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        """Sample action and compute log probability."""
        logits = self.forward(obs)
        dist = torch.distributions.Categorical(logits=logits)
        action = dist.sample()
        log_prob = dist.log_prob(action)
        entropy = dist.entropy()
        return action, log_prob, entropy


class CentralizedCritic(nn.Module):
    """Centralized critic - uses global state."""

    def __init__(self, global_state_dim: int, hidden_dim: int):
        super().__init__()
        self.network = nn.Sequential(
            nn.Linear(global_state_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
        )

    def forward(self, global_state: torch.Tensor) -> torch.Tensor:
        """
        Args:
            global_state: (batch, global_state_dim) - all robots, tasks, map
        Returns:
            value: (batch, 1) - estimated value of global state
        """
        return self.network(global_state)


class MAPPO:
    """Multi-Agent PPO with centralized critic, decentralized actors."""

    def __init__(self, config: MAPPOConfig):
        self.config = config

        # Centralized critic (single network for all agents)
        self.critic = CentralizedCritic(
            config.global_state_dim,
            config.hidden_dim
        )

        # Decentralized actors
        if config.parameter_sharing:
            # Single actor shared across all agents (more sample efficient)
            # Agent ID can be part of observation for heterogeneity
            self.actors = [Actor(config.obs_dim, config.action_dim, config.hidden_dim)]
        else:
            # Separate actor for each agent
            self.actors = [
                Actor(config.obs_dim, config.action_dim, config.hidden_dim)
                for _ in range(config.n_agents)
            ]

        # Optimizers
        self.critic_optimizer = torch.optim.Adam(
            self.critic.parameters(), lr=config.lr_critic
        )
        self.actor_optimizers = [
            torch.optim.Adam(actor.parameters(), lr=config.lr_actor)
            for actor in self.actors
        ]

    def get_actor(self, agent_id: int) -> Actor:
        """Get actor for agent (same if parameter sharing)."""
        if self.config.parameter_sharing:
            return self.actors[0]
        return self.actors[agent_id]

    def select_actions(
        self,
        observations: Dict[str, np.ndarray]
    ) -> Tuple[Dict[str, int], Dict[str, float], Dict[str, float]]:
        """
        Select actions for all agents.

        Args:
            observations: {agent_id: obs} where obs is (obs_dim,)
        Returns:
            actions: {agent_id: action}
            log_probs: {agent_id: log_prob}
            entropies: {agent_id: entropy}
        """
        actions = {}
        log_probs = {}
        entropies = {}

        for agent_id, obs in observations.items():
            idx = int(agent_id.split('_')[1])  # Extract number from 'agent_0'
            actor = self.get_actor(idx)

            obs_tensor = torch.FloatTensor(obs).unsqueeze(0)
            action, log_prob, entropy = actor.get_action(obs_tensor)

            actions[agent_id] = action.item()
            log_probs[agent_id] = log_prob.item()
            entropies[agent_id] = entropy.item()

        return actions, log_probs, entropies

    def compute_gae(
        self,
        rewards: torch.Tensor,
        values: torch.Tensor,
        dones: torch.Tensor,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        """
        Compute Generalized Advantage Estimation.

        Args:
            rewards: (batch, timesteps)
            values: (batch, timesteps)
            dones: (batch, timesteps)
        Returns:
            advantages: (batch, timesteps)
            returns: (batch, timesteps)
        """
        advantages = torch.zeros_like(rewards)
        returns = torch.zeros_like(rewards)

        gae = 0
        next_value = 0

        for t in reversed(range(rewards.shape[1])):
            delta = rewards[:, t] + self.config.gamma * next_value * (1 - dones[:, t]) - values[:, t]
            gae = delta + self.config.gamma * self.config.gae_lambda * (1 - dones[:, t]) * gae
            advantages[:, t] = gae
            returns[:, t] = gae + values[:, t]
            next_value = values[:, t]

        return advantages, returns

    def update(
        self,
        observations: torch.Tensor,  # (batch, n_agents, obs_dim)
        actions: torch.Tensor,  # (batch, n_agents)
        old_log_probs: torch.Tensor,  # (batch, n_agents)
        global_states: torch.Tensor,  # (batch, global_state_dim)
        rewards: torch.Tensor,  # (batch,) - shared reward
        dones: torch.Tensor,  # (batch,)
        n_epochs: int = 10,
    ) -> Dict[str, float]:
        """Update actors and critic."""

        batch_size = observations.shape[0]

        # Compute values with centralized critic
        values = self.critic(global_states).squeeze(-1)

        # Compute advantages and returns
        advantages, returns = self.compute_gae(
            rewards.unsqueeze(1).expand(-1, self.config.n_agents),
            values.unsqueeze(1).expand(-1, self.config.n_agents),
            dones.unsqueeze(1).expand(-1, self.config.n_agents),
        )

        # Normalize advantages
        advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

        metrics = {
            'actor_loss': 0.0,
            'critic_loss': 0.0,
            'entropy': 0.0,
            'kl_divergence': 0.0,
        }

        for epoch in range(n_epochs):
            # Update critic
            new_values = self.critic(global_states).squeeze(-1)

            if self.config.value_clip > 0:
                # Clipped value loss
                value_pred_clipped = values + torch.clamp(
                    new_values - values,
                    -self.config.value_clip,
                    self.config.value_clip
                )
                value_loss_1 = (new_values - returns.mean(dim=1)) ** 2
                value_loss_2 = (value_pred_clipped - returns.mean(dim=1)) ** 2
                critic_loss = torch.max(value_loss_1, value_loss_2).mean()
            else:
                critic_loss = ((new_values - returns.mean(dim=1)) ** 2).mean()

            self.critic_optimizer.zero_grad()
            critic_loss.backward()
            nn.utils.clip_grad_norm_(self.critic.parameters(), self.config.max_grad_norm)
            self.critic_optimizer.step()

            # Update actors
            actor_loss_total = 0
            entropy_total = 0
            kl_total = 0

            for agent_idx in range(self.config.n_agents):
                actor = self.get_actor(agent_idx)
                optimizer = self.actor_optimizers[0 if self.config.parameter_sharing else agent_idx]

                obs_agent = observations[:, agent_idx, :]
                actions_agent = actions[:, agent_idx]
                old_log_probs_agent = old_log_probs[:, agent_idx]
                advantages_agent = advantages[:, agent_idx]

                # Compute new log probs
                logits = actor(obs_agent)
                dist = torch.distributions.Categorical(logits=logits)
                new_log_probs = dist.log_prob(actions_agent)
                entropy = dist.entropy().mean()

                # PPO clipped objective
                ratio = torch.exp(new_log_probs - old_log_probs_agent)
                surr1 = ratio * advantages_agent
                surr2 = torch.clamp(ratio, 1 - self.config.clip_ratio, 1 + self.config.clip_ratio) * advantages_agent
                actor_loss = -torch.min(surr1, surr2).mean()

                # Total loss with entropy bonus
                loss = actor_loss - self.config.entropy_coef * entropy

                optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(actor.parameters(), self.config.max_grad_norm)
                optimizer.step()

                # KL divergence for early stopping
                kl = (old_log_probs_agent - new_log_probs).mean()

                actor_loss_total += actor_loss.item()
                entropy_total += entropy.item()
                kl_total += kl.item()

            metrics['actor_loss'] += actor_loss_total / self.config.n_agents
            metrics['critic_loss'] += critic_loss.item()
            metrics['entropy'] += entropy_total / self.config.n_agents
            metrics['kl_divergence'] += kl_total / self.config.n_agents

        # Average over epochs
        for key in metrics:
            metrics[key] /= n_epochs

        return metrics
```

**Global State Construction for Warehouser:**

```python
# training/training/envs/warehouser_env.py
def get_global_state(self) -> np.ndarray:
    """
    Construct global state for centralized critic.

    Global state includes:
    - All robot positions (n_robots * 3: x, y, theta)
    - All robot velocities (n_robots * 2: vx, vy)
    - All task locations (n_tasks * 2: x, y)
    - Task statuses (n_tasks: 0=available, 1=assigned, 2=completed)
    - Zone occupancy counts (n_zones)
    - Static warehouse map (flattened grid)
    """
    n_robots = len(self.agents)

    # Robot states
    robot_positions = np.zeros(n_robots * 3, dtype=np.float32)
    robot_velocities = np.zeros(n_robots * 2, dtype=np.float32)

    for i, agent_id in enumerate(self.agents):
        robot_state = self._get_robot_state(agent_id)
        robot_positions[i*3:(i+1)*3] = robot_state['position']
        robot_velocities[i*2:(i+1)*2] = robot_state['velocity']

    # Task states
    task_locations = np.array([task['position'] for task in self.tasks], dtype=np.float32).flatten()
    task_statuses = np.array([task['status'] for task in self.tasks], dtype=np.float32)

    # Zone occupancy
    zone_counts = self._compute_zone_occupancy()

    # Warehouse map (static, can cache)
    warehouse_map = self.warehouse_map.flatten()

    global_state = np.concatenate([
        robot_positions,
        robot_velocities,
        task_locations,
        task_statuses,
        zone_counts,
        warehouse_map,
    ])

    return global_state
```

### Application to Warehouser

1. **Add `training/training/algorithms/mappo.py`**:
   - Implement Actor, CentralizedCritic, MAPPO classes above
   - Use with existing PettingZoo ParallelEnv

2. **Extend `training/training/envs/ros_env.py`**:
   - Add `get_global_state()` method
   - Collect global state during rollouts
   - Pass to centralized critic during training

3. **Create training script `training/scripts/train_mappo.py`**:
   ```python
   from training.envs.ros_env import WarehouseMultiEnv
   from training.algorithms.mappo import MAPPO, MAPPOConfig

   env = WarehouseMultiEnv(num_robots=5)
   config = MAPPOConfig(
       n_agents=5,
       obs_dim=env.observation_space.shape[0],
       global_state_dim=...,  # Computed from get_global_state
       action_dim=env.action_space.n,
       parameter_sharing=True,
   )

   agent = MAPPO(config)
   # Training loop...
   ```

4. **Comparison baseline with IPPO**:
   - Train IPPO (independent PPO) first
   - Train MAPPO with same hyperparameters
   - Compare: fleet throughput, collisions, task completion time

## Pattern 3: Zone-Based Traffic Management

### Source
Traffic management research (hierarchical systems, zone control, deadlock prevention)

### Pattern

**Zone Data Structure:**

```cpp
// ros_ws/src/warehouser_traffic/include/warehouser_traffic/zone_manager.hpp
#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <expected>
#include <string>

namespace warehouser_traffic {

struct Zone {
    uint32_t id;
    std::string name;
    std::vector<std::pair<float, float>> boundary_points;  // Polygon vertices
    uint32_t capacity;  // Max robots allowed
    bool is_nonstop_area;  // Robots cannot stop here (intersections)
    std::unordered_set<uint32_t> current_robots;  // Robot IDs currently in zone

    [[nodiscard]] auto is_full() const -> bool {
        return current_robots.size() >= capacity;
    }

    [[nodiscard]] auto has_capacity_for(uint32_t n_robots) const -> bool {
        return current_robots.size() + n_robots <= capacity;
    }
};

struct ZoneReservation {
    uint32_t robot_id;
    uint32_t zone_id;
    float entry_time;  // Planned entry time
    float exit_time;   // Planned exit time
    uint32_t priority;  // Higher = more important
};

class ZoneManager {
public:
    ZoneManager() = default;

    // Zone definition
    auto add_zone(
        uint32_t zone_id,
        std::string name,
        std::vector<std::pair<float, float>> boundary,
        uint32_t capacity,
        bool is_nonstop
    ) -> void;

    // Zone queries
    [[nodiscard]] auto get_zone(uint32_t zone_id) const -> std::expected<Zone, std::string>;
    [[nodiscard]] auto get_zone_at_position(float x, float y) const -> std::expected<uint32_t, std::string>;
    [[nodiscard]] auto is_position_in_zone(float x, float y, uint32_t zone_id) const -> bool;

    // Reservation system
    [[nodiscard]] auto request_reservation(ZoneReservation reservation) -> std::expected<bool, std::string>;
    auto release_reservation(uint32_t robot_id, uint32_t zone_id) -> void;

    // Occupancy tracking
    auto robot_entered_zone(uint32_t robot_id, uint32_t zone_id) -> std::expected<void, std::string>;
    auto robot_exited_zone(uint32_t robot_id, uint32_t zone_id) -> void;

    // Conflict detection
    [[nodiscard]] auto check_conflicts(const std::vector<ZoneReservation>& reservations) const
        -> std::vector<std::pair<uint32_t, uint32_t>>;  // Pairs of conflicting robot IDs

    // Deadlock detection
    [[nodiscard]] auto detect_deadlock() const -> std::vector<uint32_t>;  // Robot IDs in deadlock

private:
    std::unordered_map<uint32_t, Zone> zones_;
    std::vector<ZoneReservation> active_reservations_;

    [[nodiscard]] auto point_in_polygon(
        float x, float y,
        const std::vector<std::pair<float, float>>& polygon
    ) const -> bool;

    [[nodiscard]] auto reservations_overlap(
        const ZoneReservation& r1,
        const ZoneReservation& r2
    ) const -> bool;
};

} // namespace warehouser_traffic
```

**Zone Manager Implementation:**

```cpp
// ros_ws/src/warehouser_traffic/src/zone_manager.cpp
#include "warehouser_traffic/zone_manager.hpp"
#include <algorithm>
#include <cmath>

namespace warehouser_traffic {

auto ZoneManager::add_zone(
    uint32_t zone_id,
    std::string name,
    std::vector<std::pair<float, float>> boundary,
    uint32_t capacity,
    bool is_nonstop
) -> void {
    zones_[zone_id] = Zone{
        .id = zone_id,
        .name = std::move(name),
        .boundary_points = std::move(boundary),
        .capacity = capacity,
        .is_nonstop_area = is_nonstop,
        .current_robots = {},
    };
}

auto ZoneManager::get_zone_at_position(float x, float y) const
    -> std::expected<uint32_t, std::string> {

    for (const auto& [zone_id, zone] : zones_) {
        if (point_in_polygon(x, y, zone.boundary_points)) {
            return zone_id;
        }
    }

    return std::unexpected("No zone found at position");
}

auto ZoneManager::request_reservation(ZoneReservation reservation)
    -> std::expected<bool, std::string> {

    auto zone_result = get_zone(reservation.zone_id);
    if (!zone_result) {
        return std::unexpected(zone_result.error());
    }

    const auto& zone = *zone_result;

    // Check capacity
    if (zone.is_full()) {
        // Check if we can preempt lower priority reservations
        std::vector<ZoneReservation> overlapping;
        for (const auto& existing : active_reservations_) {
            if (existing.zone_id == reservation.zone_id &&
                reservations_overlap(existing, reservation)) {
                overlapping.push_back(existing);
            }
        }

        // Sort by priority (descending)
        std::sort(overlapping.begin(), overlapping.end(),
            [](const auto& a, const auto& b) { return a.priority > b.priority; });

        // Can we preempt lowest priority reservation?
        if (!overlapping.empty() && overlapping.back().priority < reservation.priority) {
            // Remove lowest priority reservation
            auto it = std::remove_if(active_reservations_.begin(), active_reservations_.end(),
                [&](const auto& r) {
                    return r.robot_id == overlapping.back().robot_id &&
                           r.zone_id == overlapping.back().zone_id;
                });
            active_reservations_.erase(it, active_reservations_.end());
        } else {
            return false;  // Cannot reserve, no preemption possible
        }
    }

    // Add reservation
    active_reservations_.push_back(reservation);
    return true;
}

auto ZoneManager::robot_entered_zone(uint32_t robot_id, uint32_t zone_id)
    -> std::expected<void, std::string> {

    auto zone_it = zones_.find(zone_id);
    if (zone_it == zones_.end()) {
        return std::unexpected("Zone not found");
    }

    auto& zone = zone_it->second;

    // Nonstop area violation check
    if (zone.is_nonstop_area) {
        // Robot must have reservation and be moving
        bool has_reservation = std::any_of(
            active_reservations_.begin(),
            active_reservations_.end(),
            [robot_id, zone_id](const auto& r) {
                return r.robot_id == robot_id && r.zone_id == zone_id;
            }
        );

        if (!has_reservation) {
            return std::unexpected("Cannot enter nonstop area without reservation");
        }
    }

    zone.current_robots.insert(robot_id);
    return {};
}

auto ZoneManager::robot_exited_zone(uint32_t robot_id, uint32_t zone_id) -> void {
    auto zone_it = zones_.find(zone_id);
    if (zone_it != zones_.end()) {
        zone_it->second.current_robots.erase(robot_id);
    }

    // Remove reservation
    auto it = std::remove_if(active_reservations_.begin(), active_reservations_.end(),
        [robot_id, zone_id](const auto& r) {
            return r.robot_id == robot_id && r.zone_id == zone_id;
        });
    active_reservations_.erase(it, active_reservations_.end());
}

auto ZoneManager::detect_deadlock() const -> std::vector<uint32_t> {
    // Simple cycle detection in wait-for graph
    // Robot A waits for Robot B if:
    // - A wants to enter zone Z
    // - B is currently in zone Z
    // - Z is at capacity

    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> wait_for_graph;

    for (const auto& reservation : active_reservations_) {
        auto zone_result = get_zone(reservation.zone_id);
        if (!zone_result) continue;

        const auto& zone = *zone_result;

        // If zone is full, robot waits for all robots currently in zone
        if (zone.is_full()) {
            wait_for_graph[reservation.robot_id].insert(
                zone.current_robots.begin(),
                zone.current_robots.end()
            );
        }
    }

    // Detect cycles using DFS
    std::unordered_set<uint32_t> visited;
    std::unordered_set<uint32_t> rec_stack;
    std::vector<uint32_t> deadlock_robots;

    std::function<bool(uint32_t)> has_cycle = [&](uint32_t robot_id) -> bool {
        visited.insert(robot_id);
        rec_stack.insert(robot_id);

        if (wait_for_graph.contains(robot_id)) {
            for (uint32_t waiting_on : wait_for_graph[robot_id]) {
                if (!visited.contains(waiting_on)) {
                    if (has_cycle(waiting_on)) {
                        deadlock_robots.push_back(robot_id);
                        return true;
                    }
                } else if (rec_stack.contains(waiting_on)) {
                    deadlock_robots.push_back(robot_id);
                    return true;
                }
            }
        }

        rec_stack.erase(robot_id);
        return false;
    };

    for (const auto& [robot_id, _] : wait_for_graph) {
        if (!visited.contains(robot_id)) {
            has_cycle(robot_id);
        }
    }

    return deadlock_robots;
}

auto ZoneManager::point_in_polygon(
    float x, float y,
    const std::vector<std::pair<float, float>>& polygon
) const -> bool {
    // Ray casting algorithm
    bool inside = false;
    size_t n = polygon.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = polygon[i].first, yi = polygon[i].second;
        float xj = polygon[j].first, yj = polygon[j].second;

        bool intersect = ((yi > y) != (yj > y)) &&
                        (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }

    return inside;
}

auto ZoneManager::reservations_overlap(
    const ZoneReservation& r1,
    const ZoneReservation& r2
) const -> bool {
    // Time intervals overlap if one starts before the other ends
    return !(r1.exit_time <= r2.entry_time || r2.exit_time <= r1.entry_time);
}

} // namespace warehouser_traffic
```

**ROS2 Service Interface:**

```cpp
// ros_ws/src/warehouser_msgs/srv/ReserveZone.srv
uint32 robot_id
uint32 zone_id
float32 entry_time
float32 exit_time
uint32 priority
---
bool success
string message
```

### Application to Warehouser

1. **Create new package `warehouser_traffic`**:
   ```bash
   cd ros_ws/src
   ros2 pkg create warehouser_traffic --build-type ament_cmake --dependencies rclcpp warehouser_msgs
   ```

2. **Implement ZoneManager** as shown above

3. **Create TrafficManagerNode**:
   ```cpp
   class TrafficManagerNode : public rclcpp::Node {
       ZoneManager zone_manager_;
       rclcpp::Service<warehouser_msgs::srv::ReserveZone>::SharedPtr reserve_service_;
       rclcpp::TimerBase::SharedPtr deadlock_check_timer_;

       // Initialize zones from warehouse config
       // Provide reservation service
       // Periodic deadlock detection
   };
   ```

4. **Define zones in warehouse config**:
   ```yaml
   zones:
     - id: 0
       name: "aisle_1"
       boundary: [[0, 0], [10, 0], [10, 2], [0, 2]]
       capacity: 2
       is_nonstop: false
     - id: 1
       name: "intersection_1"
       boundary: [[10, 0], [12, 0], [12, 2], [10, 2]]
       capacity: 1
       is_nonstop: true  # No stopping allowed
   ```

5. **Integrate with path planning**:
   - Path planner requests zone reservations before executing path
   - If reservation fails, replan with different route
   - Release reservations upon zone exit

## Pattern 4: Conflict-Based Search (CBS) for MAPF

### Source
CBS algorithm research + MAPF-LNS2 implementations

### Pattern

**CBS Core Algorithm (Pseudocode):**

```python
# Conceptual CBS implementation for Warehouser
from dataclasses import dataclass
from typing import List, Tuple, Optional, Set
import heapq

@dataclass
class Constraint:
    """Constraint preventing agent from position at timestep."""
    agent_id: int
    position: Tuple[int, int]
    timestep: int

@dataclass
class Conflict:
    """Detected conflict between agents."""
    agent1: int
    agent2: int
    position: Tuple[int, int]
    timestep: int
    conflict_type: str  # 'vertex' or 'edge'

@dataclass
class CTNode:
    """Node in Constraint Tree."""
    constraints: List[Constraint]
    paths: dict[int, List[Tuple[int, int]]]  # agent_id -> path
    cost: int  # Sum of path lengths

    def __lt__(self, other):
        return self.cost < other.cost

class CBS:
    """Conflict-Based Search for Multi-Agent Path Finding."""

    def __init__(self, grid_map: np.ndarray, starts: dict, goals: dict):
        self.grid_map = grid_map
        self.starts = starts  # {agent_id: (x, y)}
        self.goals = goals    # {agent_id: (x, y)}

    def find_paths(self) -> Optional[dict[int, List[Tuple[int, int]]]]:
        """
        Find collision-free paths for all agents.

        Returns:
            Dictionary mapping agent_id to path, or None if no solution
        """
        # Initialize with root node (no constraints)
        root_paths = {}
        for agent_id in self.starts:
            path = self._low_level_search(agent_id, [])
            if path is None:
                return None  # No solution exists
            root_paths[agent_id] = path

        root_cost = sum(len(path) for path in root_paths.values())
        root = CTNode(constraints=[], paths=root_paths, cost=root_cost)

        # Priority queue (min-heap by cost)
        open_list = [root]
        heapq.heapify(open_list)

        while open_list:
            current = heapq.heappop(open_list)

            # Check for conflicts
            conflict = self._find_first_conflict(current.paths)

            if conflict is None:
                # No conflicts - solution found!
                return current.paths

            # Generate two child nodes with constraints
            for agent_id in [conflict.agent1, conflict.agent2]:
                # Create constraint for this agent
                constraint = Constraint(
                    agent_id=agent_id,
                    position=conflict.position,
                    timestep=conflict.timestep
                )

                # Child node inherits parent constraints + new constraint
                child_constraints = current.constraints + [constraint]

                # Re-plan path for constrained agent
                new_path = self._low_level_search(agent_id, child_constraints)

                if new_path is not None:
                    # Create child node with updated path
                    child_paths = current.paths.copy()
                    child_paths[agent_id] = new_path
                    child_cost = sum(len(path) for path in child_paths.values())

                    child = CTNode(
                        constraints=child_constraints,
                        paths=child_paths,
                        cost=child_cost
                    )

                    heapq.heappush(open_list, child)

        # No solution found
        return None

    def _low_level_search(
        self,
        agent_id: int,
        constraints: List[Constraint]
    ) -> Optional[List[Tuple[int, int]]]:
        """
        A* search for single agent given constraints.

        Args:
            agent_id: Agent to plan for
            constraints: Time-space constraints to avoid
        Returns:
            Path as list of (x, y) positions, or None if no path exists
        """
        start = self.starts[agent_id]
        goal = self.goals[agent_id]

        # Build constraint table for fast lookup
        constraint_table = set()
        for c in constraints:
            if c.agent_id == agent_id:
                constraint_table.add((c.position, c.timestep))

        # A* search with time dimension
        @dataclass
        class SearchNode:
            position: Tuple[int, int]
            timestep: int
            g_cost: int  # Actual cost from start
            h_cost: int  # Heuristic to goal
            parent: Optional['SearchNode'] = None

            @property
            def f_cost(self):
                return self.g_cost + self.h_cost

            def __lt__(self, other):
                return self.f_cost < other.f_cost

        def heuristic(pos: Tuple[int, int]) -> int:
            # Manhattan distance
            return abs(pos[0] - goal[0]) + abs(pos[1] - goal[1])

        def get_neighbors(pos: Tuple[int, int]) -> List[Tuple[int, int]]:
            x, y = pos
            neighbors = []
            for dx, dy in [(0, 1), (1, 0), (0, -1), (-1, 0), (0, 0)]:  # Include wait
                nx, ny = x + dx, y + dy
                if (0 <= nx < self.grid_map.shape[0] and
                    0 <= ny < self.grid_map.shape[1] and
                    self.grid_map[nx, ny] == 0):  # 0 = free space
                    neighbors.append((nx, ny))
            return neighbors

        start_node = SearchNode(
            position=start,
            timestep=0,
            g_cost=0,
            h_cost=heuristic(start)
        )

        open_list = [start_node]
        heapq.heapify(open_list)
        closed_set = set()

        while open_list:
            current = heapq.heappop(open_list)

            if current.position == goal:
                # Reconstruct path
                path = []
                node = current
                while node is not None:
                    path.append(node.position)
                    node = node.parent
                return list(reversed(path))

            state = (current.position, current.timestep)
            if state in closed_set:
                continue
            closed_set.add(state)

            # Expand neighbors
            for neighbor_pos in get_neighbors(current.position):
                next_timestep = current.timestep + 1

                # Check constraints
                if (neighbor_pos, next_timestep) in constraint_table:
                    continue

                neighbor_node = SearchNode(
                    position=neighbor_pos,
                    timestep=next_timestep,
                    g_cost=current.g_cost + 1,
                    h_cost=heuristic(neighbor_pos),
                    parent=current
                )

                heapq.heappush(open_list, neighbor_node)

        # No path found
        return None

    def _find_first_conflict(
        self,
        paths: dict[int, List[Tuple[int, int]]]
    ) -> Optional[Conflict]:
        """
        Find first conflict between agent paths.

        Returns:
            Conflict object or None if paths are conflict-free
        """
        max_timesteps = max(len(path) for path in paths.values())

        for t in range(max_timesteps):
            # Check vertex conflicts (same position at same time)
            positions_at_t = {}
            for agent_id, path in paths.items():
                if t < len(path):
                    pos = path[t]
                else:
                    pos = path[-1]  # Agent stays at goal

                if pos in positions_at_t:
                    return Conflict(
                        agent1=positions_at_t[pos],
                        agent2=agent_id,
                        position=pos,
                        timestep=t,
                        conflict_type='vertex'
                    )
                positions_at_t[pos] = agent_id

            # Check edge conflicts (agents swap positions)
            if t > 0:
                for agent1, path1 in paths.items():
                    for agent2, path2 in paths.items():
                        if agent1 >= agent2:
                            continue

                        pos1_prev = path1[t-1] if t-1 < len(path1) else path1[-1]
                        pos1_curr = path1[t] if t < len(path1) else path1[-1]
                        pos2_prev = path2[t-1] if t-1 < len(path2) else path2[-1]
                        pos2_curr = path2[t] if t < len(path2) else path2[-1]

                        # Edge conflict: (A->B, B->A)
                        if pos1_prev == pos2_curr and pos1_curr == pos2_prev:
                            return Conflict(
                                agent1=agent1,
                                agent2=agent2,
                                position=pos1_curr,
                                timestep=t,
                                conflict_type='edge'
                            )

        return None
```

### Application to Warehouser

1. **Create `warehouser_planning` package**:
   ```bash
   ros2 pkg create warehouser_planning --build-type ament_cmake
   ```

2. **Implement CBS in C++23** (above is Python pseudocode):
   - Use `std::expected` for error handling
   - Use `std::priority_queue` for open lists
   - Integrate with warehouse grid from simulation

3. **Create PlanMultiAgentPath service**:
   ```cpp
   // warehouser_msgs/srv/PlanMultiAgentPath.srv
   uint32[] robot_ids
   geometry_msgs/Point[] starts
   geometry_msgs/Point[] goals
   ---
   bool success
   nav_msgs/Path[] paths  # One path per robot
   string message
   ```

4. **Path execution**:
   - Path planner publishes paths to `/robot{id}/planned_path`
   - Each robot follows its path
   - If collision detected, request replan

5. **Integration with traffic manager**:
   - CBS generates paths
   - Traffic manager validates zone reservations
   - If reservation conflicts, CBS replans with zone constraints

## Pattern 5: VDA5050 Message Format

### Source
VDA5050 specification v2.0 (https://github.com/VDA5050/VDA5050)

### Pattern

**VDA5050 Core Message Structures (JSON/MQTT):**

```json
// Order Message (Master Control -> AGV)
{
  "headerId": 1234,
  "timestamp": "2026-02-12T17:51:00Z",
  "version": "2.0.0",
  "manufacturer": "Warehouser",
  "serialNumber": "robot_0",

  "orderId": "order_42",
  "orderUpdateId": 0,

  "nodes": [
    {
      "nodeId": "node_1",
      "sequenceId": 0,
      "nodePosition": {
        "x": 10.5,
        "y": 5.2,
        "theta": 1.57,
        "mapId": "warehouse_floor_1"
      },
      "actions": [
        {
          "actionType": "pick",
          "actionId": "pick_1",
          "blockingType": "HARD"
        }
      ]
    },
    {
      "nodeId": "node_2",
      "sequenceId": 1,
      "nodePosition": {
        "x": 15.0,
        "y": 5.2,
        "theta": 0.0,
        "mapId": "warehouse_floor_1"
      },
      "actions": [
        {
          "actionType": "drop",
          "actionId": "drop_1",
          "blockingType": "HARD"
        }
      ]
    }
  ],

  "edges": [
    {
      "edgeId": "edge_1",
      "sequenceId": 0,
      "startNodeId": "node_1",
      "endNodeId": "node_2",
      "maxSpeed": 1.5,
      "trajectory": {
        "degree": 1,
        "knotVector": [0, 0, 1, 1],
        "controlPoints": [
          {"x": 10.5, "y": 5.2},
          {"x": 15.0, "y": 5.2}
        ]
      },
      "actions": []
    }
  ]
}
```

```json
// State Message (AGV -> Master Control)
{
  "headerId": 5678,
  "timestamp": "2026-02-12T17:51:05Z",
  "version": "2.0.0",
  "manufacturer": "Warehouser",
  "serialNumber": "robot_0",

  "orderId": "order_42",
  "orderUpdateId": 0,

  "lastNodeId": "node_1",
  "lastNodeSequenceId": 0,

  "driving": true,
  "paused": false,
  "newBaseRequest": false,
  "distanceSinceLastNode": 2.3,

  "operatingMode": "AUTOMATIC",

  "nodeStates": [
    {
      "nodeId": "node_1",
      "sequenceId": 0,
      "released": true
    },
    {
      "nodeId": "node_2",
      "sequenceId": 1,
      "released": false
    }
  ],

  "edgeStates": [
    {
      "edgeId": "edge_1",
      "sequenceId": 0,
      "released": true
    }
  ],

  "agvPosition": {
    "x": 12.8,
    "y": 5.2,
    "theta": 0.0,
    "mapId": "warehouse_floor_1",
    "positionInitialized": true,
    "localizationScore": 0.95,
    "deviationRange": 0.1
  },

  "velocity": {
    "vx": 1.2,
    "vy": 0.0,
    "omega": 0.0
  },

  "actionStates": [
    {
      "actionId": "pick_1",
      "actionStatus": "FINISHED",
      "resultDescription": "Item picked successfully"
    }
  ],

  "batteryState": {
    "batteryCharge": 75.5,
    "batteryVoltage": 48.2,
    "batteryHealth": 95,
    "charging": false,
    "reach": 12000
  },

  "errors": [],
  "informations": []
}
```

**ROS2 Adapter for VDA5050:**

```cpp
// ros_ws/src/warehouser_vda5050/include/warehouser_vda5050/vda5050_adapter.hpp
#pragma once

#include <rclcpp/rclcpp.hpp>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include <warehouser_msgs/msg/robot_state.hpp>
#include <geometry_msgs/msg/twist.hpp>

namespace warehouser_vda5050 {

using json = nlohmann::json;

class VDA5050Adapter : public rclcpp::Node {
public:
    VDA5050Adapter(const std::string& mqtt_broker, const std::string& robot_id);

private:
    // MQTT connection
    std::unique_ptr<mqtt::async_client> mqtt_client_;
    std::string robot_id_;

    // ROS2 publishers
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;

    // ROS2 subscribers
    rclcpp::Subscription<warehouser_msgs::msg::RobotState>::SharedPtr robot_state_sub_;

    // Timers
    rclcpp::TimerBase::SharedPtr state_publish_timer_;

    // Current state
    json current_order_;
    std::string current_order_id_;
    uint32_t current_order_update_id_ = 0;

    // MQTT callbacks
    auto on_order_received(const std::string& topic, const std::string& payload) -> void;
    auto on_instant_action_received(const std::string& topic, const std::string& payload) -> void;

    // ROS2 callbacks
    auto on_robot_state_update(const warehouser_msgs::msg::RobotState::SharedPtr msg) -> void;
    auto publish_state() -> void;

    // Message conversion
    auto order_to_ros_commands(const json& order) -> void;
    auto ros_state_to_vda5050(const warehouser_msgs::msg::RobotState& state) -> json;

    // Validation
    auto validate_order(const json& order) -> std::expected<void, std::string>;
};

} // namespace warehouser_vda5050
```

### Application to Warehouser

1. **VDA5050 is optional** - only implement if interoperability with commercial fleet systems is needed

2. **If implementing**:
   - Create `warehouser_vda5050` package
   - Use MQTT client library (e.g., Paho MQTT C++)
   - Implement VDA5050Adapter node as translation layer
   - Subscribe to MQTT topics: `{manufacturer}/{serialNumber}/order`
   - Publish to MQTT topics: `{manufacturer}/{serialNumber}/state`

3. **Integration points**:
   - VDA5050 orders → ROS2 task messages
   - ROS2 robot state → VDA5050 state messages
   - VDA5050 actions → ROS2 action servers

4. **Benefits**:
   - Compatibility with commercial warehouse systems
   - Standard interface for task assignment
   - Fleet-level coordination with external systems

## Pattern 6: Parameter Sharing in MARL

### Source
Multi-agent RL research (sample efficiency, scalability)

### Pattern

**Parameter-Shared Actor with Agent ID Encoding:**

```python
# training/training/algorithms/shared_actor.py
import torch
import torch.nn as nn
import numpy as np

class ParameterSharedActor(nn.Module):
    """
    Single actor network shared across all agents.
    Agent ID is part of input to handle heterogeneity.
    """

    def __init__(
        self,
        obs_dim: int,
        action_dim: int,
        n_agents: int,
        hidden_dim: int = 256,
        use_agent_id_encoding: bool = True
    ):
        super().__init__()

        self.use_agent_id_encoding = use_agent_id_encoding
        self.n_agents = n_agents

        # Input: observation + one-hot agent ID
        input_dim = obs_dim
        if use_agent_id_encoding:
            input_dim += n_agents  # One-hot encoding

        self.network = nn.Sequential(
            nn.Linear(input_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, action_dim),
        )

    def forward(self, obs: torch.Tensor, agent_ids: torch.Tensor) -> torch.Tensor:
        """
        Args:
            obs: (batch, obs_dim) observations
            agent_ids: (batch,) agent ID integers
        Returns:
            action_logits: (batch, action_dim)
        """
        if self.use_agent_id_encoding:
            # One-hot encode agent IDs
            agent_id_onehot = torch.nn.functional.one_hot(
                agent_ids.long(),
                num_classes=self.n_agents
            ).float()

            # Concatenate observation with agent ID
            input_tensor = torch.cat([obs, agent_id_onehot], dim=-1)
        else:
            input_tensor = obs

        return self.network(input_tensor)

    def get_action(
        self,
        obs: torch.Tensor,
        agent_id: int
    ) -> tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        """Sample action for a single agent."""
        agent_id_tensor = torch.tensor([agent_id])
        logits = self.forward(obs, agent_id_tensor)
        dist = torch.distributions.Categorical(logits=logits)
        action = dist.sample()
        log_prob = dist.log_prob(action)
        entropy = dist.entropy()
        return action, log_prob, entropy


class ParameterSharedMAPPO:
    """MAPPO with parameter sharing for improved sample efficiency."""

    def __init__(
        self,
        n_agents: int,
        obs_dim: int,
        global_state_dim: int,
        action_dim: int,
        hidden_dim: int = 256,
    ):
        self.n_agents = n_agents

        # Single shared actor for all agents
        self.actor = ParameterSharedActor(
            obs_dim, action_dim, n_agents, hidden_dim
        )

        # Centralized critic
        self.critic = nn.Sequential(
            nn.Linear(global_state_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
        )

        # Single optimizer for actor (shared across all agents)
        self.actor_optimizer = torch.optim.Adam(
            self.actor.parameters(), lr=3e-4
        )
        self.critic_optimizer = torch.optim.Adam(
            self.critic.parameters(), lr=1e-3
        )

    def select_actions(
        self,
        observations: dict[str, np.ndarray]
    ) -> tuple[dict, dict, dict]:
        """
        Select actions for all agents using shared actor.

        Args:
            observations: {agent_id: obs_array}
        Returns:
            actions, log_probs, entropies
        """
        actions = {}
        log_probs = {}
        entropies = {}

        for agent_id, obs in observations.items():
            agent_idx = int(agent_id.split('_')[1])
            obs_tensor = torch.FloatTensor(obs).unsqueeze(0)

            action, log_prob, entropy = self.actor.get_action(obs_tensor, agent_idx)

            actions[agent_id] = action.item()
            log_probs[agent_id] = log_prob.item()
            entropies[agent_id] = entropy.item()

        return actions, log_probs, entropies
```

**Benefits:**
- Fewer parameters (1 actor vs N actors)
- Better sample efficiency (more data per parameter)
- Easier to scale to variable team sizes
- Natural transfer learning across robot counts

### Application to Warehouser

1. **Modify MAPPO implementation** to use ParameterSharedActor
2. **Agent ID encoding**: Include robot ID in observation (one-hot or integer)
3. **Training**: All agents contribute gradients to same actor network
4. **Deployment**: Load single actor, use with any number of robots
5. **Experiment**: Compare parameter-shared vs. individual actors

## Summary

These patterns provide copy-paste-ready implementations for:

1. **ROS2 Communication**: Per-robot namespaces, Discovery Server config
2. **MARL**: MAPPO with centralized critic, parameter sharing
3. **Traffic Management**: Zone-based control, deadlock detection
4. **Path Planning**: CBS algorithm for optimal multi-agent paths
5. **Fleet Standards**: VDA5050 message formats (optional)
6. **Scalability**: Parameter sharing for efficient multi-robot learning

## Immediate Next Steps for Warehouser

1. **Phase 1** - Communication:
   - Implement multi_robot.launch.py with namespaces
   - Configure Discovery Server
   - Test with 3-5 robots

2. **Phase 2** - Traffic:
   - Create warehouser_traffic package
   - Implement ZoneManager
   - Define warehouse zones in config

3. **Phase 3** - Learning:
   - Implement MAPPO algorithm
   - Add parameter sharing
   - Train with curriculum (2→5→10 robots)

4. **Phase 4** - Planning:
   - Implement CBS for path planning
   - Integrate with traffic manager
   - Validate optimal paths

All code patterns above use:
- C++23 with `std::expected` (ROS2 nodes)
- Python 3.12+ with type hints (training)
- Warehouser conventions (naming, error handling, testing)
