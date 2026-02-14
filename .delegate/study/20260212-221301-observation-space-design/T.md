# Template Analysis: Observation Space Design Patterns

Created: 2026-02-12T22:15:00Z

## Overview

This document analyzes observation space design patterns from Stable-Baselines3 and Gymnasium libraries, providing copy-paste-ready templates for improving Warehouser's observation space.

## Sources Analyzed

1. **VecNormalize**: `stable_baselines3.common.vec_env.vec_normalize.py`
2. **StackedObservations**: `stable_baselines3.common.vec_env.stacked_observations.py`
3. **RunningMeanStd**: `stable_baselines3.common.running_mean_std.py`
4. **Warehouser ObservationBuilder**: Current implementation in `ros_ws/src/warehouser_observations/`

## Templates and Patterns

### 1. VecNormalize - Observation Normalization

**Pattern**: Running mean/variance normalization for Box and Dict observation spaces.

**Core Algorithm**:
```python
# Normalization formula (line 221 of vec_normalize.py)
normalized = np.clip(
    (obs - obs_rms.mean) / np.sqrt(obs_rms.var + epsilon),
    -clip_obs,
    clip_obs
)
```

**Usage Template (Box Space)**:
```python
from stable_baselines3.common.vec_env import VecNormalize, DummyVecEnv

# Wrap environment with normalization
env = ROSGymEnv(config)
vec_env = DummyVecEnv([lambda: env])

# Add normalization wrapper
normalized_env = VecNormalize(
    vec_env,
    training=True,           # Update statistics during training
    norm_obs=True,           # Normalize observations
    norm_reward=True,        # Normalize rewards
    clip_obs=10.0,          # Clip normalized obs to [-10, 10]
    clip_reward=10.0,       # Clip normalized reward
    gamma=0.99,             # Discount factor for reward normalization
    epsilon=1e-8            # Numerical stability
)

# Train model
model = PPO("MlpPolicy", normalized_env)
model.learn(total_timesteps=100000)

# CRITICAL: Save normalization statistics
normalized_env.save("vec_normalize.pkl")
model.save("ppo_model")

# For deployment/evaluation - load statistics and disable training
eval_env = DummyVecEnv([lambda: ROSGymEnv(config)])
eval_env = VecNormalize.load("vec_normalize.pkl", eval_env)
eval_env.training = False  # Don't update stats during evaluation
eval_env.norm_reward = False  # Don't normalize rewards during evaluation
```

**Usage Template (Dict Space)**:
```python
# For Dict observation spaces, specify which keys to normalize
# Example observation space:
# {
#   'lidar': Box(60,),
#   'goal': Box(2,),
#   'velocity': Box(3,),
#   'carrying': Discrete(2)  # Don't normalize discrete!
# }

normalized_env = VecNormalize(
    vec_env,
    training=True,
    norm_obs=True,
    norm_obs_keys=['lidar', 'goal', 'velocity'],  # Only normalize Box spaces
    clip_obs=10.0,
    epsilon=1e-8
)
```

**Key Insights**:
- Only supports `gym.spaces.Box` and `gym.spaces.Dict` (lines 100-126)
- For Dict spaces, must explicitly pass `norm_obs_keys` for selective normalization
- Automatically updates statistics during training (lines 186-191)
- Handles terminal observations correctly (lines 199-204)
- Pickle-able for saving/loading (lines 311-332)

### 2. Frame Stacking - Temporal Observations

**Pattern**: Stack multiple consecutive observations to capture temporal information (velocity, acceleration).

**Implementation**:
```python
from stable_baselines3.common.vec_env import VecFrameStack

# Stack 4 consecutive frames
stacked_env = VecFrameStack(vec_env, n_stack=4, channels_order='last')

# Observation shape changes:
# Before: (obs_dim,) e.g., (8,)
# After: (obs_dim * n_stack,) e.g., (32,) for n_stack=4
```

**How it works** (from `stacked_observations.py`):
```python
# Stack dimension detection (lines 80-93)
if is_image_space(observation_space):
    # For images, auto-detect channels (first or last)
    channels_first = is_image_space_channels_first(observation_space)
else:
    # For non-image spaces, stack on last axis
    channels_first = False

# Stack on last dimension: [obs1, obs2, obs3, obs4]
# Stack on first dimension: [[obs1], [obs2], [obs3], [obs4]]
```

**Reset behavior** (lines 102-117):
```python
def reset(self, observation):
    """Initialize stack with reset observation repeated."""
    self.stacked_obs[...] = 0  # Zero-fill
    # Place observation in last position of stack
    if self.channels_first:
        self.stacked_obs[:, -observation.shape[1]:, ...] = observation
    else:
        self.stacked_obs[..., -observation.shape[-1]:] = observation
    return self.stacked_obs
```

**Update behavior** (lines 157-177):
```python
def update(self, observations, dones, infos):
    """Roll stack and add new observation."""
    # Shift stack left (oldest observation drops out)
    shift = -observations.shape[self.stack_dimension]
    self.stacked_obs = np.roll(self.stacked_obs, shift, axis=self.stack_dimension)

    # Handle episode termination - reset stack to zeros
    for env_idx, done in enumerate(dones):
        if done:
            self.stacked_obs[env_idx] = 0

    # Add new observation to end of stack
    if self.channels_first:
        self.stacked_obs[:, shift:, ...] = observations
    else:
        self.stacked_obs[..., shift:] = observations

    return self.stacked_obs, infos
```

**Application to Warehouser**:
```python
# Option 1: Use frame stacking for velocity inference
env = ROSGymEnv(config)
vec_env = DummyVecEnv([lambda: env])
vec_env = VecFrameStack(vec_env, n_stack=4)  # Stack 4 frames
vec_env = VecNormalize(vec_env, norm_obs=True)

# Option 2: Explicitly add velocity to observation (preferred for interpretability)
# Modify ObservationBuilder to include velocity instead
```

### 3. RunningMeanStd - Online Statistics

**Pattern**: Welford's online algorithm for computing running mean and variance.

**Implementation** (from `running_mean_std.py`):
```python
class RunningMeanStd:
    """Computes running mean and std of a data stream.

    Uses Welford's online algorithm:
    https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Parallel_algorithm
    """

    def __init__(self, epsilon: float = 1e-4, shape: tuple[int, ...] = ()):
        self.mean = np.zeros(shape, np.float64)
        self.var = np.ones(shape, np.float64)
        self.count = epsilon  # Prevent division by zero

    def update(self, arr: np.ndarray) -> None:
        """Update statistics with new batch of data."""
        batch_mean = np.mean(arr, axis=0)
        batch_var = np.var(arr, axis=0)
        batch_count = arr.shape[0]
        self.update_from_moments(batch_mean, batch_var, batch_count)

    def update_from_moments(
        self, batch_mean: np.ndarray, batch_var: np.ndarray, batch_count: float
    ) -> None:
        """Update from precomputed moments (parallelizable)."""
        delta = batch_mean - self.mean
        tot_count = self.count + batch_count

        # New mean
        new_mean = self.mean + delta * batch_count / tot_count

        # New variance (Welford's algorithm)
        m_a = self.var * self.count
        m_b = batch_var * batch_count
        m_2 = m_a + m_b + np.square(delta) * self.count * batch_count / tot_count
        new_var = m_2 / tot_count

        self.mean = new_mean
        self.var = new_var
        self.count = tot_count
```

**Application to Warehouser**:
```python
# Use case: Track observation statistics for debugging
obs_tracker = RunningMeanStd(shape=(8,))  # For 8-dim observation

# During training loop
obs_tracker.update(observations)  # observations shape: (num_envs, 8)

# Check if observations are well-scaled
print(f"Observation means: {obs_tracker.mean}")
print(f"Observation stds: {np.sqrt(obs_tracker.var)}")
# Ideal: means near 0, stds near 1 (after normalization)
```

### 4. Dict Observation Space Pattern

**Pattern**: Structured observations with named components.

**Template**:
```python
from gymnasium import spaces

# Define Dict observation space
observation_space = spaces.Dict({
    'lidar': spaces.Box(
        low=0.0,
        high=10.0,  # Max lidar range
        shape=(60,),  # 60 lidar rays
        dtype=np.float32
    ),
    'goal': spaces.Box(
        low=-np.inf,
        high=np.inf,
        shape=(3,),  # [distance, bearing, heading]
        dtype=np.float32
    ),
    'robot_state': spaces.Box(
        low=-np.inf,
        high=np.inf,
        shape=(4,),  # [x, y, theta, is_carrying]
        dtype=np.float32
    ),
    'velocity': spaces.Box(
        low=-2.0,
        high=2.0,
        shape=(3,),  # [vx, vy, omega]
        dtype=np.float32
    )
})

# Return dict observations
def reset(self):
    return {
        'lidar': np.zeros(60, dtype=np.float32),
        'goal': np.zeros(3, dtype=np.float32),
        'robot_state': np.zeros(4, dtype=np.float32),
        'velocity': np.zeros(3, dtype=np.float32)
    }, {}
```

**With VecNormalize**:
```python
# Normalize only specific keys
normalized_env = VecNormalize(
    vec_env,
    norm_obs=True,
    norm_obs_keys=['lidar', 'goal', 'velocity'],  # Don't normalize robot_state
    clip_obs=10.0
)
```

### 5. Ego-Centric Coordinate Transformation

**Pattern**: Transform world coordinates to robot-centric frame (from Warehouser's `buildV3`).

**Implementation** (from `observation_builder.cpp` lines 119-133):
```cpp
// Transform other robots to ego-centric frame
float cos_ego = std::cos(-ego->theta);  // Negative for inverse rotation
float sin_ego = std::sin(-ego->theta);

for (auto* other : other_robots) {
    // World-frame delta
    float world_dx = other->x - ego->x;
    float world_dy = other->y - ego->y;

    // Rotate to ego's frame (2D rotation matrix)
    float ego_dx = cos_ego * world_dx - sin_ego * world_dy;
    float ego_dy = sin_ego * world_dx + cos_ego * world_dy;

    // Relative heading
    float rel_theta = normalizeAngle(other->theta - ego->theta);
}
```

**Python equivalent**:
```python
def to_ego_frame(
    ego_x: float, ego_y: float, ego_theta: float,
    world_x: float, world_y: float, world_theta: float
) -> tuple[float, float, float]:
    """Transform world coordinates to ego-centric frame.

    Args:
        ego_x, ego_y, ego_theta: Ego robot pose
        world_x, world_y, world_theta: Other entity pose in world frame

    Returns:
        (ego_dx, ego_dy, ego_dtheta): Relative pose in ego frame
    """
    # Translation
    world_dx = world_x - ego_x
    world_dy = world_y - ego_y

    # Rotation (inverse: -theta)
    cos_ego = np.cos(-ego_theta)
    sin_ego = np.sin(-ego_theta)
    ego_dx = cos_ego * world_dx - sin_ego * world_dy
    ego_dy = sin_ego * world_dx + cos_ego * world_dy

    # Relative heading
    ego_dtheta = normalize_angle(world_theta - ego_theta)

    return ego_dx, ego_dy, ego_dtheta

def normalize_angle(angle: float) -> float:
    """Normalize angle to [-pi, pi]."""
    return np.arctan2(np.sin(angle), np.cos(angle))
```

### 6. Goal Encoding Pattern

**Pattern**: Domain-invariant goal representation (from S.md research).

**Best Practice** (lines 61-76 of `observation_builder.cpp`):
```cpp
// Current Warehouser V1 implementation
float dx = goal.x - robot.x;  // Relative displacement
float dy = goal.y - robot.y;
float dist = std::sqrt(dx * dx + dy * dy);  // Euclidean distance
float world_angle = std::atan2(dy, dx);  // Angle in world frame
float heading = normalizeAngle(world_angle - robot.theta);  // Ego-centric bearing
```

**Recommended encoding** (from research - ego-centric only):
```python
def encode_goal(
    robot_x: float, robot_y: float, robot_theta: float,
    goal_x: float, goal_y: float
) -> np.ndarray:
    """Encode goal in ego-centric frame (domain-invariant).

    Returns:
        [distance, bearing] - 2D vector (NOT absolute positions!)
    """
    # Relative position
    dx = goal_x - robot_x
    dy = goal_y - robot_y

    # Distance (scale-invariant)
    distance = np.sqrt(dx * dx + dy * dy)

    # Bearing in ego frame (rotation-invariant)
    world_angle = np.arctan2(dy, dx)
    bearing = normalize_angle(world_angle - robot_theta)

    return np.array([distance, bearing], dtype=np.float32)

# DO NOT include:
# - Absolute goal position (goal_x, goal_y) - not ego-centric
# - World-frame angle - not rotation-invariant
# - dx, dy in world frame - use ego frame instead
```

### 7. Lidar Encoding Pattern

**Pattern**: Discretized range measurements (current Warehouser V2 approach).

**Current V2 specification** (from `observation_builder.hpp` line 18-20):
```cpp
// V2_Lidar: 60 lidar rays + goal (2) + is_carrying (1) = 63 dims
// Lidar rays: discretized range measurements
```

**Implementation template**:
```python
def encode_lidar(
    ranges: np.ndarray,  # Raw lidar ranges
    max_range: float = 10.0,
    num_bins: int = 60
) -> np.ndarray:
    """Encode lidar readings for RL observation.

    Args:
        ranges: Raw lidar range measurements (variable length)
        max_range: Maximum lidar range
        num_bins: Number of discretized bins

    Returns:
        Processed lidar observation (num_bins,)
    """
    # Clip to max range
    ranges = np.clip(ranges, 0.0, max_range)

    # Normalize to [0, 1]
    normalized = ranges / max_range

    # Downsample if needed (e.g., 360 rays -> 60 bins)
    if len(ranges) != num_bins:
        indices = np.linspace(0, len(ranges) - 1, num_bins, dtype=int)
        binned = normalized[indices]
    else:
        binned = normalized

    return binned.astype(np.float32)

# Alternative: Use min pooling for downsampling (more conservative)
def encode_lidar_min_pool(
    ranges: np.ndarray,
    max_range: float = 10.0,
    num_bins: int = 60
) -> np.ndarray:
    """Encode lidar with min-pooling (conservative obstacle detection)."""
    ranges = np.clip(ranges, 0.0, max_range)
    normalized = ranges / max_range

    # Min-pool (closest obstacle in each bin)
    bin_size = len(ranges) // num_bins
    binned = np.array([
        np.min(normalized[i * bin_size:(i + 1) * bin_size])
        for i in range(num_bins)
    ], dtype=np.float32)

    return binned
```

### 8. Domain Randomization for Observations

**Pattern**: Add sensor noise during training for sim-to-real robustness.

**Template**:
```python
class NoisyObservationWrapper(gym.ObservationWrapper):
    """Add sensor noise to observations for domain randomization."""

    def __init__(
        self,
        env: gym.Env,
        lidar_noise_std: float = 0.02,  # 2% of max range
        position_noise_std: float = 0.05,  # 5cm
        angle_noise_std: float = 0.05,  # ~3 degrees
        dropout_prob: float = 0.01  # 1% ray dropout
    ):
        super().__init__(env)
        self.lidar_noise_std = lidar_noise_std
        self.position_noise_std = position_noise_std
        self.angle_noise_std = angle_noise_std
        self.dropout_prob = dropout_prob

    def observation(self, obs: dict) -> dict:
        """Add noise to observation."""
        noisy_obs = obs.copy()

        # Lidar noise
        if 'lidar' in obs:
            lidar = obs['lidar']
            # Gaussian noise
            noise = np.random.normal(0, self.lidar_noise_std, lidar.shape)
            # Random dropout (set to max range)
            dropout_mask = np.random.rand(len(lidar)) < self.dropout_prob
            noisy_lidar = lidar + noise
            noisy_lidar[dropout_mask] = 1.0  # Max range (normalized)
            noisy_obs['lidar'] = np.clip(noisy_lidar, 0.0, 1.0).astype(np.float32)

        # Position noise
        if 'robot_state' in obs:
            state = obs['robot_state'].copy()
            state[0] += np.random.normal(0, self.position_noise_std)  # x
            state[1] += np.random.normal(0, self.position_noise_std)  # y
            state[2] += np.random.normal(0, self.angle_noise_std)  # theta
            noisy_obs['robot_state'] = state.astype(np.float32)

        return noisy_obs
```

### 9. Complete Training Pipeline Template

**Pattern**: Combine all wrappers for robust training.

```python
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize, VecFrameStack

def make_env(config: EnvConfig, add_noise: bool = True):
    """Factory function for creating environments."""
    env = ROSGymEnv(config)

    # Add domain randomization during training
    if add_noise:
        env = NoisyObservationWrapper(
            env,
            lidar_noise_std=0.02,
            position_noise_std=0.05,
            angle_noise_std=0.05,
            dropout_prob=0.01
        )

    return env

# Training setup
config = EnvConfig()

# Create vectorized environment (4 parallel envs)
vec_env = DummyVecEnv([lambda: make_env(config, add_noise=True) for _ in range(4)])

# Add frame stacking (optional - for velocity inference)
# vec_env = VecFrameStack(vec_env, n_stack=4)

# Add normalization (REQUIRED)
vec_env = VecNormalize(
    vec_env,
    training=True,
    norm_obs=True,
    norm_reward=True,
    clip_obs=10.0,
    clip_reward=10.0,
    gamma=0.99
)

# Train
model = PPO(
    "MlpPolicy",
    vec_env,
    learning_rate=3e-4,
    n_steps=2048,
    batch_size=64,
    n_epochs=10,
    gamma=0.99,
    verbose=1
)

model.learn(total_timesteps=1_000_000)

# Save model AND normalization statistics
model.save("ppo_warehouser")
vec_env.save("vec_normalize.pkl")

# Evaluation setup (NO noise, NO training mode)
eval_env = DummyVecEnv([lambda: make_env(config, add_noise=False)])
eval_env = VecNormalize.load("vec_normalize.pkl", eval_env)
eval_env.training = False
eval_env.norm_reward = False

# Load and evaluate
model = PPO.load("ppo_warehouser", env=eval_env)
```

## Application to Warehouser

### Current State Analysis

**Warehouser V1** (lines 42-81 of `observation_builder.cpp`):
- Uses ground truth positions (privileged information)
- NOT suitable for sim-to-real transfer
- Includes absolute coordinates (x, y, theta) - not ego-centric

**Warehouser V2** (line 18-20 of `observation_builder.hpp`):
- Lidar-based (good for sim-to-real)
- 60 discretized lidar bins
- Ego-centric goal encoding (bearing, distance)
- Missing: velocity, domain randomization, normalization

**Warehouser V3** (lines 84-138 of `observation_builder.cpp`):
- Multi-robot observations
- Ego-centric frame transformation (CORRECT implementation)
- Still includes privileged information (absolute positions in first 8 dims)

### Recommended Changes

1. **Deprecate V1** - Contains privileged information
2. **Fix V3** - Remove absolute positions from ego state (lines 99-102)
3. **Add VecNormalize** - Required for V2/V3
4. **Add domain randomization** - Sensor noise wrapper
5. **Consider Dict observation space** - For clearer structure

### Proposed V4 Observation Space

```python
# V4_EgoCentric - Fully ego-centric, no privileged information
observation_space = spaces.Dict({
    'lidar': spaces.Box(
        low=0.0,
        high=1.0,  # Normalized range [0, 1]
        shape=(60,),
        dtype=np.float32
    ),
    'goal': spaces.Box(
        low=np.array([-np.inf, -np.pi], dtype=np.float32),
        high=np.array([np.inf, np.pi], dtype=np.float32),
        shape=(2,),  # [distance, bearing]
        dtype=np.float32
    ),
    'velocity': spaces.Box(
        low=np.array([-2.0, -2.0, -3.0], dtype=np.float32),
        high=np.array([2.0, 2.0, 3.0], dtype=np.float32),
        shape=(3,),  # [vx, vy, omega] in ego frame
        dtype=np.float32
    ),
    'carrying': spaces.Box(
        low=0.0,
        high=1.0,
        shape=(1,),
        dtype=np.float32
    ),
    'other_robots': spaces.Box(
        low=-np.inf,
        high=np.inf,
        shape=(3 * 3,),  # Max 3 robots: [rel_x, rel_y, rel_theta] each
        dtype=np.float32
    )
})

# Total dims: 60 + 2 + 3 + 1 + 9 = 75
# All ego-centric, no privileged information, suitable for sim-to-real
```

## Key Takeaways

1. **Always normalize observations** - Use VecNormalize with saved statistics
2. **Ego-centric > World-centric** - Better sim-to-real transfer
3. **Domain randomization** - Add sensor noise during training
4. **Frame stacking OR explicit velocity** - Temporal information improves learning
5. **Dict spaces** - More structured, easier to debug, selective normalization
6. **No privileged information** - Only sensor-realistic observations
7. **Save normalization stats** - Critical for deployment

## References

- VecNormalize: `stable_baselines3/common/vec_env/vec_normalize.py`
- StackedObservations: `stable_baselines3/common/vec_env/stacked_observations.py`
- RunningMeanStd: `stable_baselines3/common/running_mean_std.py`
- Warehouser ObservationBuilder: `ros_ws/src/warehouser_observations/`
- Research findings: `.delegate/study/20260212-221301-observation-space-design/S.md`
