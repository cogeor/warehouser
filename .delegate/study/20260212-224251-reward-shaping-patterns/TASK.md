# TASK: Implement Research-Backed Reward Engineering System

Created: 2026-02-12
Build: FAIL (pydantic dependency missing - pre-existing)
Tests: 24 passed, 4 errors (import errors - pre-existing)

## Summary

Migrate the warehouser reward system from ad-hoc manual weighting to a research-backed architecture using Potential-Based Reward Shaping (PBRS), component normalization, curriculum learning, and anti-hacking safeguards. The current system has solid modular design but exhibits critical reward engineering pitfalls: 1000x scale imbalances between components, reward hacking vulnerabilities in pickup/place logic, hardcoded weights preventing runtime tuning, and missing cooperation incentives for multi-robot scenarios.

## Context

### [S] Search Findings: State-of-the-Art Reward Shaping (2024-2026)

1. **Potential-Based Reward Shaping (PBRS)** - Mathematical gold standard:
   - Formula: F(s,a,s') = γΦ(s') - Φ(s) where Φ is potential function
   - Guarantees policy invariance while densifying sparse rewards
   - HPRS (TU Wien, 2025) achieved successful sim-to-real transfer on F1TENTH vehicles
   - VBRS (Electronics Journal, 2026) reformulated as equivalent initialization of action values
   - Setting Φ = -distance_to_goal provides proven navigation shaping

2. **Reward Hacking** - Classic pitfalls and prevention:
   - Racing loop exploit: Agent discovered it could loop endlessly hitting checkpoints without finishing race
   - Fake grasping (OpenAI 2017): Robot positioned manipulator between camera and object without actually grasping
   - Oscillating robot: Goes back and forth on initial straight portion to maximize reward
   - Mitigation: PBRS theoretical guarantees, adversarial testing, multi-objective balancing, explicit path closure

3. **Multi-Objective Reward Balancing** - Modern approaches:
   - Dynamic Weight Scalarization (2025): Assigns dynamic weights across preference space, enables real-time adaptation
   - MORL for Navigation (arXiv 2312.07953): Vector rewards with Pareto optimization, outperforms single-objective
   - Adaptive MORL with demonstrations: No retraining required for preference shifts
   - Constrained MORL: Balances conflicting objectives while ensuring safety constraints

4. **Curriculum Learning** - Progressive complexity:
   - Provides strong generalization via progressive learning (Review, 2024)
   - Automatic curriculum with success rate thresholds (>70% → advance stage)
   - Facilitates sim-to-real transfer
   - Curtails convergence times significantly

### [I] Introspection Findings: Current System Analysis

**Strengths:**
- Clean modular architecture (Strategy + Composite patterns)
- Individual components well-tested (378 lines, 34 tests in reward_strategy tests)
- Multi-robot support at framework level
- Stateful exploration tracking with OccupancyTracker

**Critical Issues:**

1. **Scale Imbalances (1000x magnitude differences):**
   - Success bonus: +100.0
   - Collision penalty: -100.0
   - Navigation progress: ±0.5 per step
   - Time penalty: -0.1 per step
   - Result: Sparse rewards dominate dense rewards, agent learns to avoid -100 collision rather than achieve +100 success

2. **Reward Hacking Vulnerabilities:**
   - Pickup/Place: Rewards ANY pickup/place transition without verifying correct object or location (could repeatedly pick/drop same object for +100 per cycle)
   - Exploration grinding: +1.0 per new cell, up to 400 cells = 400 reward points (could ignore goal and maximize coverage)
   - Time penalty avoidance: -0.1 magnitude easily dominated by other rewards (1000 steps = -100 = 1 collision penalty)
   - Goal distance manipulation: Based on distance delta not absolute distance

3. **Hardcoded Configuration:**
   - Composite weights in C++ factory functions (cannot tune without rebuild)
   - YAML only affects individual strategy configs, not composite weights
   - No documented rationale for weight choices
   - Python Pydantic config exists but disconnected from ROS parameters

4. **Missing Dense Signals:**
   - No orientation alignment toward goal
   - No smoothness penalty for jerky movements
   - No energy efficiency penalty for high velocities
   - No obstacle proximity shaping (binary collision only)
   - No task completion verification (pick + place + correct location)

5. **Multi-Robot Competition (Not Cooperation):**
   - Individual rewards per robot → competitive behavior
   - Single goal for all robots (no task allocation)
   - Collision between robots not penalized
   - Shared reward only available in Python wrapper, not C++ implementation

### [T] Template Findings: Reference Implementations

**PBRS Navigation Strategy (C++):**
```cpp
// Potential function: Φ(s) = -distance_to_goal
float potential(const Entity& robot, const Goal& goal) const {
    return -std::sqrt((goal.x - robot.x)² + (goal.y - robot.y)²);
}

// PBRS: F(s,a,s') = γΦ(s') - Φ(s)
result.reward = config_.gamma * phi_curr - phi_prev;
```

**Reward Normalization (Python):**
```python
# Running statistics for component normalization
normalized_reward = (reward - stats.mean) / (stats.std + epsilon)
normalized_reward = np.clip(normalized_reward, -10.0, 10.0)
```

**Curriculum Stages:**
- Stage 1: Single object, empty warehouse (exploration-heavy weights)
- Stage 2: Multiple objects, static environment (enable pick/place)
- Stage 3: Dynamic obstacles (increase safety weight)
- Stage 4: Multi-robot coordination (add coordination/fairness)

**Anti-Hacking Tests:**
- Empty warehouse (ensure no false positive rewards)
- Unreachable objects (ensure graceful failure)
- Dense obstacle fields (ensure no spinning/oscillating)
- Collision always terminates with large penalty

## Objective

Transform warehouser reward system into a production-grade, research-backed architecture that:
1. Eliminates reward hacking vulnerabilities through PBRS theoretical guarantees
2. Balances multi-objective trade-offs through normalization and dynamic weighting
3. Accelerates learning through curriculum progression
4. Provides transparency through comprehensive component logging
5. Ensures robustness through adversarial testing

Success Criteria:
- Agent learns navigation + pick/place within 100k timesteps (50% reduction from baseline)
- No reward hacking observed in adversarial test scenarios
- All reward components contribute meaningfully (no dead/dominant components)
- Policy transfers to test scenarios with >70% success rate
- Motion quality metrics improve: path efficiency >0.8, smoothness variance <2.0

## Scope

### Phase 1: Normalization and PBRS Migration

**C++ Changes:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_strategy.hpp`
  - Add `PBRSNavigationStrategy` class with potential function
  - Add `RewardValidationResult` struct for config validation
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_strategy.cpp`
  - Implement `PBRSNavigationStrategy::calculate()` with gamma discounting
  - Replace `NavigationRewardStrategy` usage with PBRS version
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_validator.hpp` (new)
  - Create `RewardConfigValidator::validate()` to check scale balance
  - Validate success bonus dominates shaping, collision is negative/terminal

**Python Changes:**
- `C:\Users\costa\src\warehouser\training\training\wrappers\pbrs_wrapper.py` (new)
  - Implement `PBRSWrapper` for Python-side PBRS application
  - Track prev_potential across steps, apply F(s,a,s') = γΦ(s') - Φ(s)
- `C:\Users\costa\src\warehouser\training\training\wrappers\reward_normalizer.py` (new)
  - Implement `RunningStats` with Welford's online algorithm
  - Create `ComponentWiseNormalizationWrapper` to normalize each component independently
  - Clip to [-10, +10] range to prevent extreme values

### Phase 2: Curriculum Learning Framework

**Python Changes:**
- `C:\Users\costa\src\warehouser\training\training\wrappers\curriculum.py` (new)
  - Implement `CurriculumRewardWrapper` with stage progression
  - Track success rate over window_size episodes
  - Advance when success_rate >= threshold (default 0.7)
- `C:\Users\costa\src\warehouser\training\training\config\curriculum.py` (new)
  - Define `STAGE_1`: Single object, empty (exploration 0.3, pick/place 0.0)
  - Define `STAGE_2`: Multi-object, static (exploration 0.1, pick/place 1.0)
  - Define `STAGE_3`: Dynamic obstacles (collision 1.5, smoothness 0.2)
  - Define `STAGE_4`: Multi-robot (coordination 0.5, fairness 0.2)

### Phase 3: Reward Debugging Infrastructure

**Python Changes:**
- `C:\Users\costa\src\warehouser\training\training\utils\reward_logger.py` (new)
  - Implement `RewardComponentLogger` to track component distributions
  - Compute correlation matrix between components
  - Detect dead components (variance < 1e-6) and dominant components (contribution > 70%)
- `C:\Users\costa\src\warehouser\training\training\utils\tensorboard_logger.py` (new)
  - Create `RewardTensorBoardLogger` for component visualization
  - Log individual components, statistics, correlation matrices
  - Support ablation study result tracking
- `C:\Users\costa\src\warehouser\training\training\utils\hacking_detector.py` (new)
  - Implement `RewardHackingDetector` to monitor for exploits
  - Detect spinning (high angular velocity), oscillation (low position variance)
  - Alert on high rewards without task completion

### Phase 4: Anti-Reward Hacking Safeguards

**C++ Changes:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_strategy.cpp`
  - Modify `PickPlaceRewardStrategy::calculate()` to verify correct object and placement location
  - Add goal_id check: only reward pickup/place of task-relevant objects
  - Add placement verification: only reward place at designated drop zone

**Python Changes:**
- `C:\Users\costa\src\warehouser\training\tests\test_reward_hacking.py` (new)
  - Test `test_empty_warehouse()`: Total reward < 10.0 without objects
  - Test `test_unreachable_goal()`: Accumulate negative time penalty
  - Test `test_spinning_detection()`: Detect high angular velocity
  - Test `test_collision_always_terminates()`: Reward < -50.0 on collision
  - Test `test_success_bonus_dominates()`: Success > max_shaping * max_steps * 0.1

**Configuration Changes:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\params\rl_bridge_params.yaml`
  - Add `pbrs_gamma: 0.99` parameter
  - Add `reward_weights` section with exposed composite weights
  - Add `enable_reward_validation: true` flag

## Implementation Plan

### Phase 1: PBRS and Normalization (Weeks 1-2)

**Step 1.1: C++ PBRS Implementation**
- [ ] Create `PBRSNavigationStrategy` class in reward_strategy.hpp
- [ ] Implement potential function: Φ(s) = -||robot_pos - goal_pos||
- [ ] Implement calculate() with gamma discounting
- [ ] Add unit tests in test_reward_strategy.cpp
- [ ] Update factory function to use PBRS instead of raw progress

**Step 1.2: Reward Config Validator**
- [ ] Create reward_validator.hpp with validation result struct
- [ ] Implement scale balance checks (max/min < 1000x)
- [ ] Verify success bonus dominates shaping rewards
- [ ] Call validator in rl_bridge_node initialization
- [ ] Log warnings to ROS console

**Step 1.3: Python Normalization Wrappers**
- [ ] Implement RunningStats class with Welford's algorithm
- [ ] Create ComponentWiseNormalizationWrapper
- [ ] Add unit tests in tests/test_wrappers.py
- [ ] Integrate into training loop after PBRS wrapper
- [ ] Monitor component distributions in TensorBoard

**Step 1.4: Baseline Comparison**
- [ ] Train baseline with old NavigationRewardStrategy (10k steps)
- [ ] Train with PBRS only (10k steps)
- [ ] Train with PBRS + normalization (10k steps)
- [ ] Compare convergence speed and final performance
- [ ] Document improvement metrics

### Phase 2: Curriculum Learning (Weeks 3-4)

**Step 2.1: Curriculum Wrapper**
- [ ] Implement CurriculumRewardWrapper with stage tracking
- [ ] Add success rate computation over sliding window
- [ ] Implement automatic stage advancement logic
- [ ] Log stage transitions to console and TensorBoard

**Step 2.2: Stage Definitions**
- [ ] Define STAGE_1 config (single object, exploration-heavy)
- [ ] Define STAGE_2 config (multi-object, enable pick/place)
- [ ] Define STAGE_3 config (dynamic obstacles, safety focus)
- [ ] Define STAGE_4 config (multi-robot, coordination)
- [ ] Test each stage individually for sanity

**Step 2.3: Curriculum Training**
- [ ] Train full curriculum from stage 1 to 4
- [ ] Monitor success rates and stage transitions
- [ ] Compare final policy against non-curriculum baseline
- [ ] Verify policy generalizes to test scenarios

### Phase 3: Debugging Infrastructure (Week 5)

**Step 3.1: Component Logging**
- [ ] Implement RewardComponentLogger class
- [ ] Add correlation matrix computation
- [ ] Add dead/dominant component detection
- [ ] Integrate into training loop

**Step 3.2: TensorBoard Integration**
- [ ] Implement RewardTensorBoardLogger
- [ ] Log individual components as scalars
- [ ] Visualize correlation matrices as heatmaps
- [ ] Create dashboard template

**Step 3.3: Hacking Detection**
- [ ] Implement RewardHackingDetector
- [ ] Add spinning and oscillation detection
- [ ] Monitor high-reward-no-success episodes
- [ ] Emit warnings during training

### Phase 4: Anti-Hacking Safeguards (Week 6)

**Step 4.1: Pickup/Place Verification**
- [ ] Modify PickPlaceRewardStrategy to check goal_id match
- [ ] Add placement location verification (within drop zone)
- [ ] Update tests to verify correct object/location required
- [ ] Test against exploit scenarios

**Step 4.2: Adversarial Test Suite**
- [ ] Implement test_empty_warehouse()
- [ ] Implement test_unreachable_goal()
- [ ] Implement test_spinning_detection()
- [ ] Implement test_collision_always_terminates()
- [ ] Implement test_success_bonus_dominates()
- [ ] Add to CI/CD pipeline

**Step 4.3: Final Validation**
- [ ] Run all adversarial tests on trained policy
- [ ] Verify no reward hacking detected
- [ ] Review TensorBoard logs for component health
- [ ] Test on held-out scenarios not in training

## Interface Definitions

### C++ PBRS Navigation Strategy

```cpp
// File: C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_strategy.hpp

namespace warehouser {

struct PBRSConfig {
    float gamma = 0.99f;            // Discount factor (match PPO gamma)
    float success_bonus = 100.0f;   // Terminal success reward
    float goal_threshold = 0.5f;    // Distance to consider goal reached (meters)
};

/// Potential-Based Reward Shaping for navigation
/// Uses Φ(s) = -distance_to_goal as potential function
/// Guarantees policy invariance: F(s,a,s') = γΦ(s') - Φ(s)
class PBRSNavigationStrategy : public IRewardStrategy {
public:
    explicit PBRSNavigationStrategy(const PBRSConfig& config = {});

    RewardResult calculate(const RewardContext& ctx) const override;

    std::string name() const override { return "pbrs_navigation"; }

private:
    PBRSConfig config_;

    /// Potential function: Φ(s) = -||robot_pos - goal_pos||
    /// Returns negative distance to ensure Φ increases as robot approaches goal
    float potential(
        const warehouser_msgs::msg::Entity& robot,
        const warehouser_msgs::msg::Goal& goal
    ) const;

    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world,
        size_t index
    ) const;
};

} // namespace warehouser
```

### C++ Reward Config Validator

```cpp
// File: C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_validator.hpp

namespace warehouser {

struct RewardValidationResult {
    bool valid = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    void addWarning(const std::string& msg) { warnings.push_back(msg); }
    void addError(const std::string& msg) { errors.push_back(msg); valid = false; }

    bool hasIssues() const { return !warnings.empty() || !errors.empty(); }
};

/// Validates reward configuration for common pitfalls
class RewardConfigValidator {
public:
    /// Validate reward configuration
    /// Checks:
    ///   - Success bonus dominates shaping rewards
    ///   - Collision penalty is negative and large
    ///   - Time penalty scale is reasonable
    ///   - Reward components within 1-2 orders of magnitude
    static RewardValidationResult validate(const RewardConfig& config);

private:
    static constexpr float MAX_SCALE_RATIO = 1000.0f;
    static constexpr float MIN_SUCCESS_SHAPING_RATIO = 0.5f;
};

} // namespace warehouser
```

### Python PBRS Wrapper

```python
# File: C:\Users\costa\src\warehouser\training\training\wrappers\pbrs_wrapper.py

from typing import Tuple
import numpy as np
import gymnasium as gym

class PBRSWrapper(gym.Wrapper):
    """Potential-Based Reward Shaping wrapper.

    Applies PBRS using distance-to-goal as potential function.
    Guarantees policy invariance while densifying sparse rewards.

    Formula: F(s,a,s') = γΦ(s') - Φ(s) where Φ(s) = -distance_to_goal
    """

    def __init__(self, env: gym.Env, gamma: float = 0.99):
        """Initialize PBRS wrapper.

        Args:
            env: Base environment
            gamma: Discount factor (should match training algorithm)
        """
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
        info['reward_components'] = info.get('reward_components', {})
        info['reward_components']['pbrs_shaping'] = shaping_reward
        info['reward_components']['base'] = reward
        info['reward_components']['total'] = shaped_reward

        return obs, shaped_reward, terminated, truncated, info

    def _compute_potential(self, info: dict) -> float:
        """Potential function: Φ(s) = -distance_to_goal"""
        if 'robot_pos' not in info or 'goal_pos' not in info:
            return 0.0

        robot_pos = np.array(info['robot_pos'][:2])  # Extract x, y
        goal_pos = np.array(info['goal_pos'][:2])
        distance = np.linalg.norm(robot_pos - goal_pos)

        return -distance
```

### Python Component Normalization Wrapper

```python
# File: C:\Users\costa\src\warehouser\training\training\wrappers\reward_normalizer.py

from typing import Tuple, Dict
import numpy as np
import gymnasium as gym

class RunningStats:
    """Welford's online algorithm for mean and variance.

    Computes running statistics without storing all samples.
    Numerically stable for large datasets.
    """

    def __init__(self):
        self.n = 0
        self.mean = 0.0
        self.M2 = 0.0

    def update(self, x: float) -> None:
        """Update statistics with new sample."""
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


class ComponentWiseNormalizationWrapper(gym.Wrapper):
    """Normalize each reward component independently.

    Requires environment to provide 'reward_components' in info dict.
    Normalizes each component to zero mean, unit variance.
    Prevents scale imbalance between components.
    """

    def __init__(
        self,
        env: gym.Env,
        epsilon: float = 1e-8,
        clip_range: float = 10.0
    ):
        """Initialize normalization wrapper.

        Args:
            env: Base environment
            epsilon: Small value to prevent division by zero
            clip_range: Clip normalized values to [-clip_range, +clip_range]
        """
        super().__init__(env)
        self.epsilon = epsilon
        self.clip_range = clip_range
        self.component_stats: Dict[str, RunningStats] = {}

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        return self.env.reset(seed=seed, options=options)

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Extract components (fallback to total reward if not available)
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
            normalized = np.clip(normalized, -self.clip_range, self.clip_range)

            normalized_components[name] = normalized
            total_normalized += normalized

        # Store for debugging
        info['normalized_components'] = normalized_components
        info['normalization_stats'] = {
            name: {'mean': stats.mean, 'std': stats.std}
            for name, stats in self.component_stats.items()
        }

        return obs, total_normalized, terminated, truncated, info
```

### Python Curriculum Wrapper

```python
# File: C:\Users\costa\src\warehouser\training\training\wrappers\curriculum.py

from typing import Tuple, Dict, List
import numpy as np
import gymnasium as gym

class CurriculumRewardWrapper(gym.Wrapper):
    """Curriculum learning for reward scheduling.

    Gradually increases task complexity by adjusting reward weights
    based on agent performance. Advances to next stage when success
    rate exceeds threshold over sliding window.
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
            stages: List of curriculum stage configs with 'reward_weights' dict
            success_threshold: Success rate to advance (0.0 to 1.0)
            window_size: Number of episodes to compute success rate
        """
        super().__init__(env)
        self.stages = stages
        self.success_threshold = success_threshold
        self.window_size = window_size

        self.current_stage = 0
        self.episode_successes: List[bool] = []
        self.episode_count = 0

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict | None = None
    ) -> Tuple[np.ndarray, dict]:
        obs, info = self.env.reset(seed=seed, options=options)

        # Inject current stage info
        info['curriculum_stage'] = self.current_stage
        info['curriculum_config'] = self.stages[self.current_stage]

        return obs, info

    def step(self, action) -> Tuple[np.ndarray, float, bool, bool, dict]:
        obs, reward, terminated, truncated, info = self.env.step(action)

        # Get current stage configuration
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
                print(f"Curriculum advanced to stage {self.current_stage}: {self.stages[self.current_stage].get('name', 'unknown')}")
```

## Files to Create

| File | Purpose |
|------|---------|
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_validator.hpp` | Reward config validation logic |
| `C:\Users\costa\src\warehouser\training\training\wrappers\pbrs_wrapper.py` | Python PBRS wrapper |
| `C:\Users\costa\src\warehouser\training\training\wrappers\reward_normalizer.py` | Component normalization |
| `C:\Users\costa\src\warehouser\training\training\wrappers\curriculum.py` | Curriculum learning |
| `C:\Users\costa\src\warehouser\training\training\config\curriculum.py` | Stage definitions |
| `C:\Users\costa\src\warehouser\training\training\utils\reward_logger.py` | Component logging |
| `C:\Users\costa\src\warehouser\training\training\utils\tensorboard_logger.py` | TensorBoard integration |
| `C:\Users\costa\src\warehouser\training\training\utils\hacking_detector.py` | Exploit detection |
| `C:\Users\costa\src\warehouser\training\tests\test_reward_hacking.py` | Adversarial test suite |
| `C:\Users\costa\src\warehouser\training\tests\test_wrappers.py` | Wrapper unit tests |

## Files to Modify

| File | Change |
|------|--------|
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_strategy.hpp` | Add PBRSNavigationStrategy class and PBRSConfig struct |
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_strategy.cpp` | Implement PBRSNavigationStrategy, replace NavigationRewardStrategy usage |
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_calculator.cpp` | Update factory functions to use PBRS, call validator on init |
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\params\rl_bridge_params.yaml` | Add pbrs_gamma, reward_weights section, enable_reward_validation flag |
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\rl_bridge_node.cpp` | Call RewardConfigValidator on startup, log validation results |
| `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\test\test_reward_strategy.cpp` | Add PBRS unit tests (verify gamma discounting, policy invariance) |
| `C:\Users\costa\src\warehouser\training\training\envs\ros_env.py` | Populate info dict with robot_pos, goal_pos for PBRS wrapper |
| `C:\Users\costa\src\warehouser\training\training\envs\pettingzoo_env.py` | Populate info dict for all agents, support shared reward with PBRS |

## Architecture Notes

### PBRS Policy Invariance Guarantee

The key innovation of PBRS is mathematical proof that adding F(s,a,s') = γΦ(s') - Φ(s) to rewards preserves the optimal policy. This means:
- Any potential function Φ can be used without biasing the learned policy
- Distance-to-goal is a natural choice for navigation tasks
- The agent learns the same optimal policy as with sparse rewards, but much faster
- No need to worry about reward hacking from shaping (unlike ad-hoc progress rewards)

### Reward Component Separation

Maintain clear separation between:
1. **Base rewards**: Task completion signals (success, collision, pickup, place)
2. **Shaping rewards**: PBRS signals that densify sparse rewards
3. **Regularization rewards**: Motion quality (smoothness, energy, time)
4. **Exploration rewards**: Coverage bonuses (only in early curriculum stages)

Each category has different magnitude and normalization requirements.

### Multi-Robot Reward Architecture

For multi-robot scenarios, support both:
1. **Individual rewards**: Each robot optimizes own objective (competitive)
2. **Shared rewards**: All robots receive average of individual rewards (cooperative)

Shared reward mode should be configurable at runtime, not compile-time. Implement in C++ RLBridgeNode, not just Python wrapper.

### Curriculum Progression Philosophy

Curriculum should follow principle of "gradual complexity increase":
- Start with single robot, single object, empty environment
- Add complexity one dimension at a time (more objects → obstacles → dynamic → multi-robot)
- Never regress to earlier stage (forward-only progression)
- Log stage transitions prominently for reproducibility

### Normalization Strategy

Use component-wise normalization instead of global normalization:
- Each component normalized independently to zero mean, unit variance
- Prevents one component from dominating due to scale
- Preserves semantic meaning of each component
- Enables interpretable reward debugging

### Testing Philosophy

Every reward change must pass adversarial tests BEFORE training:
- Empty warehouse: No objects → no positive rewards
- Unreachable goal: Agent should timeout gracefully
- Spinning: High angular velocity should not be rewarded
- Collision: Always terminal with large penalty
- Success dominance: Success bonus > cumulative shaping over max episode

## Verification

### Correctness Verification

- [ ] PBRS unit tests pass (verify gamma discounting, policy invariance property)
- [ ] Reward config validator detects known bad configs (success < shaping, positive collision)
- [ ] Normalization wrapper produces zero mean, unit variance over 1k steps
- [ ] Curriculum wrapper advances at correct success rate thresholds
- [ ] All adversarial tests pass on trained policy

### Performance Verification

- [ ] Training converges faster with PBRS vs raw progress (compare learning curves)
- [ ] Normalization reduces training variance (compare std of episode returns)
- [ ] Curriculum achieves >70% success on final stage
- [ ] No reward hacking detected by RewardHackingDetector
- [ ] Component logger shows all components contributing (no dead/dominant)

### Integration Verification

- [ ] C++ PBRS strategy works with existing CompositeRewardStrategy
- [ ] Python wrappers integrate with ROSGymEnv and WarehouseParallelEnv
- [ ] TensorBoard logs display correctly (components, correlation matrix, ablation)
- [ ] Curriculum stages load from config file without code changes
- [ ] Multi-robot shared reward works in both C++ and Python

### Quality Metrics

Track these metrics to validate improvements:
- **Convergence Speed**: Timesteps to reach 70% success rate
- **Sample Efficiency**: Total samples needed to achieve target performance
- **Final Success Rate**: Performance on held-out test scenarios
- **Path Efficiency**: Actual path length / optimal path length
- **Motion Smoothness**: Variance of linear and angular accelerations
- **Component Health**: Correlation matrix off-diagonal values < 0.7
- **Exploit Detection**: Zero warnings from RewardHackingDetector over 10k steps

## References

### Research Papers
- HPRS: Hierarchical PBRS (TU Wien, Frontiers in Robotics and AI, 2025)
- VBRS: Value-Based Reward Shaping (Electronics Journal, 2026)
- Confounding Robust Control via PBRS (arXiv:2602.10305, 2026)
- Reward Hacking in RL (Lilian Weng, 2024)
- Defining and Characterizing Reward Hacking (Skalse et al., arXiv:2209.13085)
- Dynamic Weight Scalarization for MORL (ScienceDirect, 2025)
- Multi-Objective RL for Navigation (arXiv:2312.07953, 2023)

### Implementation Guides
- Potential-Based Reward Shaping Overview (Emergent Mind)
- Modification-Considering Value Learning (OpenReview)
- Review on RL for Robotic Manipulators (Wiley, 2024)
- Deep RL for Robotics Survey (Annual Reviews, 2024)

### Codebase Context
- Current reward system: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\`
- Training infrastructure: `C:\Users\costa\src\warehouser\training\`
- Test coverage: 378 lines reward_strategy tests, 353 lines exploration tests
