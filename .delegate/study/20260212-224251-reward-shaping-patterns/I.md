# Introspect: Reward Shaping Patterns

Created: 2026-02-12

## Focus

Analysis of reward shaping implementation in warehouser RL bridge system. This covers the architecture, individual reward components, configuration system, multi-robot handling, and potential issues.

## Architecture Overview

The reward system uses a clean Strategy pattern with composability via Composite pattern:

**Core Files:**
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_strategy.hpp` (162 lines)
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_strategy.cpp` (202 lines)
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\include\warehouser_rl_bridge\reward_calculator.hpp` (75 lines)
- `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_rl_bridge\src\reward_calculator.cpp` (85 lines)

### Design Patterns

1. **Strategy Pattern**: `IRewardStrategy` interface with polymorphic `calculate()` method
   - Enables runtime swapping of reward functions
   - Each strategy is independently testable
   - Clean separation of concerns

2. **Composite Pattern**: `CompositeRewardStrategy` combines multiple strategies
   - Weighted sum of individual rewards: `total_reward = sum(weight_i * strategy_i.reward)`
   - Any strategy can trigger episode termination
   - Supports arbitrary composition depth

3. **Facade Pattern**: `RewardCalculator` provides backward-compatible interface
   - Legacy `RewardConfig` struct maps to composite strategy
   - Handles multi-robot indexing and truncation logic

### RewardContext Structure

```cpp
struct RewardContext {
    const WorldState& prev_world;
    const WorldState& curr_world;
    const Goal& goal;
    int step_count;
    int max_steps;
    size_t robot_index = 0;  // For multi-robot
};
```

All strategies receive same context, ensuring consistency.

## Individual Reward Strategies

### 1. NavigationRewardStrategy

**File:** `reward_strategy.hpp:55-70`, `reward_strategy.cpp:35-78`

**Configuration:**
```cpp
struct NavigationConfig {
    float progress_weight = 1.0f;      // Reward scale for distance progress
    float success_bonus = 100.0f;      // Sparse reward for goal reaching
    float goal_threshold = 0.5f;       // Distance to consider goal reached (meters)
};
```

**Behavior:**
- Calculates Euclidean distance to goal: `dist = sqrt((robot.x - goal.x)^2 + (robot.y - goal.y)^2)`
- Progress reward: `reward = (prev_dist - curr_dist) * progress_weight`
  - Positive when moving closer (dense signal)
  - Negative when moving away (punishment for wrong direction)
  - Zero when distance unchanged
- Terminates with `success_bonus` when `curr_dist < goal_threshold`

**Issues Identified:**
- No normalization: Progress reward magnitude depends on world scale
  - Moving 1m gives reward of 1.0 regardless of total distance to goal
  - In 10m x 10m world, max progress per step ~= max_velocity * dt
- Could reward "orbiting" if other penalties are weak
- No directional component (heading toward goal not rewarded)

### 2. CollisionRewardStrategy

**File:** `reward_strategy.hpp:78-89`, `reward_strategy.cpp:81-103`

**Configuration:**
```cpp
struct CollisionConfig {
    float collision_penalty = -100.0f;  // Large negative reward
};
```

**Behavior:**
- Checks if robot exists in current world state
- If `findRobotByIndex()` returns `nullptr`, assumes collision occurred
- Immediately terminates episode with penalty

**Issues Identified:**
- Assumes robot disappearance = collision (tight coupling to simulation implementation)
- No distinction between collision types (wall vs robot vs object)
- Binary penalty (no partial credit for "close calls")
- Hard termination prevents learning from post-collision states

### 3. TimeRewardStrategy

**File:** `reward_strategy.hpp:97-105`, `reward_strategy.cpp:107-117`

**Configuration:**
```cpp
struct TimeConfig {
    float time_penalty = -0.1f;  // Applied every step
};
```

**Behavior:**
- Returns constant penalty every timestep
- Encourages time-efficient solutions

**Issues Identified:**
- Constant penalty regardless of progress (could punish valid exploration)
- No scaling with episode horizon (same penalty for 100-step vs 1000-step tasks)
- Can cause premature greedy behavior before sufficient exploration

### 4. PickPlaceRewardStrategy

**File:** `reward_strategy.hpp:114-125`, `reward_strategy.cpp:121-150`

**Configuration:**
```cpp
struct PickPlaceConfig {
    float pickup_bonus = 50.0f;
    float place_bonus = 50.0f;
};
```

**Behavior:**
- Detects state transitions in `is_carrying` flag
- `!prev_robot->is_carrying && curr_robot->is_carrying` → pickup bonus
- `prev_robot->is_carrying && !curr_robot->is_carrying` → place bonus

**Issues Identified:**
- No verification of correct object picked (any pickup rewarded equally)
- Place bonus awarded even if dropped at wrong location
- No distance-to-object shaping (robot must discover pickup action randomly)
- Rewards manipulation without task completion (pick and drop anywhere)

### 5. ExplorationRewardStrategy

**File:** `exploration_reward.hpp:30-61`, `exploration_reward.cpp:27-67`

**Configuration:**
```cpp
struct ExplorationConfig {
    float new_cell_bonus = 1.0f;      // First visit reward
    float revisit_bonus = 0.0f;       // Subsequent visit reward (usually 0)
    float coverage_bonus = 10.0f;     // Bonus when target reached
    float coverage_target = 0.8f;     // Target coverage (0-1)
    OccupancyConfig occupancy = {};   // Grid discretization
};

struct OccupancyConfig {
    float world_width = 10.0f;
    float world_height = 10.0f;
    float cell_size = 0.5f;           // Grid resolution
};
```

**Behavior:**
- Discretizes world into grid cells: `grid_width = ceil(world_width / cell_size)`
- Tracks visit counts per cell in `OccupancyTracker`
- `markVisited(x, y)` returns `true` if first visit to cell
- Coverage = `visited_cells / total_cells`
- Terminates when `coverage >= coverage_target`

**Implementation Details:**
- Grid uses flattened indexing: `index = cy * grid_width + cx`
- Position clamping prevents out-of-bounds: `clamp(x, 0, world_width - 0.001)`
- **Stateful**: Must call `reset()` at episode start
- Thread-safe for single-threaded ROS nodes

**Issues Identified:**
- Mutable state in const `calculate()` method (uses `mutable OccupancyTracker`)
- Reset must be called manually (not automatic on episode reset)
- No penalty for revisiting (could get stuck oscillating between 2 cells)
- Grid resolution fixed at construction (no adaptive discretization)
- Coverage bonus only awarded once (at threshold crossing)
- No shaped reward as coverage increases (binary threshold)

## Composite Strategy System

**File:** `reward_strategy.hpp:137-155`, `reward_strategy.cpp:154-181`

### Implementation

```cpp
class CompositeRewardStrategy : public IRewardStrategy {
    vector<StrategyWeight> strategies_;  // {strategy, weight} pairs

    RewardResult calculate(const RewardContext& ctx) const override {
        RewardResult combined;
        for (const auto& sw : strategies_) {
            auto result = sw.strategy->calculate(ctx);
            combined.reward += sw.weight * result.reward;  // Weighted sum
            if (result.terminated) {
                combined.terminated = true;
                combined.termination_reason = result.termination_reason;
            }
        }
        return combined;
    }
};
```

### Weight Semantics

- All weights default to `1.0` in factory functions
- No weight normalization (sum of weights can be arbitrary)
- Weights are multiplicative scale factors, not probabilities
- No mechanism to adjust weights during training

### Termination Logic

- **Any** strategy can terminate the episode
- First termination reason is used (order-dependent)
- No way to override or prioritize termination sources
- Truncation handled separately in `RewardCalculator`

## Configuration System

### ROS Parameter Loading

**File:** `rl_bridge_node.cpp:12-27`, `rl_bridge_params.yaml:1-15`

```yaml
rl_bridge:
  ros__parameters:
    max_steps: 500
    progress_weight: 1.0
    collision_penalty: -100.0
    success_bonus: 100.0
    pickup_bonus: 50.0
    time_penalty: -0.1
    goal_threshold: 0.5
```

Parameters loaded at node construction, stored in `RewardConfig`, then converted to composite strategy via `createStrategyFromConfig()`.

### Default Strategy Construction

**File:** `reward_calculator.cpp:50-82`

```cpp
auto composite = std::make_unique<CompositeRewardStrategy>();
composite->addStrategy(NavigationRewardStrategy(nav_config), 1.0f);
composite->addStrategy(CollisionRewardStrategy(coll_config), 1.0f);
composite->addStrategy(TimeRewardStrategy(time_config), 1.0f);
composite->addStrategy(PickPlaceRewardStrategy(pp_config), 1.0f);
```

All weights hardcoded to `1.0` - no weight parameters exposed in YAML.

### Python Configuration

**File:** `training/training/models/config.py:54-94`

Pydantic models mirror C++ config:
- `RewardConfig` class with field validation
- Bounds checking: weights in `[-1000, 1000]`
- Not currently used to configure ROS parameters (disconnected from C++ side)

**Issue:** Python config exists but doesn't affect C++ reward calculation.

## Factory Functions

**File:** `reward_strategy.cpp:185-199`, `exploration_reward.cpp:71-92`

### Available Presets

1. **createDefaultRewardStrategy()**: Navigation + Collision + Time + PickPlace (all weight 1.0)
2. **createNavigationOnlyStrategy()**: Pure navigation
3. **createExplorationOnlyStrategy()**: Exploration + Collision + Time (time weight 0.1)
4. **createMultiTaskRewardStrategy()**: Navigation (0.5) + Exploration (0.3) + Collision (1.0) + Time (0.1)

**Issues:**
- Weights chosen arbitrarily (no documented rationale)
- No curriculum progression (e.g., start exploration-heavy, shift to navigation)
- Cannot configure these from YAML (hardcoded in C++)

## Multi-Robot Reward Handling

### Per-Robot Calculation

**File:** `rl_bridge_node.cpp:114-118`

```cpp
auto reward_result = reward_calculators_[robot_id].calculate(
    prev_world_states_[robot_id], curr_world_, current_goal_,
    step_count_, max_steps_);
```

- Each robot has own `RewardCalculator` instance
- Each tracks own previous world state
- All use same current world state and goal

### Credit Assignment

**Current Implementation:**
- Individual rewards per robot
- No coordination incentives
- Robots compete for same goal (potential conflict)
- No shared reward option in C++ (only Python PettingZoo wrapper)

**Python Shared Reward (PettingZoo):**

**File:** `training/training/envs/pettingzoo_env.py:285-289`

```python
if self.config.shared_reward and len(self.agents) > 0:
    avg_reward = total_reward / len(self.agents)
    for agent in self.agents:
        rewards[agent] = avg_reward
```

- Averages all robot rewards
- Encourages cooperation
- **Only available in Python wrapper, not C++ implementation**

### Issues in Multi-Robot Context

1. **No cooperation incentives**: Individual rewards → competitive behavior
2. **Single goal for all robots**: No task allocation
3. **Collision between robots not penalized**: `CollisionRewardStrategy` only checks if robot exists, not inter-robot collisions
4. **Exploration sharing**: All robots write to same `OccupancyTracker` (good for team exploration, bad for individual credit)
5. **Sequential stepping**: Python env steps robots one-by-one, not truly parallel

## Reward Normalization

**Current State:** No normalization implemented.

### Magnitude Analysis

| Component | Typical Range | Scale |
|-----------|--------------|-------|
| Navigation progress | [-5.0, +5.0] | Per-step distance change |
| Collision penalty | -100.0 | One-time |
| Success bonus | +100.0 | One-time |
| Pickup/Place bonus | +50.0 each | Per action |
| Time penalty | -0.1 | Per step |
| Exploration | +1.0 per cell | Per new cell |

**Issues:**
- Sparse rewards (collision, success, pickup) dominate dense rewards (navigation, time)
- Scale mismatch: time penalty (-0.1) vs collision penalty (-100.0) = 1000x difference
- Episode reward heavily depends on termination type (success vs collision = 200 point swing)
- No clipping or standardization
- Agent could learn to optimize for avoiding -100 collision rather than achieving +100 success

## Potential Reward Hacking Risks

### Identified Vulnerabilities

1. **Pickup/Place Exploitation**
   - `PickPlaceRewardStrategy:134-141`: Rewards any pickup/place transition
   - Agent could repeatedly pick and drop same object for +100 reward per cycle
   - No verification of task-relevant manipulation

2. **Exploration Grinding**
   - `ExplorationRewardStrategy:44-48`: +1.0 per new cell
   - Agent could ignore goal and maximize coverage for easy rewards
   - Coverage bonus (+10.0) awarded once, but cell rewards accumulate
   - With 10x10 world @ 0.5m cells = 400 cells = up to 400 reward points

3. **Time Penalty Avoidance**
   - Small magnitude (-0.1) easily dominated by other rewards
   - Agent could take extremely long paths if exploration/pickup rewards outweigh time cost
   - 1000 steps = -100 time penalty = 1 collision penalty (insufficient deterrent)

4. **Goal Distance Manipulation**
   - Navigation reward based on distance delta, not absolute distance
   - Agent could move away then back repeatedly if other rewards compensate

5. **Collision as Episode Shortcut**
   - If agent is far from goal with low expected return, collision (-100) might be preferable to time penalty accumulation
   - Could learn to "give up" via intentional collision

## Missing Reward Components

Based on warehouse robotics best practices:

1. **Orientation Alignment**: No reward for facing goal direction
2. **Smooth Control**: No penalty for jerky movements (high angular velocity changes)
3. **Energy Efficiency**: No penalty for high velocities or accelerations
4. **Obstacle Proximity**: No shaping for danger (binary collision only)
5. **Task Completion**: Pick + Place + Correct Location not verified as single task
6. **Multi-Robot Coordination**:
   - No reward for load balancing
   - No penalty for redundant coverage
   - No bonus for task specialization

## Reward Scale Imbalances

### Relative Magnitudes

**Sparse vs Dense:**
- Success bonus (100.0) requires ~100 steps of perfect navigation progress (1.0/step) to equal
- Collision penalty (-100.0) = -1000 time penalties or -100 steps of zero progress

**Exploration vs Navigation:**
- In `createMultiTaskRewardStrategy()`: navigation weight 0.5, exploration weight 0.3
- But exploration gives +1.0 per cell, navigation gives ~1.0 max per step
- Effective exploration reward = 0.3, effective navigation reward = 0.5
- Close balance, but arbitrary choice

### Recommendation for Normalization

- Clip dense rewards to [-10, +10] per step
- Scale sparse rewards to be ~10x dense reward max (success = +100 reasonable if progress capped at ±10)
- Normalize by episode horizon: `time_penalty = -success_bonus / max_steps`

## Testing Coverage

**Test Files:**
- `test_reward_calculator.cpp`: 162 lines, 11 tests
- `test_reward_strategy.cpp`: 378 lines, 34 tests
- `test_exploration_reward.cpp`: 353 lines, 18 tests

**Coverage:**
- All individual strategies tested
- Composite strategy tested
- Multi-robot indexing tested
- Edge cases: no robot, boundary conditions, coverage thresholds

**Gaps:**
- No tests for reward magnitude balance
- No tests for exploit scenarios (repeated pickup/drop)
- No tests for multi-robot cooperation vs competition
- No integration tests with actual training loop

## Configuration and Weighting

### Current State

**Static Weights:**
- Hardcoded in factory functions (`createDefaultRewardStrategy`, etc.)
- YAML parameters only affect individual strategy configs, not composite weights
- Cannot tune weight balance without recompiling

**Weight Selection Process:**
- No documented rationale for chosen values
- Likely hand-tuned during development
- No automated tuning mechanism

### Proposed Improvements

1. **Expose weights in YAML:**
   ```yaml
   reward_weights:
     navigation: 1.0
     collision: 1.0
     time: 0.1
     exploration: 0.3
   ```

2. **Automatic weight scheduling:**
   - Start with exploration-heavy weights early in training
   - Shift to task-completion weights after initial learning
   - Implement as callback in training loop

3. **Multi-objective optimization:**
   - Log individual reward components separately
   - Use Pareto frontier analysis to tune weights
   - Consider scalarization methods (weighted sum, Chebyshev, etc.)

## Gap Analysis vs Best Practices

### Strengths

1. Clean modular architecture (Strategy + Composite patterns)
2. Individual components well-tested
3. Multi-robot support at framework level
4. Stateful exploration tracking

### Weaknesses

1. **No reward normalization**: Magnitudes vary 1000x
2. **No shaped rewards for manipulation**: Binary pickup/place detection
3. **Hardcoded weight combinations**: Cannot tune without rebuild
4. **Missing dense signals**: Orientation, obstacle proximity, control smoothness
5. **Reward hacking vulnerabilities**: Pickup loops, exploration grinding
6. **Multi-robot competition**: No cooperation incentives in C++ layer
7. **Disconnected Python config**: Pydantic models not used for ROS parameters
8. **No curriculum learning**: Static reward function throughout training

### Alignment with Robotics RL Best Practices

**Good:**
- Sparse + dense hybrid approach
- Modular reward components
- Per-robot reward calculation

**Needs Improvement:**
- Reward magnitude standardization (see "Reward Scaling" in RL literature)
- Shaped rewards for hierarchical tasks (pickup → navigate → place)
- Potential-based reward shaping for theoretical guarantees
- Automatic weight tuning (e.g., evolutionary strategies, hyperparameter optimization)

## Specific Files and Roles

### Core Reward System

| File | Lines | Purpose |
|------|-------|---------|
| `reward_strategy.hpp` | 167 | Interface + concrete strategies |
| `reward_strategy.cpp` | 202 | Strategy implementations |
| `reward_calculator.hpp` | 75 | Facade + legacy interface |
| `reward_calculator.cpp` | 85 | Facade implementation + factory |
| `exploration_reward.hpp` | 74 | Exploration strategy |
| `exploration_reward.cpp` | 95 | Exploration implementation |
| `occupancy_tracker.hpp` | 70 | Grid-based coverage tracking |
| `occupancy_tracker.cpp` | 74 | Tracker implementation |

### Configuration

| File | Lines | Purpose |
|------|-------|---------|
| `rl_bridge_params.yaml` | 15 | ROS parameter defaults |
| `rl_bridge_node.cpp` | 313 | Parameter loading + reward calculator setup |
| `training/models/config.py` | 297 | Python-side config (unused for ROS) |

### Testing

| File | Lines | Tests | Coverage |
|------|-------|-------|----------|
| `test_reward_strategy.cpp` | 378 | 34 | Individual + composite strategies |
| `test_reward_calculator.cpp` | 162 | 11 | Calculator facade |
| `test_exploration_reward.cpp` | 353 | 18 | Exploration + occupancy |

### Python Integration

| File | Lines | Purpose |
|------|-------|---------|
| `ros_env.py` | 223 | Single-robot Gym wrapper |
| `pettingzoo_env.py` | 331 | Multi-robot ParallelEnv wrapper |

Python wrappers receive rewards from C++ ROS services, no Python-side reward shaping.

## Proposal

### Immediate Fixes (Low-Hanging Fruit)

1. **Expose composite weights in YAML**: Add `navigation_weight`, `collision_weight`, etc. to `rl_bridge_params.yaml`
2. **Normalize dense rewards**: Clip progress rewards to [-5, +5] per step
3. **Fix pickup/place verification**: Check if correct object picked and placed at goal
4. **Add exploration reset to episode reset**: Automatically call `ExplorationRewardStrategy::reset()` in `handleRLReset()`
5. **Document weight rationale**: Add comments explaining magnitude choices

### Medium-Term Improvements

1. **Implement potential-based shaping**: Use goal distance as potential function for provably optimal shaping
2. **Add orientation reward**: `reward += cos(robot_heading - goal_bearing) * orientation_weight`
3. **Expose Python config to ROS**: Parse Python `RewardConfig` and publish to ROS parameter server
4. **Multi-robot cooperation**: Implement shared reward option in C++, not just Python wrapper
5. **Reward component logging**: Publish individual strategy rewards to separate topics for analysis

### Long-Term Architecture

1. **Curriculum learning framework**:
   - Define reward schedules (e.g., `ExplorationPhase(steps=100k) → NavigationPhase(steps=100k)`)
   - Implement weight interpolation between phases
2. **Automatic weight tuning**:
   - Log multi-objective metrics (success rate, time to goal, coverage, etc.)
   - Use evolutionary algorithms or Bayesian optimization to find Pareto-optimal weights
3. **Hierarchical rewards**:
   - Subtask rewards: `FindObject → ApproachObject → PickupObject → NavigateToGoal → PlaceObject`
   - Use options framework or hierarchical RL
4. **Adversarial reward validation**:
   - Test trained policies for exploit behaviors
   - Add penalty terms to close discovered loopholes

## Conclusion

The warehouser reward system demonstrates solid software engineering with clean patterns and good test coverage. However, it exhibits common RL reward engineering pitfalls:

- **Scale imbalances** between sparse and dense components
- **Reward hacking vulnerabilities** in pickup/place and exploration
- **Static configuration** preventing runtime tuning
- **Missing cooperation incentives** for multi-robot scenarios

Addressing these issues through normalization, verification, and configurable weighting would significantly improve training stability and task performance.
