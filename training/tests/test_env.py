"""Tests for the Gymnasium environment."""

import numpy as np
import pytest

from training.envs.ros_env import ROSGymEnv
from training.models.config import EnvConfig


class TestROSGymEnv:
    @pytest.fixture
    def config(self) -> EnvConfig:
        return EnvConfig(
            obs_dim=8,
            action_dim=4,
            max_steps=500,
        )

    def test_init_creates_spaces(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)

        assert env.observation_space.shape == (8,)
        assert env.action_space.shape == (4,)
        # Cast to Box for type checking
        from gymnasium.spaces import Box

        action_space = env.action_space
        assert isinstance(action_space, Box)
        assert action_space.low.min() == -1.0
        assert action_space.high.max() == 1.0

    def test_action_space_dtype(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)
        assert env.action_space.dtype == np.float32

    def test_observation_space_dtype(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)
        assert env.observation_space.dtype == np.float32

    def test_reset_returns_error_without_ros(self, config: EnvConfig) -> None:
        """Without ROS running, reset should return zeros with error info."""
        env = ROSGymEnv(config)

        # This will fail to connect to ROS but should not raise
        obs, info = env.reset()

        assert obs.shape == (config.obs_dim,)
        assert obs.dtype == np.float32
        # Should have error info since ROS is not running
        assert "error" in info

    def test_step_validates_action_shape(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)

        # Wrong action shape should raise
        with pytest.raises(ValueError):
            env.step(np.array([0.0, 0.0]))  # Only 2 elements instead of 4

    def test_close_is_safe(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)
        env.close()  # Should not raise
        env.close()  # Double close should also be safe

    def test_render_is_noop(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)
        env.render()  # Should not raise

    def test_config_stored(self, config: EnvConfig) -> None:
        env = ROSGymEnv(config)
        assert env.config.obs_dim == 8
        assert env.config.max_steps == 500

    def test_default_config(self) -> None:
        env = ROSGymEnv()  # No config provided
        assert env.config.obs_dim == 8
        assert env.config.action_dim == 4
