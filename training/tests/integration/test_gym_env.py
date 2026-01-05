"""Integration tests for the Gymnasium environment with ROS2.

These tests require the simulation, observations, and rl_bridge nodes to be running:
    ros2 launch warehouser_bringup bringup.launch.py
"""

import numpy as np
import pytest

# Skip all tests if ROS2 is not available
pytest.importorskip("rclpy")

from training.envs.ros_env import ROSGymEnv
from training.models.config import EnvConfig


class TestROSGymEnvIntegration:
    """Integration tests for ROSGymEnv with actual ROS2 backend."""

    @pytest.fixture
    def config(self) -> EnvConfig:
        return EnvConfig(
            obs_dim=8,
            action_dim=4,
            max_steps=500,
        )

    @pytest.fixture
    def env(self, config: EnvConfig) -> ROSGymEnv:
        """Create environment and clean up after test."""
        env = ROSGymEnv(config)
        yield env
        env.close()

    def test_reset_returns_valid_observation(self, env: ROSGymEnv) -> None:
        """Test reset returns properly shaped observation."""
        obs, info = env.reset(seed=42)

        assert obs.shape == (8,), f"Expected shape (8,), got {obs.shape}"
        assert obs.dtype == np.float32, f"Expected float32, got {obs.dtype}"
        assert not np.any(np.isnan(obs)), "Observation contains NaN"

    def test_step_returns_valid_transition(self, env: ROSGymEnv) -> None:
        """Test step returns all expected values."""
        env.reset()

        action = np.array([0.5, 0.0, 0.0, 0.0], dtype=np.float32)
        obs, reward, terminated, truncated, info = env.step(action)

        assert obs.shape == (8,), f"Expected shape (8,), got {obs.shape}"
        assert isinstance(reward, float), f"Expected float reward, got {type(reward)}"
        assert isinstance(terminated, bool), f"Expected bool terminated, got {type(terminated)}"
        assert isinstance(truncated, bool), f"Expected bool truncated, got {type(truncated)}"
        assert isinstance(info, dict), f"Expected dict info, got {type(info)}"

    def test_observation_values_reasonable(self, env: ROSGymEnv) -> None:
        """Test observation values are in reasonable ranges."""
        obs, _ = env.reset()

        # Robot position should be within world bounds [0, 10]
        robot_x, robot_y = obs[0], obs[1]
        assert 0.0 <= robot_x <= 10.0, f"Robot x out of bounds: {robot_x}"
        assert 0.0 <= robot_y <= 10.0, f"Robot y out of bounds: {robot_y}"

        # Theta should be in [-π, π]
        robot_theta = obs[2]
        assert -np.pi <= robot_theta <= np.pi, f"Theta out of bounds: {robot_theta}"

        # Goal distance should be non-negative
        goal_dist = obs[5]
        assert goal_dist >= 0, f"Goal distance negative: {goal_dist}"

        # Carrying flag should be 0 or 1
        is_carrying = obs[7]
        assert is_carrying in [0.0, 1.0], f"Invalid carrying flag: {is_carrying}"

    def test_action_affects_state(self, env: ROSGymEnv) -> None:
        """Test that actions actually change the state."""
        obs_before, _ = env.reset()
        robot_x_before = obs_before[0]

        # Take forward action
        action = np.array([1.0, 0.0, 0.0, 0.0], dtype=np.float32)

        # Take multiple steps
        for _ in range(5):
            obs_after, _, terminated, truncated, _ = env.step(action)
            if terminated or truncated:
                break

        robot_x_after = obs_after[0]

        # Robot should have moved forward
        assert robot_x_after != robot_x_before, "Robot position did not change"

    def test_episode_terminates(self, env: ROSGymEnv) -> None:
        """Test that episode eventually terminates or truncates."""
        env.reset()

        action = np.array([0.1, 0.0, 0.0, 0.0], dtype=np.float32)

        done = False
        steps = 0
        max_test_steps = 600  # More than max_steps to ensure truncation

        while not done and steps < max_test_steps:
            _, _, terminated, truncated, _ = env.step(action)
            done = terminated or truncated
            steps += 1

        assert done, f"Episode did not terminate after {max_test_steps} steps"

    def test_reset_after_done(self, env: ROSGymEnv) -> None:
        """Test that reset works correctly after episode ends."""
        env.reset()

        # Run until done
        action = np.array([0.5, 0.0, 0.0, 0.0], dtype=np.float32)
        for _ in range(100):
            _, _, terminated, truncated, _ = env.step(action)
            if terminated or truncated:
                break

        # Reset should work
        obs, info = env.reset()
        assert obs.shape == (8,), "Reset after done failed"
        assert not np.any(np.isnan(obs)), "Reset returned NaN observation"

    def test_deterministic_with_seed(self, config: EnvConfig) -> None:
        """Test that same seed produces same initial state."""
        env1 = ROSGymEnv(config)
        env2 = ROSGymEnv(config)

        try:
            obs1, _ = env1.reset(seed=123)
            obs2, _ = env2.reset(seed=123)

            # Robot position should be identical
            np.testing.assert_array_almost_equal(
                obs1[:2], obs2[:2], decimal=2,
                err_msg="Same seed produced different positions"
            )
        finally:
            env1.close()
            env2.close()

    def test_different_actions_different_outcomes(self, env: ROSGymEnv) -> None:
        """Test that different actions lead to different states."""
        # Forward action
        env.reset(seed=42)
        forward_action = np.array([1.0, 0.0, 0.0, 0.0], dtype=np.float32)
        obs_forward, _, _, _, _ = env.step(forward_action)

        # Rotate action
        env.reset(seed=42)
        rotate_action = np.array([0.0, 1.0, 0.0, 0.0], dtype=np.float32)
        obs_rotate, _, _, _, _ = env.step(rotate_action)

        # Observations should differ
        assert not np.allclose(obs_forward, obs_rotate), "Different actions gave same result"


class TestGymEnvWithSB3:
    """Test that the environment works with Stable-Baselines3."""

    @pytest.fixture
    def env(self) -> ROSGymEnv:
        config = EnvConfig(obs_dim=8, action_dim=4, max_steps=100)
        env = ROSGymEnv(config)
        yield env
        env.close()

    def test_check_env(self, env: ROSGymEnv) -> None:
        """Test environment passes SB3 check_env."""
        from stable_baselines3.common.env_checker import check_env

        # This will raise if environment is not compatible
        try:
            check_env(env, warn=True)
        except Exception as e:
            pytest.fail(f"Environment failed check_env: {e}")

    def test_ppo_can_predict(self, env: ROSGymEnv) -> None:
        """Test that PPO can load and predict on the environment."""
        from stable_baselines3 import PPO

        # Create model (no training)
        model = PPO("MlpPolicy", env, n_steps=64, batch_size=32, verbose=0)

        obs, _ = env.reset()
        action, _states = model.predict(obs, deterministic=True)

        assert action.shape == (4,), f"Expected action shape (4,), got {action.shape}"
        assert action.dtype == np.float32, f"Expected float32, got {action.dtype}"

    def test_ppo_can_learn_few_steps(self, env: ROSGymEnv) -> None:
        """Test that PPO can learn for a few steps without error."""
        from stable_baselines3 import PPO

        model = PPO("MlpPolicy", env, n_steps=64, batch_size=32, verbose=0)

        # Should not raise
        try:
            model.learn(total_timesteps=64)
        except Exception as e:
            pytest.fail(f"PPO learn failed: {e}")
