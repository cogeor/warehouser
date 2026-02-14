"""Environment factory for creating wrapped Gymnasium environments.

Provides a single function to create a fully-configured training environment
with the complete action wrapper pipeline.
"""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray

from training.envs.ros_env import ROSGymEnv
from training.models.config import ActionConfig, EnvConfig
from training.wrappers import (
    AccelerationLimitWrapper,
    ActionScalingWrapper,
    ActionSmoothingWrapper,
    SafetyClippingWrapper,
)


def create_warehouser_env(
    config: dict[str, Any],
) -> gym.Env[NDArray[np.float32], NDArray[np.float32]]:
    """Create a fully-configured warehouser training environment.

    Constructs the environment with the complete action wrapper pipeline:
    1. ROSGymEnv - Base environment with ROS2 communication
    2. ActionScalingWrapper - Scale [-1, 1] to physical velocity limits
    3. ActionSmoothingWrapper - EMA filter for smooth transitions
    4. AccelerationLimitWrapper - Enforce acceleration constraints
    5. SafetyClippingWrapper - Hard safety limits
    6. TimeLimit - Episode length limit

    Example:
        >>> config = {
        ...     "env": {"obs_dim": 63, "action_dim": 4, "max_steps": 500},
        ...     "action": {
        ...         "velocity_limits": {"linear": 1.0, "angular": 2.0},
        ...         "smoothing_alpha": 0.3,
        ...         "max_acceleration": {"linear": 2.0, "angular": 4.0},
        ...         "hard_limits": {
        ...             "linear": (-1.0, 1.0),
        ...             "angular": (-2.0, 2.0),
        ...             "pick": (-1.0, 1.0),
        ...             "place": (-1.0, 1.0),
        ...         },
        ...         "dt": 0.05,
        ...     },
        ... }
        >>> env = create_warehouser_env(config)

    Args:
        config: Configuration dictionary with 'env' and 'action' sections.
            - 'env': EnvConfig parameters (obs_dim, action_dim, max_steps, etc.)
            - 'action': ActionConfig parameters (velocity_limits, smoothing_alpha, etc.)

    Returns:
        Fully wrapped gymnasium environment ready for training.

    Raises:
        ValueError: If config is missing required sections or has invalid values.
    """
    # Extract configuration sections
    env_config_dict = config.get("env", {})
    action_config_dict = config.get("action", {})

    # Create configuration objects
    env_config = EnvConfig(**env_config_dict)
    action_config = ActionConfig(**action_config_dict)

    # 1. Create base environment
    env: gym.Env[NDArray[np.float32], NDArray[np.float32]] = ROSGymEnv(config=env_config)

    # 2. Apply action scaling wrapper
    env = ActionScalingWrapper(
        env,
        velocity_limits={
            "linear": action_config.velocity_limits["linear"],
            "angular": action_config.velocity_limits["angular"],
        },
    )

    # 3. Apply action smoothing wrapper
    env = ActionSmoothingWrapper(
        env,
        alpha=action_config.smoothing_alpha,
    )

    # 4. Apply acceleration limit wrapper
    env = AccelerationLimitWrapper(
        env,
        max_delta={
            "linear": action_config.max_acceleration["linear"],
            "angular": action_config.max_acceleration["angular"],
        },
        dt=action_config.dt,
    )

    # 5. Apply safety clipping wrapper
    env = SafetyClippingWrapper(
        env,
        hard_limits=action_config.hard_limits,
    )

    # 6. Apply time limit wrapper
    env = gym.wrappers.TimeLimit(env, max_episode_steps=env_config.max_steps)

    return env


def create_env_from_configs(
    env_config: EnvConfig,
    action_config: ActionConfig,
) -> gym.Env[NDArray[np.float32], NDArray[np.float32]]:
    """Create environment from pre-validated config objects.

    Alternative factory function that accepts Pydantic config objects
    directly instead of a dictionary.

    Args:
        env_config: Validated environment configuration.
        action_config: Validated action configuration.

    Returns:
        Fully wrapped gymnasium environment ready for training.
    """
    # 1. Create base environment
    env: gym.Env[NDArray[np.float32], NDArray[np.float32]] = ROSGymEnv(config=env_config)

    # 2. Apply action scaling wrapper
    env = ActionScalingWrapper(
        env,
        velocity_limits={
            "linear": action_config.velocity_limits["linear"],
            "angular": action_config.velocity_limits["angular"],
        },
    )

    # 3. Apply action smoothing wrapper
    env = ActionSmoothingWrapper(
        env,
        alpha=action_config.smoothing_alpha,
    )

    # 4. Apply acceleration limit wrapper
    env = AccelerationLimitWrapper(
        env,
        max_delta={
            "linear": action_config.max_acceleration["linear"],
            "angular": action_config.max_acceleration["angular"],
        },
        dt=action_config.dt,
    )

    # 5. Apply safety clipping wrapper
    env = SafetyClippingWrapper(
        env,
        hard_limits=action_config.hard_limits,
    )

    # 6. Apply time limit wrapper
    env = gym.wrappers.TimeLimit(env, max_episode_steps=env_config.max_steps)

    return env
