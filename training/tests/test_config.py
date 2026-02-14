"""Tests for configuration models."""

import math

import pytest
from pydantic import ValidationError

from training.models.config import (
    EnvConfig,
    Goal,
    ObservationVersion,
    RewardConfig,
    RobotState,
    TrainingConfig,
)


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

    def test_theta_at_pi_boundary(self) -> None:
        """Test theta at exactly pi and -pi are valid."""
        state_positive = RobotState(x=0.0, y=0.0, theta=math.pi)
        assert state_positive.theta == math.pi
        state_negative = RobotState(x=0.0, y=0.0, theta=-math.pi)
        assert state_negative.theta == -math.pi

    def test_theta_rejects_out_of_range(self) -> None:
        """Test theta outside [-pi, pi] is rejected with clear message."""
        with pytest.raises(ValidationError) as exc_info:
            RobotState(x=0.0, y=0.0, theta=10.0)
        error_msg = str(exc_info.value)
        assert "REP-103" in error_msg
        assert "[-pi, pi]" in error_msg
        assert "atan2" in error_msg  # Suggests fix

    def test_theta_rejects_negative_out_of_range(self) -> None:
        """Test large negative theta is rejected."""
        with pytest.raises(ValidationError):
            RobotState(x=0.0, y=0.0, theta=-4.0)


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

    def test_theta_is_optional(self) -> None:
        """Test theta can be None."""
        goal = Goal(x=5.0, y=5.0, theta=None)
        assert goal.theta is None

    def test_theta_valid_when_provided(self) -> None:
        """Test valid theta is accepted."""
        goal = Goal(x=5.0, y=5.0, theta=1.5)
        assert goal.theta == 1.5

    def test_theta_rejects_out_of_range(self) -> None:
        """Test theta outside [-pi, pi] is rejected."""
        with pytest.raises(ValidationError) as exc_info:
            Goal(x=5.0, y=5.0, theta=5.0)
        error_msg = str(exc_info.value)
        assert "REP-103" in error_msg


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

    def test_weights_within_bounds(self) -> None:
        """Test weights at boundary values are accepted."""
        config = RewardConfig(
            progress_weight=1000.0,
            collision_penalty=-1000.0,
        )
        assert config.progress_weight == 1000.0
        assert config.collision_penalty == -1000.0

    def test_weights_reject_extreme_values(self) -> None:
        """Test weights outside [-1000, 1000] are rejected."""
        with pytest.raises(ValidationError) as exc_info:
            RewardConfig(success_bonus=2000.0)
        error_msg = str(exc_info.value)
        assert "[-1000, 1000]" in error_msg
        assert "destabilize" in error_msg

    def test_goal_threshold_must_be_positive(self) -> None:
        """Test goal_threshold rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            RewardConfig(goal_threshold=0.0)
        error_msg = str(exc_info.value)
        assert "goal_threshold" in error_msg
        assert "> 0" in error_msg

        with pytest.raises(ValidationError):
            RewardConfig(goal_threshold=-0.5)


class TestEnvConfig:
    def test_defaults(self) -> None:
        config = EnvConfig()
        # V1 ego-centric: [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
        assert config.obs_dim == 5
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

    def test_max_steps_must_be_positive(self) -> None:
        """Test max_steps rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            EnvConfig(max_steps=0)
        error_msg = str(exc_info.value)
        assert "max_steps" in error_msg
        assert "> 0" in error_msg

        with pytest.raises(ValidationError):
            EnvConfig(max_steps=-10)

    def test_world_dimensions_must_be_positive(self) -> None:
        """Test world width and height reject non-positive values."""
        with pytest.raises(ValidationError) as exc_info:
            EnvConfig(world_width=0.0)
        error_msg = str(exc_info.value)
        assert "> 0" in error_msg

        with pytest.raises(ValidationError):
            EnvConfig(world_height=-5.0)

    def test_obs_dim_must_be_positive(self) -> None:
        """Test obs_dim rejects non-positive values."""
        with pytest.raises(ValidationError) as exc_info:
            EnvConfig(obs_dim=0)
        error_msg = str(exc_info.value)
        assert "> 0" in error_msg
        assert "dimension" in error_msg.lower()

    def test_robot_spawn_theta_validation(self) -> None:
        """Test robot_spawn theta is validated for REP-103 compliance."""
        # Valid spawn
        config = EnvConfig(robot_spawn=(1.0, 1.0, math.pi))
        assert config.robot_spawn == (1.0, 1.0, math.pi)

        # Invalid spawn theta
        with pytest.raises(ValidationError) as exc_info:
            EnvConfig(robot_spawn=(1.0, 1.0, 5.0))
        error_msg = str(exc_info.value)
        assert "REP-103" in error_msg
        assert "robot_spawn" in error_msg

    def test_obs_dim_v1_basic(self) -> None:
        """Test V1_Basic observation dimension (default)."""
        config = EnvConfig()
        # Ego-centric: [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
        assert config.obs_dim == 5
        assert config.obs_dim == ObservationVersion.V1_Basic

    def test_obs_dim_v2_lidar(self) -> None:
        """Test V2_Lidar observation dimension."""
        config = EnvConfig(obs_dim=ObservationVersion.V2_Lidar)
        assert config.obs_dim == 63
        assert config.obs_dim == ObservationVersion.V2_Lidar

    def test_obs_dim_v3_multi_robot(self) -> None:
        """Test V3_MultiRobot observation dimension."""
        config = EnvConfig(obs_dim=ObservationVersion.V3_MultiRobot)
        assert config.obs_dim == 14  # 5 (ego) + 3 * 3 (other robots)
        assert config.obs_dim == ObservationVersion.V3_MultiRobot

    def test_obs_dim_accepts_integer(self) -> None:
        """Test obs_dim can be set with plain integer."""
        config = EnvConfig(obs_dim=63)
        assert config.obs_dim == 63

    def test_obs_dim_serialization(self) -> None:
        """Test obs_dim serializes and deserializes correctly."""
        config = EnvConfig(obs_dim=ObservationVersion.V2_Lidar)
        data = config.model_dump()
        assert data["obs_dim"] == 63
        restored = EnvConfig.model_validate(data)
        assert restored.obs_dim == 63


class TestObservationVersion:
    """Tests for ObservationVersion enum."""

    def test_version_values(self) -> None:
        """Test ObservationVersion enum values."""
        assert int(ObservationVersion.V1_Basic) == 5  # Ego-centric
        assert int(ObservationVersion.V2_Lidar) == 63
        assert int(ObservationVersion.V3_MultiRobot) == 14  # 5 + 3*3

    def test_version_is_int_compatible(self) -> None:
        """Test ObservationVersion can be used as int."""
        assert int(ObservationVersion.V1_Basic) == 5  # Ego-centric
        assert int(ObservationVersion.V2_Lidar) == 63
        assert int(ObservationVersion.V3_MultiRobot) == 14  # 5 + 3*3

    def test_version_comparison(self) -> None:
        """Test ObservationVersion can be compared with integers."""
        assert int(ObservationVersion.V2_Lidar) == 63
        assert 63 == int(ObservationVersion.V2_Lidar)
        assert ObservationVersion.V2_Lidar > ObservationVersion.V1_Basic


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

    def test_learning_rate_must_be_positive(self) -> None:
        """Test learning_rate rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(learning_rate=0.0)
        error_msg = str(exc_info.value)
        assert "learning_rate" in error_msg
        assert "(0, 1]" in error_msg

        with pytest.raises(ValidationError):
            TrainingConfig(learning_rate=-0.001)

    def test_learning_rate_rejects_greater_than_one(self) -> None:
        """Test learning_rate > 1 is rejected."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(learning_rate=1.5)
        error_msg = str(exc_info.value)
        assert "learning_rate" in error_msg
        assert "PPO" in error_msg  # Mentions recommended value

    def test_learning_rate_at_boundary(self) -> None:
        """Test learning_rate at exactly 1.0 is valid."""
        config = TrainingConfig(learning_rate=1.0)
        assert config.learning_rate == 1.0

    def test_batch_size_must_be_positive(self) -> None:
        """Test batch_size rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(batch_size=0)
        error_msg = str(exc_info.value)
        assert "batch_size" in error_msg
        assert "> 0" in error_msg

    def test_gamma_must_be_in_valid_range(self) -> None:
        """Test gamma rejects values outside (0, 1]."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(gamma=0.0)
        error_msg = str(exc_info.value)
        assert "gamma" in error_msg
        assert "(0, 1]" in error_msg

        with pytest.raises(ValidationError):
            TrainingConfig(gamma=1.5)

        # Valid boundary
        config = TrainingConfig(gamma=1.0)
        assert config.gamma == 1.0

    def test_clip_range_must_be_in_valid_range(self) -> None:
        """Test clip_range rejects values outside (0, 0.5]."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(clip_range=0.0)
        error_msg = str(exc_info.value)
        assert "clip_range" in error_msg
        assert "(0, 0.5]" in error_msg

        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(clip_range=0.6)
        error_msg = str(exc_info.value)
        assert "defeat the purpose" in error_msg

        # Valid boundary
        config = TrainingConfig(clip_range=0.5)
        assert config.clip_range == 0.5

    def test_coefficients_must_be_non_negative(self) -> None:
        """Test ent_coef, vf_coef, max_grad_norm reject negative values."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(ent_coef=-0.01)
        error_msg = str(exc_info.value)
        assert ">= 0" in error_msg

        with pytest.raises(ValidationError):
            TrainingConfig(vf_coef=-0.5)

        with pytest.raises(ValidationError):
            TrainingConfig(max_grad_norm=-1.0)

        # Zero is valid
        config = TrainingConfig(ent_coef=0.0)
        assert config.ent_coef == 0.0

    def test_hidden_layers_must_not_be_empty(self) -> None:
        """Test policy_hidden and value_hidden reject empty lists."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(policy_hidden=[])
        error_msg = str(exc_info.value)
        assert "at least one layer" in error_msg

        with pytest.raises(ValidationError):
            TrainingConfig(value_hidden=[])

    def test_hidden_layers_reject_non_positive_sizes(self) -> None:
        """Test hidden layer sizes must be positive."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(policy_hidden=[64, 0, 32])
        error_msg = str(exc_info.value)
        assert "> 0" in error_msg
        assert "[1]" in error_msg  # Index of invalid element

        with pytest.raises(ValidationError):
            TrainingConfig(value_hidden=[-1, 64])

    def test_normalization_defaults(self) -> None:
        """Test VecNormalize configuration defaults."""
        config = TrainingConfig()
        assert config.norm_obs is True
        assert config.norm_reward is True
        assert config.clip_obs == 10.0
        assert config.clip_reward == 10.0

    def test_normalization_custom_values(self) -> None:
        """Test VecNormalize fields accept custom values."""
        config = TrainingConfig(
            norm_obs=False,
            norm_reward=False,
            clip_obs=5.0,
            clip_reward=15.0,
        )
        assert config.norm_obs is False
        assert config.norm_reward is False
        assert config.clip_obs == 5.0
        assert config.clip_reward == 15.0

    def test_clip_obs_must_be_positive(self) -> None:
        """Test clip_obs rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(clip_obs=0.0)
        error_msg = str(exc_info.value)
        assert "clip_obs" in error_msg
        assert "> 0" in error_msg

        with pytest.raises(ValidationError):
            TrainingConfig(clip_obs=-5.0)

    def test_clip_reward_must_be_positive(self) -> None:
        """Test clip_reward rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(clip_reward=0.0)
        error_msg = str(exc_info.value)
        assert "clip_reward" in error_msg
        assert "> 0" in error_msg

        with pytest.raises(ValidationError):
            TrainingConfig(clip_reward=-10.0)

    def test_serialization_with_normalization_fields(self) -> None:
        """Test serialization roundtrip includes normalization fields."""
        config = TrainingConfig(
            norm_obs=False,
            norm_reward=True,
            clip_obs=8.0,
            clip_reward=12.0,
        )
        data = config.model_dump()

        # Verify normalization fields are in serialized data
        assert data["norm_obs"] is False
        assert data["norm_reward"] is True
        assert data["clip_obs"] == 8.0
        assert data["clip_reward"] == 12.0

        # Verify deserialization
        restored = TrainingConfig.model_validate(data)
        assert restored.norm_obs is False
        assert restored.norm_reward is True
        assert restored.clip_obs == 8.0
        assert restored.clip_reward == 12.0

    def test_n_epochs_must_be_positive(self) -> None:
        """Test n_epochs rejects zero and negative values."""
        with pytest.raises(ValidationError) as exc_info:
            TrainingConfig(n_epochs=0)
        error_msg = str(exc_info.value)
        assert "n_epochs" in error_msg
        assert "> 0" in error_msg
