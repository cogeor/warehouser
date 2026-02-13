"""Acceleration limiting wrapper for realistic robot dynamics.

Enforces maximum acceleration/deceleration limits on velocity commands to
ensure physically realistic motion and prevent impossible velocity jumps.
"""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray


class AccelerationLimitWrapper(gym.ActionWrapper):
    """Enforce acceleration limits on velocity commands.

    Limits the rate of change of velocity commands to ensure the robot
    cannot instantly change direction or speed. This produces more
    realistic motion and smoother trajectories.

    The acceleration clipping formula is:
        delta = clamp(target - current, -max_delta * dt, max_delta * dt)
        new_velocity = current + delta

    Example:
        >>> env = AccelerationLimitWrapper(
        ...     base_env,
        ...     max_delta={"linear": 2.0, "angular": 4.0},
        ...     dt=0.05
        ... )
        >>> # Previous velocity: [0.0, 0.0]
        >>> # Requested velocity: [1.0, 2.0]
        >>> # Max change per step: [0.1, 0.2]  (max_delta * dt)
        >>> # Clamped velocity: [0.1, 0.2]

    Attributes:
        env: The wrapped gymnasium environment.
        max_linear_delta: Maximum linear acceleration in m/s^2.
        max_angular_delta: Maximum angular acceleration in rad/s^2.
        dt: Timestep in seconds.
        prev_velocity: Previous velocity for acceleration calculation.
    """

    def __init__(
        self,
        env: gym.Env[NDArray[np.float32], NDArray[np.float32]],
        max_delta: dict[str, float],
        dt: float = 0.05,
    ) -> None:
        """Initialize the acceleration limit wrapper.

        Args:
            env: The gymnasium environment to wrap.
            max_delta: Dictionary with maximum acceleration values:
                - 'linear': Maximum linear acceleration in m/s^2
                - 'angular': Maximum angular acceleration in rad/s^2
            dt: Simulation timestep in seconds. Default is 0.05 (20 Hz).

        Raises:
            ValueError: If max_delta is missing keys or has invalid values.
        """
        super().__init__(env)

        # Validate max_delta
        if "linear" not in max_delta:
            raise ValueError("max_delta must contain 'linear' key")
        if "angular" not in max_delta:
            raise ValueError("max_delta must contain 'angular' key")

        self.max_linear_delta = max_delta["linear"]
        self.max_angular_delta = max_delta["angular"]

        if self.max_linear_delta <= 0:
            raise ValueError(f"linear acceleration limit must be > 0, got {self.max_linear_delta}")
        if self.max_angular_delta <= 0:
            raise ValueError(f"angular acceleration limit must be > 0, got {self.max_angular_delta}")

        if dt <= 0:
            raise ValueError(f"dt must be > 0, got {dt}")

        self.dt = dt
        self._prev_velocity: NDArray[np.float32] | None = None

    def action(self, action: NDArray[np.float32]) -> NDArray[np.float32]:
        """Apply acceleration limits to velocity commands.

        Args:
            action: Action array from the policy, expected shape (4,):
                - action[0]: target linear velocity
                - action[1]: target angular velocity
                - action[2]: pick signal
                - action[3]: place signal

        Returns:
            Acceleration-limited action array. Velocity components are clamped
            to ensure acceleration does not exceed limits.
        """
        action = np.asarray(action, dtype=np.float32)
        limited_action = action.copy()

        if self._prev_velocity is None:
            # First action after reset - initialize to zeros
            self._prev_velocity = np.zeros(2, dtype=np.float32)

        # Compute maximum velocity change per timestep
        max_linear_change = self.max_linear_delta * self.dt
        max_angular_change = self.max_angular_delta * self.dt

        # Compute desired velocity change
        linear_delta = action[0] - self._prev_velocity[0]
        angular_delta = action[1] - self._prev_velocity[1]

        # Clamp acceleration
        linear_delta = np.clip(linear_delta, -max_linear_change, max_linear_change)
        angular_delta = np.clip(angular_delta, -max_angular_change, max_angular_change)

        # Compute new velocity
        limited_action[0] = self._prev_velocity[0] + linear_delta
        limited_action[1] = self._prev_velocity[1] + angular_delta

        # Update previous velocity for next step
        self._prev_velocity[0] = limited_action[0]
        self._prev_velocity[1] = limited_action[1]

        return limited_action

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[NDArray[np.float32], dict[str, Any]]:
        """Reset the environment and clear velocity state.

        Args:
            seed: Random seed for reproducibility.
            options: Additional reset options.

        Returns:
            Initial observation and info dict from the wrapped environment.
        """
        self._prev_velocity = None
        return self.env.reset(seed=seed, options=options)
