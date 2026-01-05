"""Gymnasium environment wrapper for ROS2 simulation."""

from typing import Any

import gymnasium as gym
import numpy as np
from numpy.typing import NDArray

from training.models.config import EnvConfig
from training.utils.result import Result

# Type aliases
Observation = NDArray[np.float32]
Action = NDArray[np.float32]


class ROSGymEnv(gym.Env[Observation, Action]):
    """Gymnasium environment that wraps the ROS2 simulation.

    Communicates with the simulation via ROS2 services:
    - /rl/reset: Reset the simulation
    - /rl/step: Execute an action and get observation/reward

    This environment is designed for PPO training with Stable-Baselines3.
    """

    metadata = {"render_modes": ["human"], "render_fps": 20}

    def __init__(self, config: EnvConfig | None = None) -> None:
        """Initialize the environment.

        Args:
            config: Environment configuration. Uses defaults if not provided.
        """
        super().__init__()

        self.config = config or EnvConfig()

        # Define action and observation spaces
        # Action: [linear_vel, angular_vel, pick, place]
        self.action_space = gym.spaces.Box(
            low=-1.0, high=1.0, shape=(self.config.action_dim,), dtype=np.float32
        )

        # Observation: [robot_x, robot_y, robot_theta, goal_dx, goal_dy,
        #               goal_dist, goal_heading, is_carrying]
        self.observation_space = gym.spaces.Box(
            low=-np.inf, high=np.inf, shape=(self.config.obs_dim,), dtype=np.float32
        )

        # ROS2 client initialization (lazy)
        self._ros_initialized = False
        self._node: Any = None
        self._reset_client: Any = None
        self._step_client: Any = None

        # Episode state
        self._step_count = 0

    def _init_ros(self) -> Result[None]:
        """Initialize ROS2 node and service clients.

        Returns:
            Result indicating success or failure.
        """
        if self._ros_initialized:
            return Result.ok(None)

        try:
            import rclpy
            from rclpy.node import Node

            # Initialize ROS2 if not already done
            if not rclpy.ok():
                rclpy.init()

            # Create node
            self._node = Node("gym_env")

            # Import message types
            from warehouser_msgs.srv import RLReset, RLStep

            # Create service clients
            self._reset_client = self._node.create_client(RLReset, "/rl/reset")
            self._step_client = self._node.create_client(RLStep, "/rl/step")

            # Wait for services
            if not self._reset_client.wait_for_service(timeout_sec=5.0):
                return Result.err("RLReset service not available")
            if not self._step_client.wait_for_service(timeout_sec=5.0):
                return Result.err("RLStep service not available")

            self._ros_initialized = True
            return Result.ok(None)

        except ImportError as e:
            return Result.err(f"Failed to import ROS2: {e}")
        except Exception as e:
            return Result.err(f"Failed to initialize ROS2: {e}")

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[Observation, dict[str, Any]]:
        """Reset the environment.

        Args:
            seed: Random seed for reproducibility.
            options: Additional options (unused).

        Returns:
            Initial observation and info dict.
        """
        super().reset(seed=seed)

        # Initialize ROS if needed
        init_result = self._init_ros()
        if init_result.is_err():
            # Return zero observation if ROS not available
            return np.zeros(self.config.obs_dim, dtype=np.float32), {"error": init_result.error()}

        # Call reset service
        from warehouser_msgs.srv import RLReset

        request = RLReset.Request()
        request.seed = seed if seed is not None else 0

        import rclpy

        future = self._reset_client.call_async(request)
        rclpy.spin_until_future_complete(self._node, future, timeout_sec=5.0)

        if future.result() is None:
            return np.zeros(self.config.obs_dim, dtype=np.float32), {"error": "Reset failed"}

        response = future.result()
        self._step_count = 0

        # Convert observation
        obs = np.array(response.observation.data, dtype=np.float32)
        if len(obs) != self.config.obs_dim:
            obs = np.zeros(self.config.obs_dim, dtype=np.float32)

        return obs, {"info": response.info}

    def step(self, action: Action) -> tuple[Observation, float, bool, bool, dict[str, Any]]:
        """Execute one step in the environment.

        Args:
            action: Action to take [linear_vel, angular_vel, pick, place].

        Returns:
            Tuple of (observation, reward, terminated, truncated, info).
        """
        # Ensure action is the right shape and type
        action = np.asarray(action, dtype=np.float32).flatten()
        if len(action) != self.config.action_dim:
            raise ValueError(f"Expected action dim {self.config.action_dim}, got {len(action)}")

        # Initialize ROS if needed
        init_result = self._init_ros()
        if init_result.is_err():
            return (
                np.zeros(self.config.obs_dim, dtype=np.float32),
                0.0,
                True,
                False,
                {"error": init_result.error()},
            )

        # Call step service
        from warehouser_msgs.srv import RLStep

        request = RLStep.Request()
        request.action_linear = float(action[0])
        request.action_angular = float(action[1])
        request.action_pick = float(action[2])
        request.action_place = float(action[3])
        request.num_steps = 1

        import rclpy

        future = self._step_client.call_async(request)
        rclpy.spin_until_future_complete(self._node, future, timeout_sec=5.0)

        if future.result() is None:
            return (
                np.zeros(self.config.obs_dim, dtype=np.float32),
                0.0,
                True,
                False,
                {"error": "Step failed"},
            )

        response = future.result()
        self._step_count += 1

        # Convert observation
        obs = np.array(response.observation.data, dtype=np.float32)
        if len(obs) != self.config.obs_dim:
            obs = np.zeros(self.config.obs_dim, dtype=np.float32)

        return (
            obs,
            float(response.reward),
            bool(response.terminated),
            bool(response.truncated),
            {"info": response.info, "step": self._step_count},
        )

    def render(self) -> None:
        """Render the environment (no-op, use frontend for visualization)."""
        pass

    def close(self) -> None:
        """Clean up resources."""
        if self._node is not None:
            self._node.destroy_node()
            self._node = None
        self._ros_initialized = False
