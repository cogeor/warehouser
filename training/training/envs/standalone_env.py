"""Standalone Gymnasium environment for training without ROS2.

This environment simulates a simple warehouse robot navigation task
for testing the training pipeline. Does not require ROS2 dependencies.
"""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray

from training.models.config import EnvConfig

# Type aliases
Observation = NDArray[np.float32]
Action = NDArray[np.float32]


class StandaloneEnv(gym.Env[Observation, Action]):
    """Standalone warehouse environment for training without ROS2.

    Simulates a robot navigating to a goal position. The robot receives
    observations about its position relative to the goal and takes
    velocity commands as actions.

    Observation space (5D):
        [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]

    Action space (4D):
        [linear_vel, angular_vel, pick, place] normalized to [-1, 1]
    """

    metadata = {"render_modes": ["human"], "render_fps": 20}

    def __init__(self, config: EnvConfig | None = None) -> None:
        """Initialize the environment.

        Args:
            config: Environment configuration. Uses defaults if not provided.
        """
        super().__init__()

        self.config = config or EnvConfig()

        # Action: [linear_vel, angular_vel, pick, place]
        self.action_space = gym.spaces.Box(
            low=-1.0, high=1.0, shape=(int(self.config.action_dim),), dtype=np.float32
        )

        # Observation space
        self.observation_space = gym.spaces.Box(
            low=-np.inf, high=np.inf, shape=(int(self.config.obs_dim),), dtype=np.float32
        )

        # World bounds
        self.world_size = 10.0

        # Robot state
        self._robot_x: float = 0.0
        self._robot_y: float = 0.0
        self._robot_theta: float = 0.0

        # Goal state
        self._goal_x: float = 0.0
        self._goal_y: float = 0.0

        # Episode state
        self._step_count = 0
        self._is_carrying = False

        # Physics params
        self._dt = 0.1  # timestep
        self._max_linear_vel = 1.0  # m/s
        self._max_angular_vel = 2.0  # rad/s

    def _get_observation(self) -> Observation:
        """Compute observation from current state."""
        # Relative goal position
        dx = self._goal_x - self._robot_x
        dy = self._goal_y - self._robot_y
        goal_dist = np.sqrt(dx * dx + dy * dy)

        # Goal heading relative to robot orientation
        goal_angle = np.arctan2(dy, dx)
        goal_heading = self._normalize_angle(goal_angle - self._robot_theta)

        # Build observation (pad with zeros if obs_dim > 5)
        obs = np.zeros(int(self.config.obs_dim), dtype=np.float32)
        obs[0] = dx
        obs[1] = dy
        obs[2] = goal_dist
        obs[3] = goal_heading
        obs[4] = float(self._is_carrying)

        return obs

    def _normalize_angle(self, angle: float) -> float:
        """Normalize angle to [-pi, pi]."""
        while angle > np.pi:
            angle -= 2 * np.pi
        while angle < -np.pi:
            angle += 2 * np.pi
        return angle

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[Observation, dict[str, Any]]:
        """Reset environment to initial state."""
        super().reset(seed=seed)

        # Random robot start position
        self._robot_x = self.np_random.uniform(1.0, self.world_size - 1.0)
        self._robot_y = self.np_random.uniform(1.0, self.world_size - 1.0)
        self._robot_theta = self.np_random.uniform(-np.pi, np.pi)

        # Random goal position (at least 2m away)
        while True:
            self._goal_x = self.np_random.uniform(1.0, self.world_size - 1.0)
            self._goal_y = self.np_random.uniform(1.0, self.world_size - 1.0)
            dx = self._goal_x - self._robot_x
            dy = self._goal_y - self._robot_y
            if np.sqrt(dx * dx + dy * dy) >= 2.0:
                break

        self._step_count = 0
        self._is_carrying = False

        return self._get_observation(), {}

    def step(self, action: Action) -> tuple[Observation, float, bool, bool, dict[str, Any]]:
        """Execute action and return results."""
        self._step_count += 1

        # Extract and scale actions
        linear_vel = float(action[0]) * self._max_linear_vel
        angular_vel = float(action[1]) * self._max_angular_vel

        # Apply kinematics
        self._robot_theta += angular_vel * self._dt
        self._robot_theta = self._normalize_angle(self._robot_theta)

        self._robot_x += linear_vel * np.cos(self._robot_theta) * self._dt
        self._robot_y += linear_vel * np.sin(self._robot_theta) * self._dt

        # Clamp to world bounds
        self._robot_x = np.clip(self._robot_x, 0.0, self.world_size)
        self._robot_y = np.clip(self._robot_y, 0.0, self.world_size)

        # Compute reward
        obs = self._get_observation()
        goal_dist = obs[2]

        # Dense reward: negative distance to goal
        reward = -goal_dist * 0.1

        # Bonus for reaching goal
        terminated = goal_dist < 0.5
        if terminated:
            reward += 10.0

        # Truncated if max steps reached
        truncated = self._step_count >= self.config.max_steps

        info = {
            "step": self._step_count,
            "goal_dist": goal_dist,
            "robot_pos": (self._robot_x, self._robot_y, self._robot_theta),
        }

        return obs, reward, terminated, truncated, info
