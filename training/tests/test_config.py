"""Tests for configuration models."""

import pytest
from pydantic import ValidationError

from training.models.config import EnvConfig, Goal, RewardConfig, RobotState, TrainingConfig


class TestRobotState:
    def test_create_with_defaults(self) -> None:
        state = RobotState(x=1.0, y=2.0, theta=0.5)
        assert state.x == 1.0
        assert state.y == 2.0
        assert state.theta == 0.5
        assert state.v == 0.0
        assert state.omega == 0.0
        assert state.is_carrying is False
        assert state.carried_object_id is None

    def test_create_with_all_fields(self) -> None:
        state = RobotState(
            x=1.0,
            y=2.0,
            theta=0.5,
            v=0.3,
            omega=0.1,
            is_carrying=True,
            carried_object_id="obj_1",
        )
        assert state.is_carrying is True
        assert state.carried_object_id == "obj_1"

    def test_validation_rejects_invalid_type(self) -> None:
        with pytest.raises(ValidationError):
            RobotState(x="invalid", y=0.0, theta=0.0)

    def test_model_serialization(self) -> None:
        state = RobotState(x=1.0, y=2.0, theta=0.5)
        data = state.model_dump()
        assert data["x"] == 1.0
        restored = RobotState.model_validate(data)
        assert restored == state


class TestGoal:
    def test_create_with_defaults(self) -> None:
        goal = Goal(x=5.0, y=5.0)
        assert goal.x == 5.0
        assert goal.y == 5.0
        assert goal.color is None
        assert goal.active is True

    def test_create_with_color(self) -> None:
        goal = Goal(x=5.0, y=5.0, color="red")
        assert goal.color == "red"


class TestRewardConfig:
    def test_defaults(self) -> None:
        config = RewardConfig()
        assert config.progress_weight == 1.0
        assert config.collision_penalty == -100.0
        assert config.success_bonus == 100.0
        assert config.pickup_bonus == 50.0
        assert config.time_penalty == -0.1
        assert config.goal_threshold == 0.5

    def test_custom_values(self) -> None:
        config = RewardConfig(progress_weight=2.0, success_bonus=200.0)
        assert config.progress_weight == 2.0
        assert config.success_bonus == 200.0


class TestEnvConfig:
    def test_defaults(self) -> None:
        config = EnvConfig()
        assert config.obs_dim == 8
        assert config.action_dim == 4
        assert config.max_steps == 500
        assert config.world_width == 10.0
        assert config.world_height == 10.0
        assert config.robot_spawn == (1.0, 1.0, 0.0)
        assert isinstance(config.reward, RewardConfig)

    def test_custom_reward_config(self) -> None:
        reward = RewardConfig(success_bonus=150.0)
        config = EnvConfig(reward=reward)
        assert config.reward.success_bonus == 150.0


class TestTrainingConfig:
    def test_defaults(self) -> None:
        config = TrainingConfig()
        assert config.learning_rate == 3e-4
        assert config.n_steps == 2048
        assert config.batch_size == 64
        assert config.total_timesteps == 1_000_000
        assert config.policy_hidden == [64, 64]
        assert config.value_hidden == [64, 64]

    def test_custom_network(self) -> None:
        config = TrainingConfig(policy_hidden=[128, 128, 64])
        assert config.policy_hidden == [128, 128, 64]

    def test_serialization_roundtrip(self) -> None:
        config = TrainingConfig(total_timesteps=500_000)
        data = config.model_dump()
        restored = TrainingConfig.model_validate(data)
        assert restored.total_timesteps == 500_000
