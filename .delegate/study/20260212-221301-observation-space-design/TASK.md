# TASK: Implement Production-Ready Observation Space with Normalization

Created: 2026-02-12T22:30:00Z
Build: Tests have import errors (missing pydantic in test environment)
Tests: 28 collected / 4 errors / 2 skipped

## Summary

Warehouser's observation system has solid architecture but critical gaps for production RL training. V1 uses privileged information (absolute positions), V2 lidar is specified but not implemented despite lidar infrastructure being ready, and most critically, no observation normalization is applied. This task implements best-practice observation design: VecNormalize wrapper, completes V2 lidar integration, makes observations ego-centric, and adds optional V4 dict-based observation space for future flexibility.

## Context

### From Search Phase [S.md]

Research validates the move from V1 to V2/V3 and identifies critical requirements:

- **Ego-centric observations**: Best practice for sim-to-real transfer, only use sensor-realistic data
- **VecNormalize is mandatory**: SB3 documentation emphasizes normalization for non-image inputs
- **Goal encoding**: Use domain-invariant representations (relative distance/bearing, not absolute positions)
- **Domain randomization**: Sensor noise models critical for sim-to-real robustness
- **Temporal information**: Frame stacking or explicit velocity helps capture dynamics
- **Sensor fusion pattern**: Combine lidar (geometry) + goal (task) + velocity (dynamics)

Key finding: V1's absolute positions are privileged information unsuitable for real deployment.

### From Introspection Phase [I.md]

Codebase analysis reveals:

**What works:**
- Clean 3-layer architecture: Simulation (C++) → Transport (ROS2) → Training (Python)
- V1 and V3 fully implemented with comprehensive unit tests
- Lidar simulator exists and is functional (60 rays, 180° FOV, noise models ready)
- Noise infrastructure present (NoiseModel class with Gaussian + dropout)
- Ego-centric coordinate transforms correct in V3 (buildV3 lines 119-133)

**Critical gaps:**
1. **No normalization** (`train.py:112`) - Raw unbounded observations fed to network
2. **V2 not connected** (`observation_builder.cpp:17-20`) - Falls back to V1 despite lidar being ready
3. **Absolute positions** (`observation_builder.cpp:57-59`) - V1/V3 include world coordinates
4. **Unbounded observation space** (`ros_env.py:47-49`) - Box(low=-inf, high=inf)
5. **Noise models unused** - Domain randomization ready but not applied to observations

**Data flow issues:**
- Lidar published to `/observations/lidar_debug` and `/scan` but NOT in observation vector
- Odometry computed but NOT included in observations (no velocity information)
- Observation features have vastly different scales: positions [0,10], angles [-3.14,3.14], binary {0,1}

### From Template Phase [T.md]

Provides production-ready patterns from Stable-Baselines3:

**VecNormalize pattern:**
- Running mean/std normalization using Welford's algorithm
- Saves statistics with model for deployment
- Supports Dict spaces with selective normalization via `norm_obs_keys`

**Ego-centric transformation:**
- Correct 2D rotation matrix implementation from V3 can be reused
- Transform: `ego_frame = rotate(-ego_theta) * (world_pos - ego_pos)`

**Proposed V4 observation space:**
- Dict-based structure with semantic components
- Fully ego-centric (no privileged information)
- 75 dims total: lidar(60) + goal(2) + velocity(3) + carrying(1) + other_robots(9)

## Objective

Implement production-ready observation space that:
1. Enables stable, fast training via VecNormalize
2. Supports sim-to-real transfer with ego-centric, sensor-realistic observations
3. Provides complete V2 lidar integration for vision-based policies
4. Maintains backward compatibility with V1/V3 during transition
5. Establishes foundation for multi-modal observation spaces (V4)

## Scope

### Phase 1: Observation Normalization (HIGH PRIORITY)

**Impact:** Immediate training improvement with minimal code change

- `training/training/scripts/train.py`: Add VecNormalize wrapper
- `training/training/scripts/train.py`: Save/load normalization statistics
- `training/training/models/config.py`: Add normalization config options
- Update documentation on normalization strategy

### Phase 2: V2 Lidar Completion (HIGH PRIORITY)

**Impact:** Enables vision-based navigation with obstacle awareness

- `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp`: Add buildV2 declaration
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp`: Implement buildV2 (connect existing lidar)
- `ros_ws/src/warehouser_observations/test/test_observation_builder.cpp`: Add V2 unit tests
- `training/training/models/config.py`: Add V2 config with obs_dim=63

### Phase 3: Ego-Centric V1/V3 (MEDIUM PRIORITY)

**Impact:** Better generalization, prepare for sim-to-real

- `ros_ws/src/warehouser_observations/src/observation_builder.cpp`: Remove absolute positions from V1
- `ros_ws/src/warehouser_observations/src/observation_builder.cpp`: Remove absolute ego state from V3
- Update obs_dim: V1: 8→5, V3: 17→14
- `training/training/models/config.py`: Update default obs_dim values
- Retrain models with new observation space

### Phase 4: V4 Dict Observation Space (LOW PRIORITY - FUTURE)

**Impact:** Cleaner API, selective normalization, extensibility

- Design V4 Dict observation space structure
- Implement in ObservationBuilder with semantic components
- Add velocity observations (from odometry or WorldState)
- Create Dict-compatible ROSGymEnv variant
- VecNormalize with `norm_obs_keys` for selective normalization

## Implementation Plan

### Phase 1: Add VecNormalize (Immediate)

**Files to Modify:**

| File | Change |
|------|--------|
| `training/training/scripts/train.py` | Wrap DummyVecEnv with VecNormalize after line 112 |
| `training/training/scripts/train.py` | Save vec_normalize.pkl with model |
| `training/training/scripts/evaluate.py` | Load normalization stats, set training=False |
| `training/training/models/config.py` | Add TrainingConfig fields: norm_obs, norm_reward, clip_obs |

**Code Template:**
```python
# In train.py after line 112
env = DummyVecEnv([lambda: make_env(env_config)])

# Add VecNormalize wrapper
from stable_baselines3.common.vec_env import VecNormalize
env = VecNormalize(
    env,
    training=True,
    norm_obs=True,
    norm_reward=True,
    clip_obs=10.0,
    clip_reward=10.0,
    gamma=0.99,
    epsilon=1e-8
)

# After model.save()
model.save(model_path)
env.save(model_path.replace('.zip', '_vecnormalize.pkl'))
```

**Acceptance Criteria:**
- [ ] VecNormalize wrapper added to training pipeline
- [ ] Normalization statistics saved with model
- [ ] Evaluation loads stats and sets training=False
- [ ] Training runs successfully with normalization
- [ ] Observation means ~0, stds ~1 after warmup

### Phase 2: Implement V2 Lidar

**Files to Create:**
None - all infrastructure exists

**Files to Modify:**

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp` | Add `buildV2()` declaration (private method) |
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Replace V2 fallback (lines 17-20) with `return buildV2(world, goal, robot_index);` |
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Implement `buildV2()` method (63 dims: lidar[60] + goal_bearing + goal_dist + is_carrying) |
| `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp` | Add LidarSimulator member to ObservationBuilder class |
| `ros_ws/src/warehouser_observations/src/observations_node.cpp` | Pass lidar simulator instance to ObservationBuilder constructor |
| `ros_ws/src/warehouser_observations/test/test_observation_builder.cpp` | Add V2 dimension tests, lidar range tests, goal encoding tests |
| `training/training/models/config.py` | Update EnvConfig with obs_dim=63 option for V2 |

**buildV2 Implementation:**
```cpp
warehouser_msgs::msg::Observation ObservationBuilder::buildV2(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {

    warehouser_msgs::msg::Observation obs;
    obs.version = 2;
    obs.data.resize(63, 0.0f);

    const auto* robot = findRobotByIndex(world, robot_index);
    if (!robot) return obs;

    // Get lidar scan (reuse existing lidar_ simulator)
    auto ranges = lidar_.scan(robot->x, robot->y, robot->theta, world);
    std::copy(ranges.begin(), ranges.end(), obs.data.begin());

    // Goal bearing and distance (ego-centric)
    float dx = goal.x - robot->x;
    float dy = goal.y - robot->y;
    float world_angle = std::atan2(dy, dx);
    obs.data[60] = normalizeAngle(world_angle - robot->theta);  // bearing
    obs.data[61] = std::sqrt(dx*dx + dy*dy);  // distance
    obs.data[62] = robot->is_carrying ? 1.0f : 0.0f;

    return obs;
}
```

**Acceptance Criteria:**
- [ ] V2 buildV2() method implemented
- [ ] Lidar ranges correctly copied to observation (60 dims)
- [ ] Goal bearing in ego-centric frame
- [ ] Unit tests pass for V2 dimension, lidar values, goal encoding
- [ ] colcon test succeeds for warehouser_observations
- [ ] Python training can load V2 observations (obs_dim=63)

### Phase 3: Ego-Centric V1/V3 Refactor

**Files to Modify:**

| File | Change |
|------|--------|
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Remove robot_x, robot_y, robot_theta from V1 (lines 57-59) |
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Adjust V1 dimension: 8→5 (goal_dx, goal_dy, goal_dist, goal_heading, is_carrying) |
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Remove absolute ego state from V3 (lines 99-102) |
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | Adjust V3 dimension: 17→14 (5 ego + 3×3 other robots) |
| `ros_ws/src/warehouser_observations/test/test_observation_builder.cpp` | Update V1/V3 dimension expectations |
| `training/training/models/config.py` | Update default obs_dim: EnvConfig(obs_dim=5), MultiAgentConfig(obs_dim=14) |
| `CLAUDE.md` | Document ego-centric observation design decision |

**Alternative approach (less breaking):**
- Keep current V1/V3, create V1_Ego and V3_Ego variants
- Deprecate V1/V3 in documentation
- Allows gradual migration

**Acceptance Criteria:**
- [ ] V1/V3 only contain ego-centric observations
- [ ] No absolute world coordinates in observation vector
- [ ] Unit tests updated and passing
- [ ] Training config defaults updated
- [ ] Documentation reflects design change

### Phase 4: V4 Dict Observation Space (Future Work)

**Proposed Structure:**
```python
observation_space = spaces.Dict({
    'lidar': spaces.Box(0.0, 1.0, shape=(60,), dtype=np.float32),
    'goal': spaces.Box([-np.inf, -np.pi], [np.inf, np.pi], shape=(2,), dtype=np.float32),  # [dist, bearing]
    'velocity': spaces.Box([-2.0, -2.0, -3.0], [2.0, 2.0, 3.0], shape=(3,), dtype=np.float32),  # [vx, vy, omega]
    'carrying': spaces.Box(0.0, 1.0, shape=(1,), dtype=np.float32),
    'other_robots': spaces.Box(-np.inf, np.inf, shape=(9,), dtype=np.float32)  # 3×[rel_x, rel_y, rel_theta]
})
```

**Files to Create:**

| File | Purpose |
|------|---------|
| `ros_ws/src/warehouser_observations/src/observation_builder.cpp` | buildV4() method returning dict-like structure |
| `training/training/envs/ros_env_dict.py` | ROSGymEnvDict with Dict observation space |
| `ros_ws/src/warehouser_msgs/msg/ObservationDict.msg` | Message for dict observations with named fields |

**Files to Modify:**

| File | Change |
|------|--------|
| `training/training/scripts/train.py` | Support Dict observation space with norm_obs_keys |
| `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp` | Add V4_Dict enum and buildV4() |

**Acceptance Criteria:**
- [ ] V4 observation space defined with semantic structure
- [ ] VecNormalize with selective normalization (norm_obs_keys)
- [ ] Velocity observations included from odometry
- [ ] Training works with Dict observations
- [ ] Backward compatible with V1/V2/V3

## Interface Definitions

### V2 Observation Structure
```python
# Dimension: 63
# dtype: np.float32
# Index mapping:
# [0:60]   - lidar ranges (normalized [0, 1] where 1 = max_range)
# [60]     - goal_bearing (radians, ego-centric, [-π, π])
# [61]     - goal_distance (meters, [0, ~14.14 for 10x10 world])
# [62]     - is_carrying (binary, 0.0 or 1.0)
```

### VecNormalize Configuration
```python
@dataclass
class TrainingConfig:
    """Training configuration with normalization options."""

    # Existing fields...

    # Normalization
    norm_obs: bool = True          # Normalize observations
    norm_reward: bool = True       # Normalize rewards
    clip_obs: float = 10.0        # Clip normalized obs to [-clip_obs, clip_obs]
    clip_reward: float = 10.0     # Clip normalized reward
    gamma: float = 0.99           # Discount factor (for reward normalization)

    # VecNormalize save path
    vec_normalize_path: str = "vec_normalize.pkl"
```

### Ego-Centric V1 (Refactored)
```python
# Dimension: 5
# dtype: np.float32
# [0]     - goal_dx (meters, relative X)
# [1]     - goal_dy (meters, relative Y)
# [2]     - goal_distance (meters)
# [3]     - goal_heading (radians, ego-centric)
# [4]     - is_carrying (0.0 or 1.0)
```

### Ego-Centric V3 (Refactored)
```python
# Dimension: 14 (for max_other_robots=3)
# dtype: np.float32
# [0:5]   - Ego state (same as V1 ego-centric)
# [5:14]  - Other robots (3×[rel_x, rel_y, rel_theta])
# Zero-padded if fewer than max_other_robots
```

## Architecture Notes

### Normalization Strategy

**Why VecNormalize is mandatory:**
- Neural networks perform best with zero-mean, unit-variance inputs
- Current observations have vastly different scales (positions [0,10], angles [-3.14,3.14], binary {0,1})
- Running statistics adapt to actual data distribution (better than manual normalization)
- SB3 documentation explicitly recommends for non-image environments

**Implementation details:**
- Welford's online algorithm for running mean/std (numerically stable)
- Statistics saved as pickle file with model
- Deployment must load same statistics (critical for consistency)
- Training mode updates stats, evaluation mode freezes stats

### Ego-Centric Design Rationale

**Benefits:**
- Policy generalizes across positions and orientations
- Sim-to-real transfer (no world-coordinate assumptions)
- Matches embodied AI paradigm (agent-centric perception)
- Natural for sensor-based observations (lidar is already ego-centric)

**Trade-offs:**
- Slightly harder initial learning (no global position context)
- Requires sufficient exploration to discover world structure
- MARL requires explicit communication for global coordination

**Mitigation:**
- Use curriculum learning (small world → large world)
- Add exploration bonuses (coverage reward already implemented)
- For MARL, consider communication channel in V4

### V2 Lidar Integration

**Design decision: Reuse existing lidar simulator**
- LidarSimulator already implemented, tested, and publishing to ROS topics
- Noise models already configured (Gaussian + dropout)
- No code duplication, minimal changes to ObservationBuilder

**Architecture choice: Lidar in ObservationBuilder vs separate component**
- Current: LidarSimulator in ObservationsNode
- Required: ObservationBuilder needs access to scan results
- Solution: Pass LidarSimulator reference to ObservationBuilder constructor
- Alternative: Compute lidar in ObservationsNode, pass ranges to buildV2

### Modularity and Extensibility

**Current architecture strengths:**
- Clean separation: Simulation (C++) ↔ Transport (ROS2) ↔ Training (Python)
- Version enum allows runtime switching (V1/V2/V3)
- ObservationBuilder stateless (pure function based on WorldState)
- Comprehensive unit tests for each version

**V4 Dict space maintains modularity:**
- Semantic components (lidar, goal, velocity, etc.) separately normalized
- Easy to add/remove components without changing dimension arithmetic
- VecNormalize norm_obs_keys allows selective normalization
- Dict structure self-documenting (no manual index tracking)

## Verification

### Phase 1 Verification (VecNormalize)

- [ ] Training script runs without errors
- [ ] vec_normalize.pkl file created alongside model
- [ ] Observation statistics logged during training (mean, std)
- [ ] After 10k steps: observation means ≈ 0 (within [-0.5, 0.5])
- [ ] After 10k steps: observation stds ≈ 1 (within [0.5, 2.0])
- [ ] Evaluation loads normalization stats correctly
- [ ] Evaluation sets training=False (stats frozen)

### Phase 2 Verification (V2 Lidar)

- [ ] colcon build succeeds for warehouser_observations
- [ ] colcon test succeeds (V2 unit tests pass)
- [ ] V2 observation dimension is 63
- [ ] Lidar ranges in [0, max_range] (10.0m)
- [ ] Goal bearing in [-π, π]
- [ ] Python training env accepts obs_dim=63
- [ ] Training runs successfully with V2 observations
- [ ] Policy learns to avoid obstacles (not just goal-seeking)

### Phase 3 Verification (Ego-Centric V1/V3)

- [ ] V1 dimension is 5 (not 8)
- [ ] V3 dimension is 14 (not 17) for max_other_robots=3
- [ ] No absolute positions (x, y, theta) in observation vector
- [ ] Unit tests updated and passing
- [ ] Training config defaults updated
- [ ] Retrained model shows generalization across world positions

### Phase 4 Verification (V4 Dict Space)

- [ ] Dict observation space defined and functional
- [ ] VecNormalize with norm_obs_keys=['lidar', 'goal', 'velocity']
- [ ] Velocity observations populated from odometry
- [ ] Training supports Dict observations
- [ ] Policy network receives correctly normalized dict components
- [ ] Backward compatibility with V1/V2/V3 maintained

## Testing Strategy

### Unit Tests (C++)
```bash
cd ros_ws
colcon test --packages-select warehouser_observations
colcon test-result --verbose
```

Expected tests:
- V2 dimension = 63
- V2 lidar ranges valid (0 to max_range)
- V2 goal encoding correct (bearing, distance)
- V1 dimension = 5 (after ego-centric refactor)
- V3 dimension = 14 (after ego-centric refactor)

### Integration Tests (Python)
```bash
cd training
pytest tests/test_env.py -v  # ROSGymEnv with V2
pytest tests/test_pettingzoo_env.py -v  # Multi-agent with V3
```

### Training Smoke Test
```bash
cd training
python training/scripts/train.py --total-timesteps 10000 --observation-version V2
# Verify: vec_normalize.pkl created, training completes, obs stats logged
```

### Normalization Validation
```python
# After training
from stable_baselines3.common.vec_env import VecNormalize
import numpy as np

env = VecNormalize.load("vec_normalize.pkl", make_env())
print(f"Obs mean: {env.obs_rms.mean}")
print(f"Obs std: {np.sqrt(env.obs_rms.var)}")
# Expected: means ≈ 0, stds ≈ 1
```

## Dependencies

### Python Packages
- stable-baselines3 (already present)
- gymnasium (already present)
- numpy (already present)
- pydantic (already present - but missing in test env per build check)

### ROS2 Packages
- warehouser_msgs (already present)
- warehouser_observations (already present)

### No New Dependencies Required
All functionality uses existing infrastructure.

## Rollback Plan

If Phase 1 (VecNormalize) causes issues:
1. Remove VecNormalize wrapper, revert to DummyVecEnv only
2. Delete vec_normalize.pkl references
3. Training continues with unnormalized observations (slower but functional)

If Phase 2 (V2 Lidar) causes issues:
1. Revert observation_builder.cpp to fallback to V1
2. V2 remains unimplemented, use V1/V3 for training
3. Lidar continues publishing to debug topics only

If Phase 3 (Ego-Centric) causes issues:
1. Keep original V1/V3 with absolute positions
2. Create new V1_Ego/V3_Ego enum variants
3. Parallel development, gradual migration

## Priority Ranking

**IMMEDIATE (This Week):**
1. Phase 1: VecNormalize - Highest impact, lowest risk, enables better training immediately

**HIGH (Next Sprint):**
2. Phase 2: V2 Lidar - Unlocks vision-based policies, all infrastructure ready

**MEDIUM (Future Sprint):**
3. Phase 3: Ego-Centric V1/V3 - Important for generalization, breaking change requires careful migration

**LOW (Research/Exploration):**
4. Phase 4: V4 Dict Space - Nice-to-have, enables cleaner API and future extensions

## Success Metrics

**Quantitative:**
- Training convergence 2-3x faster with VecNormalize (measured by episode reward)
- V2 policy achieves >90% obstacle avoidance (measured in test environment)
- Ego-centric policy generalizes to 20x20 world after training on 10x10 (transfer test)

**Qualitative:**
- Cleaner observation space design (no privileged information)
- Better code maintainability (Dict space with semantic names)
- Production-ready normalization pipeline (saved statistics)

## References

- S.md: Search findings on observation space best practices
- I.md: Introspection findings on current codebase state
- T.md: Template patterns from Stable-Baselines3
- CLAUDE.md: Project conventions and standards
- Stable-Baselines3 VecNormalize docs: https://stable-baselines3.readthedocs.io/en/master/guide/vec_envs.html
