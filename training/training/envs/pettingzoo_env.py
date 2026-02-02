"""PettingZoo ParallelEnv wrapper for multi-robot warehouse simulation.

Implements the PettingZoo ParallelEnv API for multi-agent reinforcement learning
with simultaneous agent actions. Compatible with MARL algorithms like MAPPO, QMIX.

Based on RWARE pattern: https://github.com/semitable/robotic-warehouse
"""

from typing import Any

import numpy as np
from gymnasium import spaces
from numpy.typing import NDArray
from pettingzoo import ParallelEnv

from training.models.config import MultiAgentConfig
from training.utils.result import Result

# Type aliases
AgentID = str
Observation = NDArray[np.float32]
Action = NDArray[np.float32]


class WarehouseParallelEnv(ParallelEnv[AgentID, Observation, Action]):
    """Multi-agent warehouse environment using PettingZoo ParallelEnv.

    All agents act simultaneously. Uses ROS2 services for simulation.

    Attributes:
        config: Multi-agent configuration.
        possible_agents: List of all possible agent IDs.
        agents: List of currently active agents.
    """

    metadata = {"render_modes": ["human"], "name": "warehouse_v1"}

    def __init__(self, config: MultiAgentConfig | None = None) -> None:
        """Initialize the environment.

        Args:
            config: Multi-agent configuration. Uses defaults if not provided.
        """
        super().__init__()

        self.config = config or MultiAgentConfig()

        # Agent IDs
        self.possible_agents: list[AgentID] = [
            f"robot_{i}" for i in range(self.config.num_agents)
        ]
        self.agents: list[AgentID] = self.possible_agents.copy()

        # Spaces per agent
        self._observation_spaces: dict[AgentID, spaces.Box] = {
            agent: spaces.Box(
                low=-np.inf,
                high=np.inf,
                shape=(self.config.obs_dim,),
                dtype=np.float32,
            )
            for agent in self.possible_agents
        }
        self._action_spaces: dict[AgentID, spaces.Box] = {
            agent: spaces.Box(
                low=-1.0,
                high=1.0,
                shape=(self.config.action_dim,),
                dtype=np.float32,
            )
            for agent in self.possible_agents
        }

        # ROS2 client (lazy init)
        self._ros_initialized = False
        self._node: Any = None
        self._reset_client: Any = None
        self._step_client: Any = None

        # Episode state
        self._step_count = 0

    @property
    def observation_spaces(self) -> dict[AgentID, spaces.Space[Observation]]:
        """Return observation spaces for all agents."""
        return self._observation_spaces  # type: ignore[return-value]

    @property
    def action_spaces(self) -> dict[AgentID, spaces.Space[Action]]:
        """Return action spaces for all agents."""
        return self._action_spaces  # type: ignore[return-value]

    def observation_space(self, agent: AgentID) -> spaces.Space[Observation]:
        """Return observation space for a specific agent."""
        return self._observation_spaces[agent]

    def action_space(self, agent: AgentID) -> spaces.Space[Action]:
        """Return action space for a specific agent."""
        return self._action_spaces[agent]

    def _init_ros(self) -> Result[None]:
        """Initialize ROS2 node and service clients."""
        if self._ros_initialized:
            return Result.ok(None)

        try:
            import rclpy  # type: ignore[import-not-found]
            from rclpy.node import Node  # type: ignore[import-not-found]

            # Import ROS message types
            from warehouser_msgs.srv import (  # type: ignore[import-not-found]
                RLReset,
                RLStep,
            )

            if not rclpy.ok():
                rclpy.init()

            self._node = Node("pettingzoo_env")
            self._reset_client = self._node.create_client(RLReset, "/rl/reset")
            self._step_client = self._node.create_client(RLStep, "/rl/step")

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
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[dict[AgentID, Observation], dict[AgentID, dict[str, Any]]]:
        """Reset the environment.

        Args:
            seed: Random seed for reproducibility.
            options: Additional options (unused).

        Returns:
            Tuple of (observations dict, infos dict) keyed by agent ID.
        """
        _ = options  # Unused

        self.agents = self.possible_agents.copy()
        self._step_count = 0

        # Initialize ROS if needed
        init_result = self._init_ros()
        if init_result.is_err():
            error_msg = init_result.error() or "Unknown error"
            return self._empty_observations(), self._error_infos(error_msg)

        # Call reset service with robot count
        try:
            import rclpy  # type: ignore[import-not-found]
            from warehouser_msgs.srv import RLReset  # type: ignore[import-not-found]

            request = RLReset.Request()
            request.seed = seed if seed is not None else 0
            request.robot_count = self.config.num_agents

            future = self._reset_client.call_async(request)
            rclpy.spin_until_future_complete(self._node, future, timeout_sec=5.0)

            if future.result() is None:
                return self._empty_observations(), self._error_infos("Reset failed")

            response = future.result()

            # Build per-agent observations
            observations: dict[AgentID, Observation] = {}
            infos: dict[AgentID, dict[str, Any]] = {}

            for i, agent in enumerate(self.agents):
                if i < len(response.observations):
                    obs = np.array(response.observations[i].data, dtype=np.float32)
                    if len(obs) != self.config.obs_dim:
                        obs = np.zeros(self.config.obs_dim, dtype=np.float32)
                    observations[agent] = obs
                else:
                    observations[agent] = np.zeros(
                        self.config.obs_dim, dtype=np.float32
                    )
                infos[agent] = {"info": response.info}

            return observations, infos

        except Exception as e:
            return self._empty_observations(), self._error_infos(str(e))

    def step(
        self, actions: dict[AgentID, Action]
    ) -> tuple[
        dict[AgentID, Observation],
        dict[AgentID, float],
        dict[AgentID, bool],
        dict[AgentID, bool],
        dict[AgentID, dict[str, Any]],
    ]:
        """Execute one step for all agents.

        Args:
            actions: Dict mapping agent IDs to actions.

        Returns:
            Tuple of (observations, rewards, terminations, truncations, infos).
            All return values are dicts keyed by agent ID.
        """
        observations: dict[AgentID, Observation] = {}
        rewards: dict[AgentID, float] = {}
        terminations: dict[AgentID, bool] = {}
        truncations: dict[AgentID, bool] = {}
        infos: dict[AgentID, dict[str, Any]] = {}

        # Initialize ROS if needed
        init_result = self._init_ros()
        if init_result.is_err():
            error_msg = init_result.error() or "Unknown error"
            for agent in self.agents:
                observations[agent] = np.zeros(self.config.obs_dim, dtype=np.float32)
                rewards[agent] = 0.0
                terminations[agent] = True
                truncations[agent] = False
                infos[agent] = {"error": error_msg}
            return observations, rewards, terminations, truncations, infos

        try:
            import rclpy  # type: ignore[import-not-found]
            from warehouser_msgs.srv import RLStep  # type: ignore[import-not-found]

            # Step each agent
            total_reward = 0.0
            any_terminated = False

            for i, agent in enumerate(self.agents):
                action = actions.get(
                    agent, np.zeros(self.config.action_dim, dtype=np.float32)
                )
                action = np.asarray(action, dtype=np.float32).flatten()

                request = RLStep.Request()
                request.robot_id = i
                request.action_linear = float(action[0])
                request.action_angular = float(action[1]) if len(action) > 1 else 0.0
                request.action_pick = float(action[2]) if len(action) > 2 else 0.0
                request.action_place = float(action[3]) if len(action) > 3 else 0.0
                request.num_steps = 1

                future = self._step_client.call_async(request)
                rclpy.spin_until_future_complete(self._node, future, timeout_sec=5.0)

                if future.result() is None:
                    observations[agent] = np.zeros(
                        self.config.obs_dim, dtype=np.float32
                    )
                    rewards[agent] = 0.0
                    terminations[agent] = True
                    truncations[agent] = False
                    infos[agent] = {"error": "Step failed"}
                else:
                    response = future.result()
                    obs = np.array(response.observation.data, dtype=np.float32)
                    if len(obs) != self.config.obs_dim:
                        obs = np.zeros(self.config.obs_dim, dtype=np.float32)

                    observations[agent] = obs
                    rewards[agent] = float(response.reward)
                    terminations[agent] = bool(response.terminated)
                    truncations[agent] = bool(response.truncated)
                    infos[agent] = {"info": response.info}

                    total_reward += rewards[agent]
                    any_terminated = any_terminated or terminations[agent]

            self._step_count += 1

            # Shared reward option (team reward)
            if self.config.shared_reward and len(self.agents) > 0:
                avg_reward = total_reward / len(self.agents)
                for agent in self.agents:
                    rewards[agent] = avg_reward

            # Global truncation (max steps)
            if self._step_count >= self.config.max_steps:
                for agent in self.agents:
                    truncations[agent] = True

            # Remove terminated agents
            self.agents = [a for a in self.agents if not terminations.get(a, False)]

            return observations, rewards, terminations, truncations, infos

        except Exception as e:
            for agent in self.agents:
                observations[agent] = np.zeros(self.config.obs_dim, dtype=np.float32)
                rewards[agent] = 0.0
                terminations[agent] = True
                truncations[agent] = False
                infos[agent] = {"error": str(e)}
            return observations, rewards, terminations, truncations, infos

    def render(self) -> None:
        """Render the environment (no-op, use frontend for visualization)."""
        pass

    def close(self) -> None:
        """Clean up resources."""
        if self._node is not None:
            self._node.destroy_node()
            self._node = None
        self._ros_initialized = False

    def _empty_observations(self) -> dict[AgentID, Observation]:
        """Return zero observations for all agents."""
        return {
            agent: np.zeros(self.config.obs_dim, dtype=np.float32)
            for agent in self.agents
        }

    def _error_infos(self, error: str) -> dict[AgentID, dict[str, Any]]:
        """Return error infos for all agents."""
        return {agent: {"error": error} for agent in self.agents}
