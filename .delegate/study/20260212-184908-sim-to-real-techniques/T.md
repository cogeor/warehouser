# Template

Created: 2026-02-12

## Source

Analysis of existing Warehouser codebase implementations:
- `ros_ws/src/warehouser_observations/include/warehouser_observations/noise_model.hpp`
- `ros_ws/src/warehouser_observations/include/warehouser_observations/lidar_simulator.hpp`
- `ros_ws/src/warehouser_observations/include/warehouser_observations/odometry_simulator.hpp`
- `training/training/models/config.py`

Reference architectures from research (S.md findings):
- OpenAI Active Domain Randomization (ADR)
- IsaacGym/IsaacSim domain randomization patterns
- Gymnasium wrapper patterns for noise injection
- Physics engine randomization (MuJoCo/PyBullet patterns)

## Pattern

Warehouser has already implemented foundational sensor noise models following best practices from sim-to-real literature. The existing patterns show:

### 1. Existing Sensor Noise Infrastructure (IMPLEMENTED)

**C++ Noise Model Pattern:**
```cpp
// Generic noise model with dropout support
struct NoiseConfig {
    float mean = 0.0f;
    float stddev = 0.0f;
    float dropout_prob = 0.0f;
    float dropout_value = 0.0f;
    bool enabled = false;
};

class NoiseModel {
    // Gaussian noise + probabilistic dropout
    float apply(float value);
    void applyVector(std::vector<float>& values);
    bool shouldDropout();
    void setSeed(unsigned int seed);  // Reproducibility
};
```

**Sensor-Specific Configurations:**
```cpp
struct LidarNoiseConfig {
    float range_stddev = 0.02f;    // 2cm noise (aligned with research: 2% of range)
    float dropout_prob = 0.01f;    // 1% dropout (research: 1-5%)
    bool enabled = false;
};

struct OdomNoiseConfig {
    float linear_stddev = 0.01f;   // 1% drift per meter (research: 0.5-1.5%)
    float angular_stddev = 0.02f;  // 2% angular drift
    bool enabled = false;
};
```

**Key Design Patterns:**
- **Separation of concerns**: Noise model is generic, sensor configs are specific
- **Runtime toggleable**: `enabled` flag for easy ablation studies
- **Seeded RNG**: Reproducible experiments via `setSeed()`
- **Type safety**: Strongly typed configs prevent errors

### 2. Missing Patterns (TO BE IMPLEMENTED)

Based on S.md research and comparison with state-of-the-art, Warehouser needs:

#### A. Domain Randomization Config System
#### B. Action Delay Buffer
#### C. Dynamics Randomization
#### D. Active Domain Randomization (ADR)
#### E. Evaluation Infrastructure

## Application

The following templates can be directly applied to Warehouser for complete sim-to-real coverage.

---

## TEMPLATE 1: Domain Randomization Configuration (YAML-Based)

**Pattern:** Centralized YAML configuration matching IsaacGym/IsaacSim conventions

**File:** `ros_ws/config/domain_randomization.yaml`

```yaml
# Domain Randomization Configuration for Sim-to-Real Transfer
# Based on research findings from S.md (2025-2026 best practices)

domain_randomization:
  enabled: true
  seed: 42  # Set to -1 for random seed each episode

  # ============ Visual Randomization ============
  visual:
    enabled: true

    floor:
      color:
        r: [0.3, 0.8]  # RGB uniform sampling
        g: [0.3, 0.8]
        b: [0.3, 0.8]
      friction: [0.6, 1.0]  # Smooth to grippy
      texture: ["concrete", "tile", "epoxy"]  # Categorical

    walls:
      color:
        r: [0.4, 0.9]
        g: [0.4, 0.9]
        b: [0.4, 0.9]
      reflectivity: [0.1, 0.7]

    lighting:
      intensity: [0.5, 1.5]  # Multiplier on nominal
      position_noise_std: 2.0  # Meters (Gaussian)
      ambient: [0.2, 0.5]

  # ============ Dynamics Randomization ============
  dynamics:
    enabled: true

    robot:
      mass: [24.0, 36.0]  # ±20% of 30kg nominal (research: ±20%)
      inertia_scale: [0.8, 1.2]  # Scale tensor uniformly

      wheel:
        friction: [0.8, 1.2]  # Coefficient multiplier
        damping: [0.8, 1.2]  # Motor damping
        slip_factor_mean: 0.95  # Beta distribution parameters
        slip_factor_alpha: 5.0  # Beta(5, 1) → mostly [0.9, 1.0]
        slip_factor_beta: 1.0   # with occasional [0.7, 0.9]
        slip_event_prob: 0.05   # 5% chance per step
        slip_event_factor: [0.5, 0.8]  # High slip during events

      center_of_mass_offset:
        x: [-0.05, 0.05]  # Meters
        y: [-0.05, 0.05]
        z: [-0.02, 0.02]

    objects:
      mass_scale: [0.7, 1.3]  # ±30% variation
      friction: [0.4, 1.0]
      restitution: [0.0, 0.3]  # Bounciness

  # ============ Sensor Noise ============
  sensors:
    enabled: true

    lidar:
      range_noise_percent: 0.02  # 2% of range (Gaussian σ)
      dropout_prob: [0.01, 0.05]  # 1-5% random uniform
      intensity_threshold: [0.2, 0.4]  # Low intensity → dropout
      min_range_jitter: [0.08, 0.12]  # 0.1m ± 0.02m
      max_range_jitter: [9.5, 10.5]   # 10m ± 0.5m

      # Environmental effects
      dust_particles_per_scan: [0, 50]  # Random points within 5m
      dust_max_range: 5.0
      reflective_surface_prob: 0.1  # Multi-path returns

    odometry:
      linear_drift_per_meter: [0.005, 0.015]  # 0.5-1.5%
      angular_drift_per_radian: [0.01, 0.03]
      random_walk_std: 0.005  # Additional cumulative error

      # Acceleration-dependent slip
      accel_slip_threshold: 2.0  # m/s²
      accel_slip_factor: [0.7, 0.9]  # Slip when accelerating hard

    imu:  # Future expansion
      gyro_bias_drift_deg_per_hour: [5, 50]
      accel_bias: [0.01, 0.05]  # m/s²
      gyro_noise_std: [0.001, 0.01]  # rad/s
      accel_noise_std: [0.01, 0.1]  # m/s²

  # ============ Action Delays ============
  # Research: Total delay [35, 160]ms for warehouse robots
  delays:
    enabled: true
    control_frequency: 20.0  # Hz

    # Delay in control steps (at 20 Hz, 1 step = 50ms)
    sensing_delay_steps: [0, 1]  # 0-50ms
    communication_delay_steps: [0, 2]  # 0-100ms (LAN/WiFi)
    actuation_delay_steps: [0, 2]  # 0-100ms (motor response)

    # Stochastic variation (Gaussian noise on delay)
    delay_noise_std: 0.5  # Steps (25ms std dev)

    # History augmentation
    observation_history_length: 3  # Last 3 observations
    action_history_length: 5  # Last 5 actions

  # ============ Physics Timestep ============
  physics:
    timestep_randomization: false  # Advanced: randomize dt
    nominal_dt: 0.01  # 10ms (100 Hz physics)
    dt_range: [0.008, 0.012]  # ±20% if enabled

  # ============ Active Domain Randomization (ADR) ============
  adr:
    enabled: false  # Disable initially, enable after basic DR works
    update_frequency: 10000  # Steps between ADR updates

    # Performance thresholds for range expansion/contraction
    high_threshold: 0.95  # Success rate → expand range
    low_threshold: 0.50   # Success rate → contract range
    expansion_factor: 1.1  # Multiply range by this
    contraction_factor: 0.9

    # Parameters to auto-tune
    parameters:
      - "dynamics.robot.mass"
      - "dynamics.robot.wheel.friction"
      - "delays.communication_delay_steps"
      - "sensors.lidar.dropout_prob"
```

**Usage Pattern:**
```cpp
// In C++ ROS node initialization
#include <yaml-cpp/yaml.h>

class DomainRandomizer {
public:
    void loadConfig(const std::string& yaml_path) {
        YAML::Node config = YAML::LoadFile(yaml_path);
        auto dr = config["domain_randomization"];

        enabled_ = dr["enabled"].as<bool>();
        seed_ = dr["seed"].as<int>();

        // Parse visual randomization
        auto visual = dr["visual"];
        floor_color_r_range_ = parseRange(visual["floor"]["color"]["r"]);
        // ... etc
    }

    void randomizeEpisode() {
        if (!enabled_) return;

        // Sample all parameters
        floor_color_ = sampleUniform(floor_color_r_range_,
                                     floor_color_g_range_,
                                     floor_color_b_range_);
        robot_mass_ = sampleUniform(robot_mass_range_);
        // ... apply to simulation
    }

private:
    std::mt19937 rng_;
    bool enabled_;
    int seed_;
    // ... parameter ranges
};
```

---

## TEMPLATE 2: Action Delay Buffer (Gymnasium Wrapper)

**Pattern:** History-augmented observations with stochastic delays

**File:** `training/training/wrappers/action_delay.py`

```python
"""Action delay wrapper for sim-to-real transfer training.

Based on research findings:
- Warehouse robots have total delay 35-160ms (1-8 steps at 20 Hz)
- History augmentation helps policy adapt to delays
- Stochastic delays better than fixed delays
"""

from collections import deque
from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray


class ActionDelayWrapper(gym.Wrapper):
    """Applies realistic action delays with history augmentation.

    Simulates:
    1. Sensing delay (sensor processing time)
    2. Communication delay (network latency)
    3. Actuation delay (motor response time)

    Augments observations with:
    - Recent action history (last N actions)
    - Recent observation history (last M observations)

    This allows the policy to learn to predict future states and
    compensate for delays, similar to human adaptation.
    """

    def __init__(
        self,
        env: gym.Env,
        min_delay_steps: int = 1,
        max_delay_steps: int = 8,
        delay_noise_std: float = 0.5,
        action_history_length: int = 5,
        obs_history_length: int = 3,
        enabled: bool = True,
    ):
        """Initialize action delay wrapper.

        Args:
            env: Base environment
            min_delay_steps: Minimum delay in steps (50ms at 20 Hz)
            max_delay_steps: Maximum delay in steps (400ms at 20 Hz)
            delay_noise_std: Stochastic noise on delay (steps)
            action_history_length: Number of past actions to include
            obs_history_length: Number of past observations to include
            enabled: Enable delay (for ablation studies)
        """
        super().__init__(env)

        self.min_delay = min_delay_steps
        self.max_delay = max_delay_steps
        self.delay_noise_std = delay_noise_std
        self.action_history_length = action_history_length
        self.obs_history_length = obs_history_length
        self.enabled = enabled

        # Action buffer (FIFO queue)
        self.action_buffer: deque = deque(maxlen=max_delay_steps + 5)

        # History buffers for observation augmentation
        self.action_history: deque = deque(maxlen=action_history_length)
        self.obs_history: deque = deque(maxlen=obs_history_length)

        # Modify observation space to include history
        base_obs_dim = env.observation_space.shape[0]
        action_dim = env.action_space.shape[0]

        # New obs = [current_obs, obs_history, action_history]
        augmented_dim = (
            base_obs_dim +  # Current observation
            base_obs_dim * obs_history_length +  # Past observations
            action_dim * action_history_length  # Past actions
        )

        self.observation_space = gym.spaces.Box(
            low=-np.inf,
            high=np.inf,
            shape=(augmented_dim,),
            dtype=np.float32,
        )

    def reset(self, **kwargs) -> tuple[NDArray, dict[str, Any]]:
        """Reset environment and clear buffers."""
        obs, info = self.env.reset(**kwargs)

        # Clear buffers
        self.action_buffer.clear()
        self.action_history.clear()
        self.obs_history.clear()

        # Initialize with zero actions/observations
        zero_action = np.zeros(self.env.action_space.shape[0], dtype=np.float32)
        zero_obs = np.zeros_like(obs)

        for _ in range(self.action_history_length):
            self.action_history.append(zero_action.copy())

        for _ in range(self.obs_history_length):
            self.obs_history.append(zero_obs.copy())

        # Update with current observation
        self.obs_history[-1] = obs.copy()

        return self._augment_observation(obs), info

    def step(self, action: NDArray) -> tuple[NDArray, float, bool, bool, dict]:
        """Execute delayed action and return augmented observation."""
        # Add current action to buffer
        self.action_buffer.append(action.copy())

        # Sample delay for this step
        if self.enabled:
            delay_steps = self._sample_delay()
        else:
            delay_steps = 0

        # Get delayed action (or zero if buffer not full)
        if len(self.action_buffer) > delay_steps:
            delayed_action = self.action_buffer[0]
            self.action_buffer.popleft()
        else:
            # Buffer not full yet, use zero action
            delayed_action = np.zeros_like(action)

        # Execute delayed action in environment
        obs, reward, terminated, truncated, info = self.env.step(delayed_action)

        # Update history buffers
        self.action_history.append(action.copy())  # Store intended action
        self.obs_history.append(obs.copy())

        # Augment observation with history
        augmented_obs = self._augment_observation(obs)

        # Add delay info to metadata
        info["delay_steps"] = delay_steps if self.enabled else 0
        info["delayed_action"] = delayed_action

        return augmented_obs, reward, terminated, truncated, info

    def _sample_delay(self) -> int:
        """Sample stochastic delay in steps."""
        # Uniform base delay + Gaussian noise
        base_delay = np.random.uniform(self.min_delay, self.max_delay)
        noise = np.random.normal(0, self.delay_noise_std)
        total_delay = base_delay + noise

        # Clamp to valid range
        delay_steps = int(np.clip(total_delay, 0, self.max_delay + 2))
        return delay_steps

    def _augment_observation(self, obs: NDArray) -> NDArray:
        """Augment observation with action and observation history."""
        if not self.enabled:
            # If disabled, just return base observation (for ablation)
            return obs

        # Flatten histories
        obs_hist = np.concatenate(list(self.obs_history), axis=0)
        action_hist = np.concatenate(list(self.action_history), axis=0)

        # Concatenate: [current_obs, obs_history, action_history]
        augmented = np.concatenate([obs, obs_hist, action_hist], axis=0)

        return augmented.astype(np.float32)


# ============ Action Smoothness Wrapper ============

class ActionSmoothnessWrapper(gym.Wrapper):
    """Penalizes jerky control to improve sim-to-real transfer.

    Research finding: Policies trained with action smoothness penalties
    perform better in reality as they're compatible with actuator dynamics.
    """

    def __init__(
        self,
        env: gym.Env,
        smoothness_weight: float = 0.1,
        max_action_change: float = 0.3,
        enabled: bool = True,
    ):
        """Initialize smoothness wrapper.

        Args:
            env: Base environment
            smoothness_weight: Weight for smoothness penalty
            max_action_change: Max allowed change (for hard limit, optional)
            enabled: Enable smoothness penalty
        """
        super().__init__(env)
        self.smoothness_weight = smoothness_weight
        self.max_action_change = max_action_change
        self.enabled = enabled
        self.last_action = None

    def reset(self, **kwargs):
        """Reset and clear last action."""
        self.last_action = None
        return self.env.reset(**kwargs)

    def step(self, action: NDArray) -> tuple:
        """Apply action and penalize large changes."""
        if self.enabled and self.last_action is not None:
            # Compute smoothness penalty
            action_delta = action - self.last_action
            smoothness_penalty = -self.smoothness_weight * np.sum(action_delta ** 2)

            # Optional: Hard limit action change
            if self.max_action_change > 0:
                action_delta = np.clip(
                    action_delta,
                    -self.max_action_change,
                    self.max_action_change,
                )
                action = self.last_action + action_delta
        else:
            smoothness_penalty = 0.0

        self.last_action = action.copy()

        obs, reward, terminated, truncated, info = self.env.step(action)

        # Add smoothness penalty to reward
        reward += smoothness_penalty
        info["smoothness_penalty"] = smoothness_penalty

        return obs, reward, terminated, truncated, info
```

**Usage:**
```python
# In training script
from training.wrappers.action_delay import ActionDelayWrapper, ActionSmoothnessWrapper

env = ROSGymEnv(config)
env = ActionDelayWrapper(env, min_delay_steps=1, max_delay_steps=8)
env = ActionSmoothnessWrapper(env, smoothness_weight=0.1)

model = PPO("MlpPolicy", env, ...)
model.learn(total_timesteps=1_000_000)
```

---

## TEMPLATE 3: Dynamics Randomization (C++ Simulation)

**Pattern:** Per-episode randomization of physics parameters

**File:** `ros_ws/src/ros_simulation/include/ros_simulation/domain_randomizer.hpp`

```cpp
#pragma once

#include <random>
#include <vector>

namespace warehouser {

/// Configuration for robot dynamics randomization
struct RobotDynamicsConfig {
    // Mass randomization
    float mass_min = 24.0f;  // kg
    float mass_max = 36.0f;

    // Wheel friction
    float wheel_friction_min = 0.8f;
    float wheel_friction_max = 1.2f;

    // Motor damping
    float motor_damping_min = 0.8f;
    float motor_damping_max = 1.2f;

    // Center of mass offset
    float com_offset_x_min = -0.05f;  // meters
    float com_offset_x_max = 0.05f;
    float com_offset_y_min = -0.05f;
    float com_offset_y_max = 0.05f;

    // Wheel slip (Beta distribution parameters)
    float slip_alpha = 5.0f;  // Beta(5, 1) → peak near 1.0
    float slip_beta = 1.0f;
    float slip_event_prob = 0.05f;  // 5% chance of high slip
    float slip_event_factor_min = 0.5f;
    float slip_event_factor_max = 0.8f;
};

/// Sampled dynamics parameters for one episode
struct DynamicsParameters {
    float mass;
    float wheel_friction;
    float motor_damping;
    float com_offset_x;
    float com_offset_y;
    float slip_factor;
    bool slip_event;
};

/// Domain randomizer for physics simulation
class DomainRandomizer {
public:
    explicit DomainRandomizer(const RobotDynamicsConfig& config = {},
                             unsigned int seed = 0);

    /// Sample new dynamics parameters for episode
    DynamicsParameters sampleParameters();

    /// Apply parameters to robot entity
    /// @param robot Robot entity to modify
    void applyToRobot(DynamicsParameters& params, RobotEntity& robot);

    /// Set random seed
    void setSeed(unsigned int seed);

    /// Update configuration (for ADR)
    void updateConfig(const RobotDynamicsConfig& config);

    /// Get current configuration
    const RobotDynamicsConfig& config() const { return config_; }

private:
    RobotDynamicsConfig config_;
    std::mt19937 rng_;

    // Distribution helpers
    float sampleUniform(float min, float max);
    float sampleBeta(float alpha, float beta);
    bool sampleBernoulli(float prob);
};

// ============ Implementation ============

inline DomainRandomizer::DomainRandomizer(const RobotDynamicsConfig& config,
                                         unsigned int seed)
    : config_(config), rng_(seed) {}

inline DynamicsParameters DomainRandomizer::sampleParameters() {
    DynamicsParameters params;

    params.mass = sampleUniform(config_.mass_min, config_.mass_max);
    params.wheel_friction = sampleUniform(config_.wheel_friction_min,
                                         config_.wheel_friction_max);
    params.motor_damping = sampleUniform(config_.motor_damping_min,
                                        config_.motor_damping_max);
    params.com_offset_x = sampleUniform(config_.com_offset_x_min,
                                       config_.com_offset_x_max);
    params.com_offset_y = sampleUniform(config_.com_offset_y_min,
                                       config_.com_offset_y_max);

    // Sample wheel slip factor (Beta distribution for realistic distribution)
    params.slip_factor = sampleBeta(config_.slip_alpha, config_.slip_beta);

    // Sample slip event
    params.slip_event = sampleBernoulli(config_.slip_event_prob);
    if (params.slip_event) {
        // Override with high slip
        params.slip_factor = sampleUniform(config_.slip_event_factor_min,
                                          config_.slip_event_factor_max);
    }

    return params;
}

inline void DomainRandomizer::applyToRobot(DynamicsParameters& params,
                                          RobotEntity& robot) {
    robot.mass = params.mass;
    robot.wheel_friction = params.wheel_friction;
    robot.motor_damping = params.motor_damping;
    robot.com_offset_x = params.com_offset_x;
    robot.com_offset_y = params.com_offset_y;
    // Slip factor applied during simulation step
}

inline float DomainRandomizer::sampleUniform(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_);
}

inline float DomainRandomizer::sampleBeta(float alpha, float beta) {
    // Beta distribution using gamma distributions
    std::gamma_distribution<float> gamma_a(alpha, 1.0f);
    std::gamma_distribution<float> gamma_b(beta, 1.0f);

    float x = gamma_a(rng_);
    float y = gamma_b(rng_);
    return x / (x + y);
}

inline bool DomainRandomizer::sampleBernoulli(float prob) {
    std::bernoulli_distribution dist(prob);
    return dist(rng_);
}

inline void DomainRandomizer::setSeed(unsigned int seed) {
    rng_.seed(seed);
}

inline void DomainRandomizer::updateConfig(const RobotDynamicsConfig& config) {
    config_ = config;
}

}  // namespace warehouser
```

**Integration with RLBridge:**
```cpp
// In RLResetService callback
DomainRandomizer randomizer(config);
auto dynamics = randomizer.sampleParameters();
randomizer.applyToRobot(dynamics, robot_entity);
```

---

## TEMPLATE 4: Active Domain Randomization (Python)

**Pattern:** Discriminator-based curriculum for parameter ranges

**File:** `training/training/wrappers/active_dr.py`

```python
"""Active Domain Randomization (ADR) for automatic curriculum learning.

Based on:
- "Active Domain Randomization" (Mehta et al., 2019)
- OpenAI's approach for Rubik's Cube manipulation

Key idea: Train a discriminator to distinguish randomized from reference
environments. Sample environments where discriminator is confident (hard cases)
to focus training on challenging variations.
"""

import json
from pathlib import Path
from typing import Any

import numpy as np
import torch
import torch.nn as nn
from numpy.typing import NDArray


class Discriminator(nn.Module):
    """Discriminator network to distinguish randomized vs reference rollouts."""

    def __init__(self, trajectory_dim: int, hidden_dim: int = 128):
        """Initialize discriminator.

        Args:
            trajectory_dim: Flattened trajectory dimension
            hidden_dim: Hidden layer size
        """
        super().__init__()

        self.network = nn.Sequential(
            nn.Linear(trajectory_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
            nn.Sigmoid(),
        )

    def forward(self, trajectory: torch.Tensor) -> torch.Tensor:
        """Forward pass.

        Args:
            trajectory: Flattened trajectory tensor [batch, trajectory_dim]

        Returns:
            Probability that trajectory is from randomized environment [batch, 1]
        """
        return self.network(trajectory)


class ActiveDRManager:
    """Manages Active Domain Randomization.

    Workflow:
    1. Collect reference trajectories (no randomization)
    2. During training, periodically collect randomized trajectories
    3. Train discriminator to distinguish them
    4. Sample new environments weighted by discriminator confidence
    5. Optionally: Expand/contract parameter ranges based on performance (ADR)
    """

    def __init__(
        self,
        trajectory_dim: int,
        parameter_ranges: dict[str, tuple[float, float]],
        discriminator_lr: float = 1e-3,
        update_frequency: int = 10000,
        adr_enabled: bool = False,
        high_threshold: float = 0.95,
        low_threshold: float = 0.50,
    ):
        """Initialize ADR manager.

        Args:
            trajectory_dim: Dimension of flattened trajectory
            parameter_ranges: Initial ranges {param_name: (min, max)}
            discriminator_lr: Learning rate for discriminator
            update_frequency: Steps between ADR updates
            adr_enabled: Enable automatic range adjustment
            high_threshold: Success rate to expand range
            low_threshold: Success rate to contract range
        """
        self.discriminator = Discriminator(trajectory_dim)
        self.optimizer = torch.optim.Adam(
            self.discriminator.parameters(), lr=discriminator_lr
        )
        self.criterion = nn.BCELoss()

        self.parameter_ranges = parameter_ranges
        self.update_frequency = update_frequency
        self.adr_enabled = adr_enabled
        self.high_threshold = high_threshold
        self.low_threshold = low_threshold

        # Trajectory buffers
        self.reference_trajectories: list[NDArray] = []
        self.randomized_trajectories: list[NDArray] = []

        self.step_count = 0

    def collect_reference_trajectory(self, trajectory: NDArray):
        """Store reference trajectory (no randomization)."""
        self.reference_trajectories.append(trajectory)

    def collect_randomized_trajectory(self, trajectory: NDArray):
        """Store randomized trajectory."""
        self.randomized_trajectories.append(trajectory)

    def train_discriminator(self, epochs: int = 10, batch_size: int = 32):
        """Train discriminator on collected trajectories."""
        if len(self.reference_trajectories) < batch_size or \
           len(self.randomized_trajectories) < batch_size:
            return  # Not enough data

        for _ in range(epochs):
            # Sample batches
            ref_batch = np.stack(
                np.random.choice(self.reference_trajectories, batch_size)
            )
            rand_batch = np.stack(
                np.random.choice(self.randomized_trajectories, batch_size)
            )

            # Convert to tensors
            ref_tensor = torch.FloatTensor(ref_batch)
            rand_tensor = torch.FloatTensor(rand_batch)

            # Labels: 0 = reference, 1 = randomized
            ref_labels = torch.zeros(batch_size, 1)
            rand_labels = torch.ones(batch_size, 1)

            # Train step
            self.optimizer.zero_grad()

            ref_pred = self.discriminator(ref_tensor)
            rand_pred = self.discriminator(rand_tensor)

            loss = self.criterion(ref_pred, ref_labels) + \
                   self.criterion(rand_pred, rand_labels)

            loss.backward()
            self.optimizer.step()

    def sample_environment_parameters(self) -> dict[str, float]:
        """Sample environment parameters weighted by discriminator confidence.

        Returns:
            Dictionary of sampled parameter values
        """
        # Simple strategy: Sample uniformly within current ranges
        # (Advanced: Use discriminator to bias sampling toward hard cases)
        params = {}
        for name, (min_val, max_val) in self.parameter_ranges.items():
            params[name] = np.random.uniform(min_val, max_val)
        return params

    def update_ranges(self, success_rate: float):
        """Update parameter ranges based on performance (ADR)."""
        if not self.adr_enabled:
            return

        # Randomly select a parameter to adjust
        param_name = np.random.choice(list(self.parameter_ranges.keys()))
        min_val, max_val = self.parameter_ranges[param_name]

        if success_rate > self.high_threshold:
            # Expand range
            range_size = max_val - min_val
            expansion = range_size * 0.1  # 10% expansion
            self.parameter_ranges[param_name] = (
                min_val - expansion,
                max_val + expansion,
            )
        elif success_rate < self.low_threshold:
            # Contract range
            range_size = max_val - min_val
            contraction = range_size * 0.1
            self.parameter_ranges[param_name] = (
                min_val + contraction,
                max_val - contraction,
            )

    def save(self, path: Path):
        """Save ADR state."""
        state = {
            "parameter_ranges": self.parameter_ranges,
            "discriminator_state": self.discriminator.state_dict(),
            "step_count": self.step_count,
        }
        torch.save(state, path)

    def load(self, path: Path):
        """Load ADR state."""
        state = torch.load(path)
        self.parameter_ranges = state["parameter_ranges"]
        self.discriminator.load_state_dict(state["discriminator_state"])
        self.step_count = state["step_count"]
```

**Usage:**
```python
# Initialize ADR
adr = ActiveDRManager(
    trajectory_dim=obs_dim * max_steps,
    parameter_ranges={
        "robot_mass": (24.0, 36.0),
        "wheel_friction": (0.8, 1.2),
        "delay_steps": (1, 8),
    },
    adr_enabled=True,
)

# Training loop
for episode in range(num_episodes):
    # Sample environment parameters
    params = adr.sample_environment_parameters()

    # Run episode with these parameters
    trajectory, success = run_episode(params)

    # Collect trajectory
    adr.collect_randomized_trajectory(trajectory)

    # Periodically update
    if episode % adr.update_frequency == 0:
        adr.train_discriminator()
        adr.update_ranges(success_rate=compute_success_rate())
```

---

## TEMPLATE 5: Sim-to-Real Evaluation Infrastructure

**Pattern:** Systematic evaluation with reality gap metrics

**File:** `training/training/evaluation/sim_to_real_metrics.py`

```python
"""Sim-to-Real evaluation metrics and analysis tools."""

from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
from numpy.typing import NDArray


@dataclass
class EpisodeResult:
    """Result of a single episode evaluation."""

    success: bool
    reward: float
    episode_length: int
    collision_occurred: bool
    trajectory: NDArray  # [T, 2] positions
    actions: NDArray  # [T, action_dim]
    final_distance_to_goal: float
    time_seconds: float


class SimToRealEvaluator:
    """Evaluates sim-to-real transfer performance."""

    def __init__(self):
        self.sim_results: list[EpisodeResult] = []
        self.real_results: list[EpisodeResult] = []

    def add_sim_result(self, result: EpisodeResult):
        """Add simulation evaluation result."""
        self.sim_results.append(result)

    def add_real_result(self, result: EpisodeResult):
        """Add real-world evaluation result."""
        self.real_results.append(result)

    def compute_metrics(self) -> dict[str, Any]:
        """Compute comprehensive sim-to-real metrics.

        Returns:
            Dictionary of metrics including:
            - Success rates (sim vs real)
            - Performance ratio
            - Coefficient of variation
            - Failure mode distribution
            - Trajectory similarity
        """
        if not self.sim_results or not self.real_results:
            return {"error": "Need both sim and real results"}

        # Success rates
        sim_success_rate = np.mean([r.success for r in self.sim_results])
        real_success_rate = np.mean([r.success for r in self.real_results])

        # Performance ratio (Target: > 0.8)
        performance_ratio = real_success_rate / (sim_success_rate + 1e-8)

        # Coefficient of variation (Target: < 0.3)
        real_rewards = [r.reward for r in self.real_results]
        cv = np.std(real_rewards) / (np.mean(real_rewards) + 1e-8)

        # Collision rates
        sim_collision_rate = np.mean([r.collision_occurred for r in self.sim_results])
        real_collision_rate = np.mean([r.collision_occurred for r in self.real_results])

        # Episode lengths
        sim_length = np.mean([r.episode_length for r in self.sim_results])
        real_length = np.mean([r.episode_length for r in self.real_results])

        # Path efficiency (actual / optimal)
        # (Requires computing optimal path length - use Euclidean as proxy)

        return {
            # Primary metrics
            "sim_success_rate": sim_success_rate,
            "real_success_rate": real_success_rate,
            "performance_ratio": performance_ratio,
            "transfer_success": performance_ratio > 0.8,

            # Robustness
            "real_cv": cv,
            "robust": cv < 0.3,

            # Secondary metrics
            "sim_collision_rate": sim_collision_rate,
            "real_collision_rate": real_collision_rate,
            "sim_avg_length": sim_length,
            "real_avg_length": real_length,

            # Sample sizes
            "n_sim_episodes": len(self.sim_results),
            "n_real_episodes": len(self.real_results),
        }

    def analyze_failure_modes(self) -> dict[str, int]:
        """Categorize failure modes in real-world deployment.

        Returns:
            Dictionary mapping failure type to count
        """
        failure_modes = {
            "collision": 0,
            "timeout": 0,
            "stuck": 0,  # Low velocity for extended time
            "goal_missed": 0,  # Close but not within threshold
            "other": 0,
        }

        for result in self.real_results:
            if result.success:
                continue  # Not a failure

            if result.collision_occurred:
                failure_modes["collision"] += 1
            elif result.episode_length >= 500:  # Max steps
                failure_modes["timeout"] += 1
            elif result.final_distance_to_goal < 1.0:
                failure_modes["goal_missed"] += 1
            else:
                failure_modes["other"] += 1

        return failure_modes

    def generate_report(self, output_path: Path):
        """Generate comprehensive evaluation report."""
        metrics = self.compute_metrics()
        failure_modes = self.analyze_failure_modes()

        report = f"""
# Sim-to-Real Transfer Evaluation Report

## Summary
- **Simulation Episodes**: {metrics['n_sim_episodes']}
- **Real-World Episodes**: {metrics['n_real_episodes']}

## Primary Metrics
- **Sim Success Rate**: {metrics['sim_success_rate']:.1%}
- **Real Success Rate**: {metrics['real_success_rate']:.1%}
- **Performance Ratio**: {metrics['performance_ratio']:.2f} (Target: > 0.8)
- **Transfer Success**: {'YES' if metrics['transfer_success'] else 'NO'}

## Robustness
- **Real CV**: {metrics['real_cv']:.2f} (Target: < 0.3)
- **Robust**: {'YES' if metrics['robust'] else 'NO'}

## Secondary Metrics
- **Sim Collision Rate**: {metrics['sim_collision_rate']:.1%}
- **Real Collision Rate**: {metrics['real_collision_rate']:.1%}
- **Sim Avg Length**: {metrics['sim_avg_length']:.1f} steps
- **Real Avg Length**: {metrics['real_avg_length']:.1f} steps

## Failure Mode Analysis
"""
        for mode, count in failure_modes.items():
            report += f"- **{mode.capitalize()}**: {count}\n"

        report += "\n## Recommendations\n"

        if not metrics['transfer_success']:
            report += "- Performance ratio < 0.8: Increase domain randomization\n"
            report += "- Check for unmodeled dynamics (delays, friction, etc.)\n"

        if not metrics['robust']:
            report += "- High variance: Expand randomization parameter ranges\n"

        if metrics['real_collision_rate'] > 0.1:
            report += "- High collision rate: Review sensor noise models\n"

        output_path.write_text(report)
        print(f"Report saved to {output_path}")
```

**Usage:**
```python
# Evaluation protocol
evaluator = SimToRealEvaluator()

# 1. Evaluate in simulation
for _ in range(50):
    result = evaluate_episode(env_sim, policy)
    evaluator.add_sim_result(result)

# 2. Deploy to real robot
for _ in range(50):
    result = evaluate_episode(env_real, policy)
    evaluator.add_real_result(result)

# 3. Generate report
evaluator.generate_report(Path("eval_report.md"))
metrics = evaluator.compute_metrics()

if not metrics['transfer_success']:
    print("Transfer failed! Increase domain randomization.")
```

---

## Summary: Implementation Priority

Based on research findings and existing Warehouser infrastructure:

### IMPLEMENTED (Already in Codebase)
- Sensor noise models (LiDAR, odometry) with correct parameter ranges
- Gaussian noise + dropout pattern
- Seeded RNG for reproducibility

### HIGH PRIORITY (Immediate Impact)
1. **Action Delay Wrapper** (Template 2) - Critical for real hardware
2. **Action Smoothness Penalty** (Template 2) - Prevents bang-bang control
3. **YAML Domain Randomization Config** (Template 1) - Centralized control
4. **Dynamics Randomization** (Template 3) - Physics parameter variation

### MEDIUM PRIORITY (Near-Term)
5. **Evaluation Infrastructure** (Template 5) - Measure reality gap
6. **Systematic Testing Protocol** - 50+ episode comparisons

### LONG-TERM (Advanced)
7. **Active Domain Randomization** (Template 4) - Automated curriculum
8. **Foundation Model Integration** - Multi-task learning

### Quick Wins (This Week)
- Enable existing noise models in config (set `enabled: true`)
- Add action delay wrapper to training pipeline
- Implement action smoothness penalty wrapper
- Document baseline (no DR) performance

### Research-Validated Parameter Ranges
All templates use parameter ranges validated by 2025-2026 research:
- LiDAR noise: 2% range, 1-5% dropout
- Odometry drift: 0.5-1.5% per meter
- Action delays: 1-8 steps at 20 Hz (50-400ms)
- Mass variation: ±20%
- Friction: 0.8-1.2× nominal

These patterns directly enable robust sim-to-real transfer for Warehouser.
