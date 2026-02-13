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

        # Observation space dimension depends on config.obs_dim:
        # - V1_Basic (8): [x, y, theta, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
        # - V2_Lidar (63): V1 + 55 lidar rays
        # - V3_MultiRobot (17): Per-robot observations for multi-agent
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

        # Action feedback state for masking
        self._is_carrying = False

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
        self._is_carrying = False  # Reset carrying state on episode start

        # Convert observation
        obs = np.array(response.observation.data, dtype=np.float32)
        if len(obs) != self.config.obs_dim:
            obs = np.zeros(self.config.obs_dim, dtype=np.float32)

        return obs, {"info": response.info}

    def step(self, action: Action) -> tuple[Observation, float, bool, bool, dict[str, Any]]:
        """Execute one step in the environment.

        Args:
            action: Action to take [linear_vel, angular_vel, pick, place].
                All values expected in [-1, 1] range (normalized from policy).

        Returns:
            Tuple of (observation, reward, terminated, truncated, info).
        """
        # ======================================================================
        # ACTION PROCESSING (Python Side)
        # ======================================================================
        # This is the first stage of action processing. The full pipeline is:
        #
        #   Policy Output [-1,1]
        #       |
        #       v
        #   [Action Wrappers] (if configured)
        #       - ActionScalingWrapper:  Scale to physical velocity limits
        #       - ActionSmoothingWrapper: EMA filter for smooth motion
        #       - AccelerationLimitWrapper: Limit rate of velocity change
        #       - SafetyClippingWrapper: Hard limits as final safety layer
        #       |
        #       v
        #   [ros_env.step()] <-- We are here
        #       - Action masking based on carrying state
        #       |
        #       v
        #   [RLBridge.sendAction()]
        #       - Velocity scaling (if not done by wrapper)
        #       - SafetyController (obstacle avoidance)
        #       - Discrete action triggering (threshold at 0.5)
        #       |
        #       v
        #   Robot Commands
        #
        # ======================================================================

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

        # ======================================================================
        # ACTION MASKING
        # ======================================================================
        # Mask invalid discrete actions based on the robot's carrying state.
        # This is a form of "action validity" that prevents logically impossible
        # actions from being sent to the simulation:
        #
        #   - If carrying an object: Can't pick another (pick masked to 0)
        #   - If not carrying:       Can't place nothing (place masked to 0)
        #
        # Why mask here (Python) instead of in RLBridge (C++)?
        #   1. Faster feedback loop - invalid actions rejected before ROS call
        #   2. Cleaner separation - policy correction happens at training level
        #   3. RLBridge can assume valid actions, simplifying C++ logic
        #
        # The mask sets the signal to 0.0, which is below the trigger threshold
        # of 0.5 used in RLBridge::sendAction(), so the discrete action won't
        # fire even if the policy wanted it to.
        #
        # Future enhancement: Invalid action masking could also be done via
        # Gymnasium's action_mask mechanism for more explicit policy guidance.
        # ======================================================================
        masked_action = action.copy()
        if self._is_carrying:
            masked_action[2] = 0.0  # Mask pick when carrying
        else:
            masked_action[3] = 0.0  # Mask place when not carrying

        # Call step service
        from warehouser_msgs.srv import RLStep

        request = RLStep.Request()
        request.action_linear = float(masked_action[0])
        request.action_angular = float(masked_action[1])
        request.action_pick = float(masked_action[2])
        request.action_place = float(masked_action[3])
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

        # Update carrying state from response feedback
        self._is_carrying = bool(response.is_carrying)

        # Convert observation
        obs = np.array(response.observation.data, dtype=np.float32)
        if len(obs) != self.config.obs_dim:
            obs = np.zeros(self.config.obs_dim, dtype=np.float32)

        # Build info dict with action feedback
        info: dict[str, Any] = {
            "info": response.info,
            "step": self._step_count,
            "safety_state": int(response.safety_state),
            "pick_success": bool(response.pick_success),
            "place_success": bool(response.place_success),
            "is_carrying": self._is_carrying,
        }

        return (
            obs,
            float(response.reward),
            bool(response.terminated),
            bool(response.truncated),
            info,
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
