"""Action smoothing wrapper with exponential moving average filter.

Applies EMA smoothing to velocity commands to prevent jerky motion and
improve training stability.
"""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray


class ActionSmoothingWrapper(gym.ActionWrapper):
    """Apply exponential moving average smoothing to velocity actions.

    Smooths velocity commands (linear and angular) using an EMA filter to
    prevent sudden changes that could destabilize the robot or confuse the
    neural network during training.

    The smoothing formula is:
        smoothed = alpha * current + (1 - alpha) * previous

    Where alpha controls the responsiveness:
    - alpha close to 1.0: More responsive, less smoothing
    - alpha close to 0.0: More smoothing, slower response

    Example:
        >>> env = ActionSmoothingWrapper(base_env, alpha=0.3)
        >>> # Previous action: [0.0, 0.0, ...]
        >>> # Current action:  [1.0, 1.0, ...]
        >>> # Smoothed action: [0.3, 0.3, ...]  (30% of new + 70% of old)

    Attributes:
        env: The wrapped gymnasium environment.
        alpha: EMA smoothing factor in (0, 1].
        prev_action: Previous smoothed velocity action for EMA calculation.
    """

    def __init__(
        self,
        env: gym.Env[NDArray[np.float32], NDArray[np.float32]],
        alpha: float = 0.3,
    ) -> None:
        """Initialize the action smoothing wrapper.

        Args:
            env: The gymnasium environment to wrap.
            alpha: EMA smoothing factor. Must be in (0, 1].
                - 0.3 (default): Good balance of smoothing and responsiveness
                - 0.1: Heavy smoothing, slow response
                - 1.0: No smoothing (passthrough)

        Raises:
            ValueError: If alpha is not in (0, 1].
        """
        super().__init__(env)

        if not (0.0 < alpha <= 1.0):
            raise ValueError(
                f"alpha must be in (0, 1], got {alpha}. "
                "Use alpha=1.0 for no smoothing, lower values for more smoothing."
            )

        self.alpha = alpha
        self._prev_action: NDArray[np.float32] | None = None

    def action(self, action: NDArray[np.float32]) -> NDArray[np.float32]:
        """Apply EMA smoothing to velocity actions.

        Args:
            action: Action array from the policy, expected shape (4,):
                - action[0]: linear velocity
                - action[1]: angular velocity
                - action[2]: pick signal
                - action[3]: place signal

        Returns:
            Smoothed action array with EMA applied to velocity components.
            Pick/place signals (indices 2+) are not smoothed.
        """
        action = np.asarray(action, dtype=np.float32)
        smoothed_action = action.copy()

        if self._prev_action is None:
            # First action after reset - no smoothing
            self._prev_action = action.copy()
            return smoothed_action

        # Apply EMA smoothing to velocity components only (indices 0-1)
        smoothed_action[0] = self.alpha * action[0] + (1.0 - self.alpha) * self._prev_action[0]
        smoothed_action[1] = self.alpha * action[1] + (1.0 - self.alpha) * self._prev_action[1]

        # Update previous action for next step
        self._prev_action = smoothed_action.copy()

        return smoothed_action

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[NDArray[np.float32], dict[str, Any]]:
        """Reset the environment and clear smoothing state.

        Args:
            seed: Random seed for reproducibility.
            options: Additional reset options.

        Returns:
            Initial observation and info dict from the wrapped environment.
        """
        self._prev_action = None
        return self.env.reset(seed=seed, options=options)
