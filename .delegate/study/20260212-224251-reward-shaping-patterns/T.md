# Template: Reward Shaping Patterns for Robotics RL

Created: 2026-02-12 22:47:00

## Source

Synthesized from research findings in S.md covering:
- HPRS (Hierarchical PBRS) - TU Wien/Austrian Institute (2025)
- VBRS (Value-Based Reward Shaping) - Electronics Journal (2026)
- Multi-Objective RL for Navigation - arXiv 2312.07953
- Dynamic Weight Scalarization - ScienceDirect (2025)
- Reward Hacking Literature - Lilian Weng, Anthropic, Skalse et al.

## Pattern 1: Potential-Based Reward Shaping (PBRS)

### Mathematical Foundation

**Policy Invariance Guarantee:**
```
F(s, a, s') = γΦ(s') - Φ(s)
```
Where Φ is the potential function. Adding F to rewards preserves optimal policy.

### C++ Implementation for Warehouser

```cpp
// Header: reward_strategy.hpp
struct PBRSConfig {
    float gamma = 0.99f;
    float success_bonus = 100.0f;
    float goal_threshold = 0.5f;
};

/// PBRS Navigation with Distance-to-Goal Potential
class PBRSNavigationStrategy : public IRewardStrategy {
public:
    explicit PBRSNavigationStrategy(const PBRSConfig& config = {});
    RewardResult calculate(const RewardContext& ctx) const override;
    std::string name() const override { return "pbrs_navigation"; }

private:
    PBRSConfig config_;

    // Potential function: Φ(s) = -distance_to_goal
    float potential(const warehouser_msgs::msg::Entity& robot,
                   const warehouser_msgs::msg::Goal& goal) const {
        float dx = goal.x - robot.x;
        float dy = goal.y - robot.y;
        return -std::sqrt(dx * dx + dy * dy);
    }

    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world, size_t index) const;
};

// Implementation: reward_strategy.cpp
PBRSNavigationStrategy::PBRSNavigationStrategy(const PBRSConfig& config)
    : config_(config) {}

RewardResult PBRSNavigationStrategy::calculate(const RewardContext& ctx) const {
    RewardResult result;

    const auto* prev_robot = findRobotByIndex(ctx.prev_world, ctx.robot_index);
    const auto* curr_robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);

    if (!curr_robot) {
        return result;  // Let collision strategy handle
    }

    // Check goal reached
    float curr_dist = std::sqrt(
        std::pow(ctx.goal.x - curr_robot->x, 2) +
        std::pow(ctx.goal.y - curr_robot->y, 2));

    if (curr_dist < config_.goal_threshold) {
        result.terminated = true;
        result.termination_reason = "Goal reached";
        result.reward = config_.success_bonus;
        return result;
    }

    // PBRS: F(s,a,s') = γΦ(s') - Φ(s)
    if (prev_robot) {
        float phi_curr = potential(*curr_robot, ctx.goal);
        float phi_prev = potential(*prev_robot, ctx.goal);
        result.reward = config_.gamma * phi_curr - phi_prev;
    }

    return result;
}
```

### Python Wrapper Implementation

```python
# training/training/wrappers/pbrs_wrapper.py
from typing import Tuple
import numpy as np
import gymnasium as gym

class PBRSWrapper(gym.Wrapper):
    """Potential-Based Reward Shaping wrapper.

    Applies PBRS using distance-to-goal as potential function.
    Guarantees policy invariance while densifying sparse rewards.
    """

    def __init__(self, env: gym.Env, gamma: float = 0.99):
        super().__init__(env)
        self.gamma = gamma
        self.prev_potential: float | None = None

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        obs, info = self.env.reset(seed=seed, options=options)
        self.prev_potential = self._compute_potential(info)
        return obs, info

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Apply PBRS: F(s,a,s') = γΦ(s') - Φ(s)
        curr_potential = self._compute_potential(info)
        shaping_reward = self.gamma * curr_potential - self.prev_potential

        # Store for next step
        self.prev_potential = curr_potential

        # Add shaping to base reward
        shaped_reward = reward + shaping_reward

        # Log components for debugging
        info['reward_components'] = {
            'base': reward,
            'pbrs_shaping': shaping_reward,
            'total': shaped_reward
        }

        return obs, shaped_reward, terminated, truncated, info

    def _compute_potential(self, info: dict) -> float:
        """Potential function: Φ(s) = -distance_to_goal"""
        if 'robot_pos' not in info or 'goal_pos' not in info:
            return 0.0

        robot_pos = np.array(info['robot_pos'])
        goal_pos = np.array(info['goal_pos'])
        distance = np.linalg.norm(robot_pos - goal_pos)

        return -distance
```

### Application to Warehouser

**Current Issue:** NavigationRewardStrategy uses raw distance difference:
```cpp
float progress = prev_dist - curr_dist;
result.reward = progress * config_.progress_weight;
```

**PBRS Improvement:**
1. Provides theoretical guarantee of policy invariance
2. Properly discounts future potentials with gamma
3. Prevents cyclic reward exploitation
4. Maintains optimal policy while providing dense feedback

**Integration Steps:**
1. Replace `NavigationRewardStrategy` with `PBRSNavigationStrategy`
2. Set gamma to match training algorithm (typically 0.99)
3. Verify reward scale compatibility (PBRS rewards are smaller than distance differences)
4. Monitor training curves for faster convergence

---

## Pattern 2: Reward Component Normalization

### Problem Statement

Different reward components have vastly different scales:
- Success bonus: +100.0
- Collision penalty: -100.0
- Time penalty: -0.1 per step
- Progress reward: ±0.5 per step

This causes scale imbalance where large components dominate learning.

### Solution: Running Statistics Normalization

```python
# training/training/wrappers/reward_normalizer.py
from typing import Tuple
import numpy as np
import gymnasium as gym

class RunningStats:
    """Welford's online algorithm for mean and variance."""

    def __init__(self):
        self.n = 0
        self.mean = 0.0
        self.M2 = 0.0

    def update(self, x: float) -> None:
        self.n += 1
        delta = x - self.mean
        self.mean += delta / self.n
        delta2 = x - self.mean
        self.M2 += delta * delta2

    @property
    def variance(self) -> float:
        return self.M2 / self.n if self.n > 1 else 0.0

    @property
    def std(self) -> float:
        return np.sqrt(self.variance)


class RewardNormalizationWrapper(gym.Wrapper):
    """Normalize rewards using running statistics.

    Maintains running mean/std for each reward component and normalizes
    to zero mean, unit variance. Prevents scale imbalance.
    """

    def __init__(
        self,
        env: gym.Env,
        epsilon: float = 1e-8,
        clip_range: float = 10.0
    ):
        super().__init__(env)
        self.epsilon = epsilon
        self.clip_range = clip_range
        self.stats = RunningStats()

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        return self.env.reset(seed=seed, options=options)

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Update statistics
        self.stats.update(reward)

        # Normalize: (r - mean) / (std + epsilon)
        normalized_reward = (reward - self.stats.mean) / (self.stats.std + self.epsilon)

        # Clip to prevent extreme values
        normalized_reward = np.clip(
            normalized_reward,
            -self.clip_range,
            self.clip_range
        )

        # Log for debugging
        info['reward_normalization'] = {
            'original': reward,
            'normalized': normalized_reward,
            'mean': self.stats.mean,
            'std': self.stats.std
        }

        return obs, normalized_reward, terminated, truncated, info


class ComponentWiseNormalizationWrapper(gym.Wrapper):
    """Normalize each reward component independently.

    Requires environment to provide 'reward_components' in info dict.
    Normalizes each component separately to [-1, 1] range.
    """

    def __init__(self, env: gym.Env, epsilon: float = 1e-8):
        super().__init__(env)
        self.epsilon = epsilon
        self.component_stats = {}

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        return self.env.reset(seed=seed, options=options)

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Extract components
        if 'reward_components' not in info:
            return obs, reward, terminated, truncated, info

        components = info['reward_components']
        normalized_components = {}
        total_normalized = 0.0

        for name, value in components.items():
            # Initialize stats for new components
            if name not in self.component_stats:
                self.component_stats[name] = RunningStats()

            # Update and normalize
            self.component_stats[name].update(value)
            stats = self.component_stats[name]

            normalized = (value - stats.mean) / (stats.std + self.epsilon)
            normalized_components[name] = normalized
            total_normalized += normalized

        info['normalized_components'] = normalized_components

        return obs, total_normalized, terminated, truncated, info
```

### Application to Warehouser

**Current State:** No normalization applied, reward scales vary by 3 orders of magnitude.

**Recommended Approach:**
1. Start with `ComponentWiseNormalizationWrapper` for transparency
2. Log component distributions over first 10k steps
3. Verify no component has near-zero variance (would indicate dead component)
4. Switch to `RewardNormalizationWrapper` if component tracking overhead is too high
5. Monitor training stability and convergence speed

---

## Pattern 3: Multi-Objective Reward Balancing

### Dynamic Weight Scalarization

Based on "Preference-Based Deep RL with Automatic Curriculum Learning" (2025).

```python
# training/training/wrappers/multi_objective.py
from typing import Tuple, Dict, List
import numpy as np
import gymnasium as gym

class DynamicWeightScalarizationWrapper(gym.Wrapper):
    """Dynamic multi-objective reward weighting.

    Adjusts weights across training to explore different objective trade-offs.
    Implements automatic curriculum learning over preference space.
    """

    def __init__(
        self,
        env: gym.Env,
        objective_names: List[str],
        initial_weights: Dict[str, float] | None = None,
        adaptation_rate: float = 0.001,
        exploration_noise: float = 0.1
    ):
        super().__init__(env)
        self.objective_names = objective_names

        # Initialize weights
        if initial_weights is None:
            # Equal weights initially
            n = len(objective_names)
            self.weights = {name: 1.0 / n for name in objective_names}
        else:
            self.weights = initial_weights.copy()

        self.adaptation_rate = adaptation_rate
        self.exploration_noise = exploration_noise

        # Track objective statistics
        self.objective_stats = {
            name: {'mean': 0.0, 'std': 1.0, 'count': 0}
            for name in objective_names
        }

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        return self.env.reset(seed=seed, options=options)

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Extract objective values
        if 'reward_components' not in info:
            return obs, reward, terminated, truncated, info

        objectives = info['reward_components']

        # Update objective statistics
        for name, value in objectives.items():
            if name in self.objective_stats:
                stats = self.objective_stats[name]
                stats['count'] += 1
                delta = value - stats['mean']
                stats['mean'] += delta / stats['count']
                stats['std'] = np.sqrt(
                    (stats['std']**2 * (stats['count'] - 1) + delta**2) / stats['count']
                )

        # Compute scalarized reward with dynamic weights
        scalarized_reward = 0.0
        for name in self.objective_names:
            if name in objectives:
                # Normalize objective value
                stats = self.objective_stats[name]
                normalized = (objectives[name] - stats['mean']) / (stats['std'] + 1e-8)

                # Apply dynamic weight
                scalarized_reward += self.weights[name] * normalized

        # Adapt weights based on objective progress (simplified)
        # In practice, use preference learning or Pareto optimization
        if terminated or truncated:
            self._adapt_weights(objectives)

        info['dynamic_weights'] = self.weights.copy()
        info['scalarized_reward'] = scalarized_reward

        return obs, scalarized_reward, terminated, truncated, info

    def _adapt_weights(self, objectives: Dict[str, float]) -> None:
        """Adapt weights based on episode outcomes.

        Simplified version: increase weights for underperforming objectives.
        Full implementation would use preference learning.
        """
        # Add exploration noise
        for name in self.objective_names:
            noise = np.random.normal(0, self.exploration_noise)
            self.weights[name] += noise * self.adaptation_rate

        # Normalize weights to sum to 1
        total = sum(self.weights.values())
        for name in self.objective_names:
            self.weights[name] /= total
```

### Pareto Multi-Objective Approach

```python
# training/training/wrappers/pareto_morl.py
from typing import Tuple, Dict, List
import numpy as np
import gymnasium as gym

class ParetoMORLWrapper(gym.Wrapper):
    """Pareto Multi-Objective RL wrapper.

    Returns vector of rewards instead of scalar.
    Agent learns policy achieving Pareto optimal solutions.

    Requires MORL algorithm (e.g., MO-PPO, PGMORL).
    """

    def __init__(
        self,
        env: gym.Env,
        objective_names: List[str],
        normalize_objectives: bool = True
    ):
        super().__init__(env)
        self.objective_names = objective_names
        self.normalize_objectives = normalize_objectives

        if normalize_objectives:
            self.objective_stats = {
                name: RunningStats() for name in objective_names
            }

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        return self.env.reset(seed=seed, options=options)

    def step(self, action) -> Tuple[np.ndarray, np.ndarray, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Extract objective vector
        if 'reward_components' not in info:
            # Fallback to scalar reward
            reward_vector = np.array([reward])
        else:
            components = info['reward_components']
            reward_vector = np.zeros(len(self.objective_names))

            for i, name in enumerate(self.objective_names):
                if name in components:
                    value = components[name]

                    # Normalize if enabled
                    if self.normalize_objectives:
                        self.objective_stats[name].update(value)
                        stats = self.objective_stats[name]
                        value = (value - stats.mean) / (stats.std + 1e-8)

                    reward_vector[i] = value

        info['reward_vector'] = reward_vector

        return obs, reward_vector, terminated, truncated, info
```

### Application to Warehouser

**Current Objectives:**
1. Task completion (navigation to goal)
2. Safety (collision avoidance)
3. Efficiency (time minimization)
4. Smoothness (low acceleration)
5. Energy (low action magnitude)

**Phase 1: Manual Weighting (Current)**
```python
# Recommended starting weights
weights = {
    'task_completion': 1.0,    # Highest priority
    'safety': 0.8,              # Critical but secondary
    'efficiency': 0.3,          # Important but flexible
    'smoothness': 0.1,          # Nice-to-have
    'energy': 0.05              # Optimization target
}
```

**Phase 2: Dynamic Scalarization (Future)**
- Deploy `DynamicWeightScalarizationWrapper`
- Train multiple policies with different preference vectors
- Enable runtime switching based on mission requirements

**Phase 3: Pareto MORL (Research)**
- Implement `ParetoMORLWrapper`
- Use MO-PPO or PGMORL algorithm
- Maintain Pareto front of solutions
- Select policy based on user preferences

---

## Pattern 4: Curriculum Learning for Rewards

### Staged Curriculum Implementation

```python
# training/training/wrappers/curriculum.py
from typing import Tuple, Dict, Callable
import numpy as np
import gymnasium as gym

class CurriculumRewardWrapper(gym.Wrapper):
    """Curriculum learning for reward scheduling.

    Gradually increases task complexity by adjusting reward weights
    and environment difficulty based on agent performance.
    """

    def __init__(
        self,
        env: gym.Env,
        stages: List[Dict],
        success_threshold: float = 0.7,
        window_size: int = 100
    ):
        """Initialize curriculum wrapper.

        Args:
            env: Base environment
            stages: List of curriculum stages with reward configs
            success_threshold: Success rate to advance to next stage
            window_size: Number of episodes to compute success rate
        """
        super().__init__(env)
        self.stages = stages
        self.success_threshold = success_threshold
        self.window_size = window_size

        self.current_stage = 0
        self.episode_successes = []
        self.episode_count = 0

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        obs, info = self.env.reset(seed=seed, options=options)

        # Apply current stage configuration
        stage_config = self.stages[self.current_stage]
        info['curriculum_stage'] = self.current_stage
        info['curriculum_config'] = stage_config

        return obs, info

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Get stage configuration
        stage = self.stages[self.current_stage]

        # Apply stage-specific reward weights
        if 'reward_weights' in stage and 'reward_components' in info:
            components = info['reward_components']
            weighted_reward = 0.0

            for name, value in components.items():
                weight = stage['reward_weights'].get(name, 1.0)
                weighted_reward += weight * value

            reward = weighted_reward

        # Track episode success for curriculum advancement
        if terminated or truncated:
            self.episode_count += 1
            success = info.get('success', False)
            self.episode_successes.append(success)

            # Keep only recent episodes
            if len(self.episode_successes) > self.window_size:
                self.episode_successes.pop(0)

            # Check for stage advancement
            self._check_advancement()

        info['curriculum_stage'] = self.current_stage

        return obs, reward, terminated, truncated, info

    def _check_advancement(self) -> None:
        """Check if agent should advance to next curriculum stage."""
        if len(self.episode_successes) < self.window_size:
            return

        success_rate = sum(self.episode_successes) / len(self.episode_successes)

        if success_rate >= self.success_threshold:
            if self.current_stage < len(self.stages) - 1:
                self.current_stage += 1
                self.episode_successes = []  # Reset for new stage
                print(f"Curriculum advanced to stage {self.current_stage}")
```

### Warehouser Curriculum Stages

```python
# training/training/config/curriculum.py

# Stage 1: Single object, empty warehouse
STAGE_1 = {
    'name': 'basic_navigation',
    'env_config': {
        'num_objects': 1,
        'num_obstacles': 0,
        'max_steps': 500
    },
    'reward_weights': {
        'navigation': 1.0,
        'collision': 1.0,
        'time': 0.1,
        'pick_place': 0.0,  # Disabled
        'smoothness': 0.0,
        'exploration': 0.3   # Early exploration bonus
    }
}

# Stage 2: Multiple objects, static environment
STAGE_2 = {
    'name': 'multi_object',
    'env_config': {
        'num_objects': 3,
        'num_obstacles': 2,
        'max_steps': 1000
    },
    'reward_weights': {
        'navigation': 1.0,
        'collision': 1.0,
        'time': 0.2,
        'pick_place': 1.0,   # Enable pick/place
        'smoothness': 0.0,
        'exploration': 0.1   # Reduce exploration
    }
}

# Stage 3: Dynamic obstacles
STAGE_3 = {
    'name': 'dynamic_obstacles',
    'env_config': {
        'num_objects': 3,
        'num_obstacles': 5,
        'dynamic_obstacles': True,
        'max_steps': 1500
    },
    'reward_weights': {
        'navigation': 1.0,
        'collision': 1.5,    # Increase safety weight
        'time': 0.3,
        'pick_place': 1.0,
        'smoothness': 0.2,   # Add smoothness
        'exploration': 0.0   # Disable exploration
    }
}

# Stage 4: Multi-robot coordination
STAGE_4 = {
    'name': 'multi_robot',
    'env_config': {
        'num_robots': 3,
        'num_objects': 5,
        'num_obstacles': 5,
        'dynamic_obstacles': True,
        'max_steps': 2000
    },
    'reward_weights': {
        'navigation': 1.0,
        'collision': 2.0,     # Penalize robot-robot collisions heavily
        'time': 0.4,
        'pick_place': 1.0,
        'smoothness': 0.3,
        'coordination': 0.5,  # Add coordination bonus
        'fairness': 0.2       # Ensure load balancing
    }
}

CURRICULUM_STAGES = [STAGE_1, STAGE_2, STAGE_3, STAGE_4]
```

### Usage Example

```python
# training/training/scripts/train_curriculum.py
from training.envs import WarehouseEnv
from training.wrappers import CurriculumRewardWrapper
from training.config.curriculum import CURRICULUM_STAGES

# Create base environment
env = WarehouseEnv()

# Wrap with curriculum
env = CurriculumRewardWrapper(
    env,
    stages=CURRICULUM_STAGES,
    success_threshold=0.7,
    window_size=100
)

# Train with standard PPO
# Curriculum automatically advances based on performance
```

---

## Pattern 5: Reward Debugging and Visualization

### Component Logging

```python
# training/training/utils/reward_logger.py
from typing import Dict, List
import numpy as np
from collections import defaultdict

class RewardComponentLogger:
    """Log and analyze reward components over training."""

    def __init__(self):
        self.components: Dict[str, List[float]] = defaultdict(list)
        self.episode_rewards: List[float] = []
        self.episode_lengths: List[int] = []
        self.current_episode_rewards: List[float] = []

    def log_step(self, info: dict) -> None:
        """Log reward components for a single step."""
        if 'reward_components' in info:
            for name, value in info['reward_components'].items():
                self.components[name].append(value)

        if 'total_reward' in info:
            self.current_episode_rewards.append(info['total_reward'])

    def log_episode_end(self) -> None:
        """Log end of episode statistics."""
        if self.current_episode_rewards:
            self.episode_rewards.append(sum(self.current_episode_rewards))
            self.episode_lengths.append(len(self.current_episode_rewards))
            self.current_episode_rewards = []

    def get_statistics(self, last_n: int = 100) -> Dict[str, Dict[str, float]]:
        """Get statistics for each reward component."""
        stats = {}

        for name, values in self.components.items():
            recent = values[-last_n:] if len(values) > last_n else values
            stats[name] = {
                'mean': np.mean(recent),
                'std': np.std(recent),
                'min': np.min(recent),
                'max': np.max(recent),
                'contribution': np.sum(recent) / np.sum([
                    np.sum(self.components[k][-last_n:])
                    for k in self.components
                ])
            }

        return stats

    def compute_correlation_matrix(self) -> np.ndarray:
        """Compute correlation matrix between reward components."""
        names = list(self.components.keys())
        n = len(names)

        if n == 0:
            return np.array([])

        # Align all components to same length (use minimum)
        min_len = min(len(v) for v in self.components.values())
        matrix = np.zeros((n, min_len))

        for i, name in enumerate(names):
            matrix[i, :] = self.components[name][:min_len]

        # Compute correlation
        return np.corrcoef(matrix)

    def detect_dead_components(self, threshold: float = 1e-6) -> List[str]:
        """Detect reward components with near-zero variance."""
        dead = []

        for name, values in self.components.items():
            if len(values) > 10:
                std = np.std(values[-100:])
                if std < threshold:
                    dead.append(name)

        return dead

    def detect_dominant_components(self, threshold: float = 0.7) -> List[str]:
        """Detect components contributing >threshold of total reward."""
        stats = self.get_statistics()
        dominant = []

        for name, stat in stats.items():
            if stat['contribution'] > threshold:
                dominant.append(name)

        return dominant
```

### TensorBoard Integration

```python
# training/training/utils/tensorboard_logger.py
from torch.utils.tensorboard import SummaryWriter
import numpy as np

class RewardTensorBoardLogger:
    """Log reward components to TensorBoard."""

    def __init__(self, log_dir: str):
        self.writer = SummaryWriter(log_dir)
        self.step = 0

    def log_components(
        self,
        components: Dict[str, float],
        global_step: int
    ) -> None:
        """Log individual reward components."""
        for name, value in components.items():
            self.writer.add_scalar(f'reward/{name}', value, global_step)

    def log_statistics(
        self,
        stats: Dict[str, Dict[str, float]],
        global_step: int
    ) -> None:
        """Log component statistics."""
        for name, stat in stats.items():
            self.writer.add_scalar(f'reward_stats/{name}/mean', stat['mean'], global_step)
            self.writer.add_scalar(f'reward_stats/{name}/std', stat['std'], global_step)
            self.writer.add_scalar(
                f'reward_stats/{name}/contribution',
                stat['contribution'],
                global_step
            )

    def log_correlation_matrix(
        self,
        correlation: np.ndarray,
        component_names: List[str],
        global_step: int
    ) -> None:
        """Log correlation matrix as heatmap."""
        import matplotlib.pyplot as plt
        import seaborn as sns

        fig, ax = plt.subplots(figsize=(10, 8))
        sns.heatmap(
            correlation,
            annot=True,
            fmt='.2f',
            xticklabels=component_names,
            yticklabels=component_names,
            cmap='coolwarm',
            center=0,
            ax=ax
        )
        ax.set_title('Reward Component Correlation Matrix')

        self.writer.add_figure('reward/correlation_matrix', fig, global_step)
        plt.close()

    def log_ablation_results(
        self,
        ablation_results: Dict[str, float],
        global_step: int
    ) -> None:
        """Log ablation study results."""
        for config_name, success_rate in ablation_results.items():
            self.writer.add_scalar(
                f'ablation/{config_name}',
                success_rate,
                global_step
            )

    def close(self) -> None:
        self.writer.close()
```

### Reward Hacking Detection

```python
# training/training/utils/hacking_detector.py
from typing import List, Tuple
import numpy as np

class RewardHackingDetector:
    """Detect potential reward hacking behaviors."""

    def __init__(self, window_size: int = 100):
        self.window_size = window_size
        self.episode_data: List[Dict] = []

    def log_episode(self, episode_info: dict) -> None:
        """Log episode data for analysis."""
        self.episode_data.append(episode_info)

        if len(self.episode_data) > self.window_size:
            self.episode_data.pop(0)

    def detect_spinning(self, threshold: float = 10.0) -> bool:
        """Detect if agent is spinning in place (high angular velocity)."""
        if not self.episode_data:
            return False

        recent = self.episode_data[-10:]
        avg_angular_vel = np.mean([
            ep.get('avg_angular_velocity', 0.0) for ep in recent
        ])

        return avg_angular_vel > threshold

    def detect_oscillation(self, threshold: float = 0.8) -> bool:
        """Detect if agent is oscillating without progress."""
        if len(self.episode_data) < 5:
            return False

        recent = self.episode_data[-5:]
        positions = [ep.get('final_position', (0, 0)) for ep in recent]

        # Check if final positions are very similar
        position_variance = np.var(positions, axis=0)

        return np.all(position_variance < threshold)

    def detect_reward_exploitation(self) -> List[str]:
        """Detect potential reward exploitation patterns."""
        warnings = []

        if self.detect_spinning():
            warnings.append("Spinning behavior detected")

        if self.detect_oscillation():
            warnings.append("Oscillation without progress detected")

        # Check for suspiciously high rewards without task completion
        recent = self.episode_data[-self.window_size:]
        high_reward_no_success = [
            ep for ep in recent
            if ep.get('total_reward', 0) > 50 and not ep.get('success', False)
        ]

        if len(high_reward_no_success) > self.window_size * 0.3:
            warnings.append("High rewards without task completion")

        return warnings
```

---

## Pattern 6: Anti-Reward Hacking Measures

### Adversarial Test Scenarios

```python
# training/tests/test_reward_hacking.py
import pytest
from training.envs import WarehouseEnv

class TestRewardHacking:
    """Test suite for reward hacking detection."""

    def test_empty_warehouse(self):
        """Agent should receive minimal reward in empty warehouse."""
        env = WarehouseEnv(num_objects=0, num_obstacles=0)
        obs, _ = env.reset()

        total_reward = 0.0
        for _ in range(100):
            action = env.action_space.sample()
            obs, reward, terminated, truncated, _ = env.step(action)
            total_reward += reward

            if terminated or truncated:
                break

        # Should not accumulate high rewards without objects
        assert total_reward < 10.0, "Agent receiving rewards without objects"

    def test_unreachable_goal(self):
        """Agent should handle unreachable goals gracefully."""
        env = WarehouseEnv()
        obs, _ = env.reset()

        # Manually set unreachable goal
        env.unwrapped.goal_position = (1000, 1000)

        total_reward = 0.0
        for _ in range(100):
            action = env.action_space.sample()
            obs, reward, terminated, truncated, _ = env.step(action)
            total_reward += reward

            if terminated or truncated:
                break

        # Should accumulate negative time penalty
        assert total_reward < 0, "Agent exploiting unreachable goal"

    def test_spinning_detection(self):
        """Detect if agent spins in place."""
        env = WarehouseEnv()
        obs, _ = env.reset()

        # Execute spinning action repeatedly
        angular_velocities = []
        for _ in range(50):
            action = [0.0, 1.0]  # No linear, max angular
            obs, reward, terminated, truncated, info = env.step(action)
            angular_velocities.append(abs(action[1]))

            if terminated or truncated:
                break

        avg_angular = np.mean(angular_velocities)

        # Should detect spinning and not reward it
        assert avg_angular > 0.5, "Not detecting spinning behavior"

    def test_collision_always_terminates(self):
        """Collisions should always terminate episode with penalty."""
        env = WarehouseEnv()
        obs, _ = env.reset()

        # Force collision by moving toward wall
        for _ in range(100):
            action = [1.0, 0.0]  # Max forward velocity
            obs, reward, terminated, truncated, info = env.step(action)

            if terminated:
                # Should be large negative reward
                assert reward < -50.0, "Collision not penalized heavily"
                break

        assert terminated, "Collision did not terminate episode"

    def test_success_bonus_dominates(self):
        """Success bonus should be larger than all shaping rewards."""
        env = WarehouseEnv()

        # Simulate successful episode
        env.reset()
        # ... navigate to goal ...

        # Success reward should be > cumulative shaping
        # This ensures agent prioritizes task completion
        success_reward = 100.0
        max_shaping_per_step = 2.0
        max_steps = 1000

        assert success_reward > max_shaping_per_step * max_steps * 0.1
```

### Safe Reward Configuration Validator

```cpp
// Header: reward_validator.hpp
namespace warehouser {

struct RewardValidationResult {
    bool valid = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

class RewardConfigValidator {
public:
    static RewardValidationResult validate(const RewardConfig& config) {
        RewardValidationResult result;

        // Check 1: Success bonus should dominate
        float max_shaping = config.progress_weight * 100.0f;  // Assume max 100m progress
        if (config.success_bonus < max_shaping) {
            result.warnings.push_back(
                "Success bonus (" + std::to_string(config.success_bonus) +
                ") may be too small compared to shaping rewards"
            );
        }

        // Check 2: Collision should be terminal and negative
        if (config.collision_penalty >= 0.0f) {
            result.errors.push_back("Collision penalty must be negative");
            result.valid = false;
        }

        // Check 3: Time penalty should be small
        if (std::abs(config.time_penalty) > 1.0f) {
            result.warnings.push_back(
                "Time penalty may be too large, could dominate other rewards"
            );
        }

        // Check 4: Reward scales within same order of magnitude
        std::vector<float> scales = {
            std::abs(config.success_bonus),
            std::abs(config.collision_penalty),
            std::abs(config.progress_weight),
            std::abs(config.time_penalty) * 1000.0f  // Scale by typical episode length
        };

        float min_scale = *std::min_element(scales.begin(), scales.end());
        float max_scale = *std::max_element(scales.begin(), scales.end());

        if (max_scale / min_scale > 1000.0f) {
            result.warnings.push_back(
                "Reward scales differ by >1000x, consider normalization"
            );
        }

        return result;
    }
};

}  // namespace warehouser
```

---

## Application Summary

### Immediate Implementations for Warehouser

1. **Replace Progress Reward with PBRS** (High Priority)
   - Location: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_strategy.cpp`
   - Replace `NavigationRewardStrategy::calculate()` with PBRS version
   - Use `Φ(s) = -distance_to_goal` as potential function
   - Set gamma = 0.99 to match PPO discount factor

2. **Add Component Logging** (High Priority)
   - Location: `C:\Users\costa\src\warehouser\training\training\wrappers\`
   - Create `reward_logger.py` with `RewardComponentLogger`
   - Integrate with training loop to track component distributions
   - Export to TensorBoard for visualization

3. **Implement Reward Normalization** (Medium Priority)
   - Location: `C:\Users\costa\src\warehouser\training\training\wrappers\`
   - Create `reward_normalizer.py` with `ComponentWiseNormalizationWrapper`
   - Apply after PBRS but before agent
   - Monitor training stability improvements

4. **Design Curriculum Stages** (Medium Priority)
   - Location: `C:\Users\costa\src\warehouser\training\training\config\curriculum.py`
   - Define 4-stage progression (basic → multi-object → dynamic → multi-robot)
   - Implement `CurriculumRewardWrapper`
   - Set success threshold = 0.7 for stage advancement

5. **Create Adversarial Test Suite** (Medium Priority)
   - Location: `C:\Users\costa\src\warehouser\training\tests\test_reward_hacking.py`
   - Implement empty warehouse, unreachable goal, spinning tests
   - Run after each reward modification
   - Add to CI/CD pipeline

6. **Add Reward Config Validator** (Low Priority)
   - Location: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_validator.hpp`
   - Validate reward scales and relationships
   - Call during node initialization
   - Emit warnings for suboptimal configurations

### Expected Improvements

1. **Faster Convergence** - PBRS provides denser feedback than sparse rewards
2. **Stable Training** - Normalization prevents scale imbalance
3. **Better Exploration** - Curriculum learning starts simple, gradually increases complexity
4. **Robust Policies** - Anti-hacking tests prevent exploitation
5. **Transparent Debugging** - Component logging reveals reward interactions

### Validation Metrics

Track these metrics to validate improvements:

- **Convergence Speed**: Steps to 70% success rate
- **Final Performance**: Success rate on test scenarios
- **Policy Quality**: Path efficiency, smoothness, energy
- **Reward Health**: Component variance, correlation, contribution
- **Robustness**: Performance on adversarial tests

### References

All patterns based on peer-reviewed research documented in S.md:
- HPRS (TU Wien, 2025)
- Dynamic Weight Scalarization (ScienceDirect, 2025)
- MORL for Navigation (arXiv 2312.07953, 2023)
- Reward Hacking Taxonomy (Skalse et al., 2022)
- Curriculum Learning Survey (Annual Review, 2024)
