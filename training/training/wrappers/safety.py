"""Safety clipping wrapper for hard action limits.

Provides a final safety layer that enforces hard limits on all actions,
ensuring no commands exceed safe operating ranges regardless of upstream
wrapper outputs.
"""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray


class SafetyClippingWrapper(gym.ActionWrapper):
    """Enforce hard safety limits on all action dimensions.

    This wrapper serves as the final safety layer in the action pipeline,
    clipping all action values to specified hard limits. It should be
    applied last in the wrapper chain to guarantee no unsafe commands
    reach the simulation.

    Example:
        >>> env = SafetyClippingWrapper(
        ...     base_env,
        ...     hard_limits={
        ...         "linear": (-1.0, 1.0),
        ...         "angular": (-2.0, 2.0),
        ...         "pick": (-1.0, 1.0),
        ...         "place": (-1.0, 1.0),
        ...     }
        ... )
        >>> # Input action: [1.5, -3.0, 0.5, 0.5]
        >>> # Clipped action: [1.0, -2.0, 0.5, 0.5]

    Attributes:
        env: The wrapped gymnasium environment.
        hard_limits: Dictionary mapping action names to (min, max) tuples.
    """

    def __init__(
        self,
        env: gym.Env[NDArray[np.float32], NDArray[np.float32]],
        hard_limits: dict[str, tuple[float, float]],
    ) -> None:
        """Initialize the safety clipping wrapper.

        Args:
            env: The gymnasium environment to wrap.
            hard_limits: Dictionary mapping action names to (min, max) limit tuples:
                - 'linear': (min_linear_vel, max_linear_vel) in m/s
                - 'angular': (min_angular_vel, max_angular_vel) in rad/s
                - 'pick': (min_pick, max_pick) signal range
                - 'place': (min_place, max_place) signal range

        Raises:
            ValueError: If hard_limits is missing required keys or has invalid ranges.
        """
        super().__init__(env)

        required_keys = ["linear", "angular", "pick", "place"]
        for key in required_keys:
            if key not in hard_limits:
                raise ValueError(f"hard_limits must contain '{key}' key")

        # Validate all limits
        for key, (low, high) in hard_limits.items():
            if low >= high:
                raise ValueError(
                    f"hard_limits['{key}'] invalid: low ({low}) must be < high ({high})"
                )

        self.hard_limits = hard_limits

        # Update action space to reflect hard limits
        action_dim = env.action_space.shape[0] if env.action_space.shape else 4
        low = np.array(
            [
                hard_limits["linear"][0],
                hard_limits["angular"][0],
                hard_limits["pick"][0],
                hard_limits["place"][0],
            ]
            + [float("-inf")] * (action_dim - 4),
            dtype=np.float32,
        )
        high = np.array(
            [
                hard_limits["linear"][1],
                hard_limits["angular"][1],
                hard_limits["pick"][1],
                hard_limits["place"][1],
            ]
            + [float("inf")] * (action_dim - 4),
            dtype=np.float32,
        )
        self.action_space = gym.spaces.Box(low=low, high=high, dtype=np.float32)

    def action(self, action: NDArray[np.float32]) -> NDArray[np.float32]:
        """Clip action to hard safety limits.

        Args:
            action: Action array from upstream wrapper, expected shape (4,):
                - action[0]: linear velocity
                - action[1]: angular velocity
                - action[2]: pick signal
                - action[3]: place signal

        Returns:
            Clipped action array with all values within hard limits.
        """
        action = np.asarray(action, dtype=np.float32)
        clipped_action = action.copy()

        # Clip each action dimension to its hard limits
        clipped_action[0] = np.clip(
            action[0],
            self.hard_limits["linear"][0],
            self.hard_limits["linear"][1],
        )
        clipped_action[1] = np.clip(
            action[1],
            self.hard_limits["angular"][0],
            self.hard_limits["angular"][1],
        )
        clipped_action[2] = np.clip(
            action[2],
            self.hard_limits["pick"][0],
            self.hard_limits["pick"][1],
        )
        clipped_action[3] = np.clip(
            action[3],
            self.hard_limits["place"][0],
            self.hard_limits["place"][1],
        )

        return clipped_action
