"""Tests for PettingZoo multi-agent environment."""

import numpy as np
import pytest

from training.envs.pettingzoo_env import WarehouseParallelEnv
from training.models.config import MultiAgentConfig


class TestWarehouseParallelEnv:
    """Tests for PettingZoo multi-agent environment."""

    def test_default_config(self) -> None:
        """Test default configuration values."""
        env = WarehouseParallelEnv()
        assert env.config.num_agents == 2
        assert env.config.obs_dim == 17
        assert env.config.action_dim == 4
        assert env.config.max_steps == 500
        assert env.config.shared_reward is False

    def test_custom_config(self) -> None:
        """Test custom configuration."""
        config = MultiAgentConfig(
            num_agents=3,
            obs_dim=20,
            action_dim=5,
            max_steps=1000,
            shared_reward=True,
        )
        env = WarehouseParallelEnv(config)
        assert env.config.num_agents == 3
        assert env.config.obs_dim == 20
        assert env.config.action_dim == 5
        assert env.config.max_steps == 1000
        assert env.config.shared_reward is True

    def test_agent_count(self) -> None:
        """Test that agent count matches config."""
        config = MultiAgentConfig(num_agents=3)
        env = WarehouseParallelEnv(config)
        assert len(env.possible_agents) == 3
        assert env.possible_agents == ["robot_0", "robot_1", "robot_2"]

    def test_agent_ids_format(self) -> None:
        """Test agent ID format."""
        config = MultiAgentConfig(num_agents=5)
        env = WarehouseParallelEnv(config)
        for i, agent in enumerate(env.possible_agents):
            assert agent == f"robot_{i}"

    def test_observation_spaces(self) -> None:
        """Test observation space per agent."""
        config = MultiAgentConfig(num_agents=2, obs_dim=17)
        env = WarehouseParallelEnv(config)

        assert len(env.observation_spaces) == 2
        for agent in env.possible_agents:
            assert agent in env.observation_spaces
            assert env.observation_space(agent).shape == (17,)
            assert env.observation_space(agent).dtype == np.float32

    def test_action_spaces(self) -> None:
        """Test action space per agent."""
        config = MultiAgentConfig(num_agents=2, action_dim=4)
        env = WarehouseParallelEnv(config)

        assert len(env.action_spaces) == 2
        for agent in env.possible_agents:
            assert agent in env.action_spaces
            assert env.action_space(agent).shape == (4,)
            assert env.action_space(agent).dtype == np.float32

    def test_action_space_bounds(self) -> None:
        """Test action space bounds."""
        env = WarehouseParallelEnv()
        for agent in env.possible_agents:
            space = env.action_space(agent)
            assert np.all(space.low == -1.0)
            assert np.all(space.high == 1.0)

    def test_metadata(self) -> None:
        """Test environment metadata."""
        env = WarehouseParallelEnv()
        assert "render_modes" in env.metadata
        assert "name" in env.metadata
        assert env.metadata["name"] == "warehouse_v1"

    def test_agents_mutable(self) -> None:
        """Test that agents list is independent from possible_agents."""
        env = WarehouseParallelEnv()
        # Modify agents (simulating agent termination)
        env.agents = env.agents[:-1]
        # possible_agents should be unchanged
        assert len(env.possible_agents) == 2
        assert len(env.agents) == 1

    def test_close_idempotent(self) -> None:
        """Test that close() can be called multiple times."""
        env = WarehouseParallelEnv()
        env.close()
        env.close()  # Should not raise

    def test_render_noop(self) -> None:
        """Test that render() is a no-op."""
        env = WarehouseParallelEnv()
        env.render()  # Should not raise

    def test_empty_observations_shape(self) -> None:
        """Test _empty_observations returns correct shapes."""
        config = MultiAgentConfig(num_agents=3, obs_dim=20)
        env = WarehouseParallelEnv(config)
        obs = env._empty_observations()

        assert len(obs) == 3
        for agent in env.agents:
            assert obs[agent].shape == (20,)
            assert np.all(obs[agent] == 0.0)

    def test_error_infos_format(self) -> None:
        """Test _error_infos returns correct format."""
        env = WarehouseParallelEnv()
        infos = env._error_infos("test error")

        assert len(infos) == 2
        for agent in env.agents:
            assert "error" in infos[agent]
            assert infos[agent]["error"] == "test error"


class TestMultiAgentConfig:
    """Tests for MultiAgentConfig."""

    def test_default_values(self) -> None:
        """Test default configuration values."""
        config = MultiAgentConfig()
        assert config.num_agents == 2
        assert config.obs_dim == 17
        assert config.action_dim == 4
        assert config.max_steps == 500
        assert config.shared_reward is False

    def test_num_agents_bounds(self) -> None:
        """Test num_agents validation."""
        # Valid values
        config = MultiAgentConfig(num_agents=1)
        assert config.num_agents == 1

        config = MultiAgentConfig(num_agents=10)
        assert config.num_agents == 10

        # Invalid values
        with pytest.raises(ValueError):
            MultiAgentConfig(num_agents=0)

        with pytest.raises(ValueError):
            MultiAgentConfig(num_agents=11)

    def test_shared_reward_boolean(self) -> None:
        """Test shared_reward is boolean."""
        config = MultiAgentConfig(shared_reward=True)
        assert config.shared_reward is True

        config = MultiAgentConfig(shared_reward=False)
        assert config.shared_reward is False


# Integration tests require ROS2 running
@pytest.mark.integration
class TestWarehouseParallelEnvIntegration:
    """Integration tests requiring ROS2 services."""

    def test_reset_returns_dict(self) -> None:
        """Test that reset returns observation dict."""
        env = WarehouseParallelEnv()
        try:
            observations, infos = env.reset()

            assert isinstance(observations, dict)
            assert isinstance(infos, dict)
            for agent in env.agents:
                assert agent in observations
                assert agent in infos
        finally:
            env.close()

    def test_step_returns_dicts(self) -> None:
        """Test that step returns all required dicts."""
        env = WarehouseParallelEnv()
        try:
            env.reset()

            actions = {
                agent: env.action_space(agent).sample() for agent in env.agents
            }
            obs, rewards, terms, truncs, infos = env.step(actions)

            assert isinstance(obs, dict)
            assert isinstance(rewards, dict)
            assert isinstance(terms, dict)
            assert isinstance(truncs, dict)
            assert isinstance(infos, dict)
        finally:
            env.close()
