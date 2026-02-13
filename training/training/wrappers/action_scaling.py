"""Action scaling wrapper for velocity limit enforcement.

Scales normalized actions from [-1, 1] to physical velocity limits while
keeping discrete actions (pick/place) in their original range.
"""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray


class ActionScalingWrapper(gym.ActionWrapper):
    """Scale normalized actions to physical velocity limits.

    The robot's neural network outputs actions in [-1, 1]. This wrapper scales
    the velocity components to match the robot's physical capabilities:
    - action[0] (linear velocity): scaled by linear_limit
    - action[1] (angular velocity): scaled by angular_limit
    - action[2:] (pick/place): kept in [-1, 1] for threshold comparison

    Example:
        >>> env = ActionScalingWrapper(
        ...     base_env,
        ...     velocity_limits={"linear": 1.0, "angular": 2.0}
        ... )
        >>> # Network output: [0.5, -0.5, 0.8, -0.2]
        >>> # Scaled action: [0.5, -1.0, 0.8, -0.2]

    Attributes:
        env: The wrapped gymnasium environment.
        linear_limit: Maximum linear velocity in m/s.
        angular_limit: Maximum angular velocity in rad/s.
    """

    def __init__(
        self,
        env: gym.Env[NDArray[np.float32], NDArray[np.float32]],
        velocity_limits: dict[str, float],
    ) -> None:
        """Initialize the action scaling wrapper.

        Args:
            env: The gymnasium environment to wrap.
            velocity_limits: Dictionary with 'linear' and 'angular' velocity limits.
                - 'linear': Maximum linear velocity in m/s (e.g., 1.0)
                - 'angular': Maximum angular velocity in rad/s (e.g., 2.0)

        Raises:
            ValueError: If velocity_limits is missing required keys or has invalid values.
        """
        super().__init__(env)

        # Validate velocity limits
        if "linear" not in velocity_limits:
            raise ValueError("velocity_limits must contain 'linear' key")
        if "angular" not in velocity_limits:
            raise ValueError("velocity_limits must contain 'angular' key")

        self.linear_limit = velocity_limits["linear"]
        self.angular_limit = velocity_limits["angular"]

        if self.linear_limit <= 0:
            raise ValueError(f"linear velocity limit must be > 0, got {self.linear_limit}")
        if self.angular_limit <= 0:
            raise ValueError(f"angular velocity limit must be > 0, got {self.angular_limit}")

        # Update action space to reflect scaled limits
        # The policy still outputs [-1, 1], but the effective action space is larger
        action_dim = env.action_space.shape[0] if env.action_space.shape else 4
        low = np.array(
            [-self.linear_limit, -self.angular_limit] + [-1.0] * (action_dim - 2),
            dtype=np.float32,
        )
        high = np.array(
            [self.linear_limit, self.angular_limit] + [1.0] * (action_dim - 2),
            dtype=np.float32,
        )
        self.action_space = gym.spaces.Box(low=low, high=high, dtype=np.float32)

    def action(self, action: NDArray[np.float32]) -> NDArray[np.float32]:
        """Scale the action to physical velocity limits.

        Args:
            action: Normalized action array from the policy, expected shape (4,):
                - action[0]: linear velocity in [-1, 1]
                - action[1]: angular velocity in [-1, 1]
                - action[2]: pick signal in [-1, 1]
                - action[3]: place signal in [-1, 1]

        Returns:
            Scaled action array with:
                - action[0]: linear velocity in [-linear_limit, linear_limit]
                - action[1]: angular velocity in [-angular_limit, angular_limit]
                - action[2:]: unchanged (pick/place signals)
        """
        action = np.asarray(action, dtype=np.float32)
        scaled_action = action.copy()

        # Scale velocity components
        scaled_action[0] = action[0] * self.linear_limit
        scaled_action[1] = action[1] * self.angular_limit

        # Keep pick/place actions in [-1, 1] range (indices 2+)
        # No modification needed for these

        return scaled_action
