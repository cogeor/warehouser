"""Pydantic configuration models for training."""

import math
from enum import IntEnum

from pydantic import BaseModel, Field, field_validator


class ObservationVersion(IntEnum):
    """Observation space versions with their corresponding dimensions.

    Each version represents a different observation format:
    - V1_Basic: 5 dimensions - [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
    - V2_Lidar: 63 dimensions - [lidar_ranges(60), goal_bearing, goal_dist, is_carrying]
    - V3_MultiRobot: 14 dimensions - Per-robot observations for multi-agent training
      (5 ego state + 3 * 3 other robots)
    """

    V1_Basic = 5
    V2_Lidar = 63
    V3_MultiRobot = 14


class RobotState(BaseModel):
    """Robot state representation."""

    x: float
    y: float
    theta: float
    v: float = 0.0
    omega: float = 0.0
    is_carrying: bool = False
    carried_object_id: str | None = None

    @field_validator("theta")
    @classmethod
    def theta_must_be_in_rep103_range(cls, v: float) -> float:
        """Validate theta is in [-pi, pi] per REP-103 conventions."""
        if not (-math.pi <= v <= math.pi):
            raise ValueError(
                f"theta must be in [-pi, pi] for REP-103 compliance, got {v:.4f}. "
                f"Valid range: [{-math.pi:.4f}, {math.pi:.4f}]. "
                "Normalize using math.atan2(sin(theta), cos(theta))."
            )
        return v


class Goal(BaseModel):
    """Goal/target representation."""

    x: float
    y: float
    theta: float | None = None
    color: str | None = None
    active: bool = True

    @field_validator("theta")
    @classmethod
    def theta_must_be_in_rep103_range(cls, v: float | None) -> float | None:
        """Validate theta is in [-pi, pi] per REP-103 conventions."""
        if v is not None and not (-math.pi <= v <= math.pi):
            raise ValueError(
                f"theta must be in [-pi, pi] for REP-103 compliance, got {v:.4f}. "
                f"Valid range: [{-math.pi:.4f}, {math.pi:.4f}]. "
                "Normalize using math.atan2(sin(theta), cos(theta))."
            )
        return v


class RewardConfig(BaseModel):
    """Reward shaping configuration."""

    progress_weight: float = Field(default=1.0, description="Weight for progress toward goal")
    collision_penalty: float = Field(default=-100.0, description="Penalty for collision")
    success_bonus: float = Field(default=100.0, description="Bonus for reaching goal")
    pickup_bonus: float = Field(default=50.0, description="Bonus for picking up object")
    time_penalty: float = Field(default=-0.1, description="Penalty per timestep")
    goal_threshold: float = Field(default=0.5, description="Distance to consider goal reached")

    @field_validator(
        "progress_weight",
        "collision_penalty",
        "success_bonus",
        "pickup_bonus",
        "time_penalty",
    )
    @classmethod
    def weights_must_be_bounded(cls, v: float, info: object) -> float:
        """Validate reward weights are within reasonable bounds [-1000, 1000]."""
        if not (-1000.0 <= v <= 1000.0):
            # Get field name from info (ValidationInfo object)
            field_name = getattr(info, "field_name", "weight")
            raise ValueError(
                f"{field_name} must be in [-1000, 1000], got {v}. "
                "Extreme reward values can destabilize training. "
                "Consider using smaller magnitudes (typical range: -100 to 100)."
            )
        return v

    @field_validator("goal_threshold")
    @classmethod
    def goal_threshold_must_be_positive(cls, v: float) -> float:
        """Validate goal_threshold is positive."""
        if v <= 0:
            raise ValueError(
                f"goal_threshold must be > 0, got {v}. "
                "This is the distance threshold for considering a goal reached."
            )
        return v


class MultiAgentConfig(BaseModel):
    """Configuration for multi-agent PettingZoo environments."""

    num_agents: int = Field(default=2, ge=1, le=10, description="Number of agents")
    obs_dim: int = Field(
        default=ObservationVersion.V3_MultiRobot,
        description="Observation dimension (V3 multi-robot)",
    )
    action_dim: int = Field(default=4, description="Action dimension")
    max_steps: int = Field(default=500, description="Maximum steps per episode")
    shared_reward: bool = Field(default=False, description="Use shared team reward")

    @field_validator("num_agents")
    @classmethod
    def num_agents_must_be_valid(cls, v: int) -> int:
        """Validate num_agents is in valid range [1, 10]."""
        if not (1 <= v <= 10):
            raise ValueError(
                f"num_agents must be in [1, 10], got {v}. "
                "Multi-agent environments support 1-10 agents."
            )
        return v


class ActionConfig(BaseModel):
    """Action wrapper configuration for velocity limits and smoothing.

    Controls the action transformation pipeline:
    1. Scaling: Convert [-1, 1] policy outputs to physical velocity limits
    2. Smoothing: EMA filter for smooth transitions
    3. Acceleration: Enforce maximum acceleration/deceleration
    4. Safety: Hard limits on all action dimensions

    Example:
        >>> config = ActionConfig(
        ...     velocity_limits={"linear": 1.0, "angular": 2.0},
        ...     smoothing_alpha=0.3,
        ...     max_acceleration={"linear": 2.0, "angular": 4.0},
        ... )
    """

    velocity_limits: dict[str, float] = Field(
        default={"linear": 1.0, "angular": 2.0},
        description="Maximum velocities: linear (m/s) and angular (rad/s)",
    )
    smoothing_alpha: float = Field(
        default=0.3,
        description="EMA smoothing factor in (0, 1]. Higher = less smoothing.",
    )
    max_acceleration: dict[str, float] = Field(
        default={"linear": 2.0, "angular": 4.0},
        description="Maximum accelerations: linear (m/s^2) and angular (rad/s^2)",
    )
    hard_limits: dict[str, tuple[float, float]] = Field(
        default={
            "linear": (-1.0, 1.0),
            "angular": (-2.0, 2.0),
            "pick": (-1.0, 1.0),
            "place": (-1.0, 1.0),
        },
        description="Hard safety limits for each action dimension (min, max)",
    )
    dt: float = Field(
        default=0.05,
        description="Simulation timestep in seconds (default: 20 Hz)",
    )

    @field_validator("smoothing_alpha")
    @classmethod
    def smoothing_alpha_must_be_valid(cls, v: float) -> float:
        """Validate smoothing alpha is in (0, 1]."""
        if not (0.0 < v <= 1.0):
            raise ValueError(
                f"smoothing_alpha must be in (0, 1], got {v}. "
                "Use 1.0 for no smoothing, lower values for more smoothing."
            )
        return v

    @field_validator("velocity_limits")
    @classmethod
    def velocity_limits_must_be_valid(cls, v: dict[str, float]) -> dict[str, float]:
        """Validate velocity limits contain required keys and positive values."""
        required = {"linear", "angular"}
        missing = required - set(v.keys())
        if missing:
            raise ValueError(
                f"velocity_limits missing required keys: {missing}. "
                "Must contain 'linear' and 'angular'."
            )
        for key in required:
            if v[key] <= 0:
                raise ValueError(
                    f"velocity_limits['{key}'] must be > 0, got {v[key]}. "
                    "Velocity limits must be positive values."
                )
        return v

    @field_validator("max_acceleration")
    @classmethod
    def max_acceleration_must_be_valid(cls, v: dict[str, float]) -> dict[str, float]:
        """Validate acceleration limits contain required keys and positive values."""
        required = {"linear", "angular"}
        missing = required - set(v.keys())
        if missing:
            raise ValueError(
                f"max_acceleration missing required keys: {missing}. "
                "Must contain 'linear' and 'angular'."
            )
        for key in required:
            if v[key] <= 0:
                raise ValueError(
                    f"max_acceleration['{key}'] must be > 0, got {v[key]}. "
                    "Acceleration limits must be positive values."
                )
        return v

    @field_validator("hard_limits")
    @classmethod
    def hard_limits_must_be_valid(
        cls, v: dict[str, tuple[float, float]]
    ) -> dict[str, tuple[float, float]]:
        """Validate hard limits contain required keys and valid ranges."""
        required = {"linear", "angular", "pick", "place"}
        missing = required - set(v.keys())
        if missing:
            raise ValueError(
                f"hard_limits missing required keys: {missing}. "
                "Must contain 'linear', 'angular', 'pick', and 'place'."
            )
        for key in required:
            low, high = v[key]
            if low >= high:
                raise ValueError(
                    f"hard_limits['{key}'] invalid: low ({low}) must be < high ({high}). "
                    "Each limit must be a (min, max) tuple with min < max."
                )
        return v

    @field_validator("dt")
    @classmethod
    def dt_must_be_positive(cls, v: float) -> float:
        """Validate dt is positive."""
        if v <= 0:
            raise ValueError(
                f"dt must be > 0, got {v}. Timestep must be a positive value in seconds."
            )
        return v


class EnvConfig(BaseModel):
    """Environment configuration.

    Observation dimension (obs_dim) should match the ROS observation builder version:
    - V1_Basic (5): [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
    - V2_Lidar (63): [lidar_ranges(60), goal_bearing, goal_dist, is_carrying]
    - V3_MultiRobot (14): Per-robot observations for multi-agent training

    Use ObservationVersion enum for clarity: obs_dim=ObservationVersion.V2_Lidar
    """

    obs_dim: int = Field(
        default=ObservationVersion.V1_Basic,
        description="Observation dimension (V1=5, V2=63, V3=14)",
    )
    action_dim: int = Field(default=4, description="Action dimension")
    max_steps: int = Field(default=500, description="Maximum steps per episode")

    # World settings
    world_width: float = Field(default=10.0, description="World width in meters")
    world_height: float = Field(default=10.0, description="World height in meters")

    # Robot settings
    robot_spawn: tuple[float, float, float] = Field(
        default=(1.0, 1.0, 0.0), description="Robot spawn position (x, y, theta)"
    )

    # Reward config
    reward: RewardConfig = Field(default_factory=RewardConfig)

    # Action wrapper config
    action: ActionConfig = Field(default_factory=ActionConfig)

    @field_validator("obs_dim", "action_dim")
    @classmethod
    def dimensions_must_be_positive(cls, v: int, info: object) -> int:
        """Validate observation and action dimensions are positive."""
        field_name = getattr(info, "field_name", "dimension")
        if v <= 0:
            raise ValueError(
                f"{field_name} must be > 0, got {v}. "
                "Neural network dimensions must be positive integers."
            )
        return v

    @field_validator("max_steps")
    @classmethod
    def max_steps_must_be_positive(cls, v: int) -> int:
        """Validate max_steps is positive."""
        if v <= 0:
            raise ValueError(
                f"max_steps must be > 0, got {v}. Episodes need at least one step to run."
            )
        return v

    @field_validator("world_width", "world_height")
    @classmethod
    def world_size_must_be_positive(cls, v: float, info: object) -> float:
        """Validate world dimensions are positive."""
        field_name = getattr(info, "field_name", "world_size")
        if v <= 0:
            raise ValueError(
                f"{field_name} must be > 0, got {v}. "
                "World dimensions must be positive values in meters."
            )
        return v

    @field_validator("robot_spawn")
    @classmethod
    def robot_spawn_theta_must_be_valid(
        cls, v: tuple[float, float, float]
    ) -> tuple[float, float, float]:
        """Validate robot spawn theta is in [-pi, pi] per REP-103."""
        x, y, theta = v
        if not (-math.pi <= theta <= math.pi):
            raise ValueError(
                f"robot_spawn theta must be in [-pi, pi] for REP-103 compliance, "
                f"got {theta:.4f}. Valid range: [{-math.pi:.4f}, {math.pi:.4f}]. "
                "Normalize using math.atan2(sin(theta), cos(theta))."
            )
        return v


class TrainingConfig(BaseModel):
    """PPO training hyperparameters."""

    # Algorithm settings
    learning_rate: float = Field(default=3e-4, description="Learning rate")
    n_steps: int = Field(default=2048, description="Steps per rollout")
    batch_size: int = Field(default=64, description="Minibatch size")
    n_epochs: int = Field(default=10, description="Epochs per update")
    gamma: float = Field(default=0.99, description="Discount factor")
    gae_lambda: float = Field(default=0.95, description="GAE lambda")
    clip_range: float = Field(default=0.2, description="PPO clip range")
    ent_coef: float = Field(default=0.01, description="Entropy coefficient")
    vf_coef: float = Field(default=0.5, description="Value function coefficient")
    max_grad_norm: float = Field(default=0.5, description="Max gradient norm")

    # Training settings
    total_timesteps: int = Field(default=1_000_000, description="Total training timesteps")
    eval_freq: int = Field(default=10_000, description="Evaluation frequency")
    n_eval_episodes: int = Field(default=10, description="Number of evaluation episodes")
    save_freq: int = Field(default=50_000, description="Checkpoint save frequency")

    # Policy network
    policy_hidden: list[int] = Field(
        default=[64, 64], description="Policy network hidden layer sizes"
    )
    value_hidden: list[int] = Field(
        default=[64, 64], description="Value network hidden layer sizes"
    )

    # Paths
    checkpoint_dir: str = Field(default="checkpoints", description="Checkpoint directory")
    log_dir: str = Field(default="logs", description="Tensorboard log directory")

    # VecNormalize wrapper settings
    norm_obs: bool = Field(default=True, description="Normalize observations")
    norm_reward: bool = Field(default=True, description="Normalize rewards")
    clip_obs: float = Field(default=10.0, description="Clip normalized observations")
    clip_reward: float = Field(default=10.0, description="Clip normalized rewards")

    @field_validator("learning_rate")
    @classmethod
    def learning_rate_must_be_valid(cls, v: float) -> float:
        """Validate learning rate is in valid range (0, 1]."""
        if not (0 < v <= 1.0):
            raise ValueError(
                f"learning_rate must be in (0, 1], got {v}. "
                "Typical values: 1e-5 to 1e-2. Recommended for PPO: 3e-4."
            )
        return v

    @field_validator(
        "n_steps",
        "batch_size",
        "n_epochs",
        "total_timesteps",
        "eval_freq",
        "n_eval_episodes",
        "save_freq",
    )
    @classmethod
    def training_counts_must_be_positive(cls, v: int, info: object) -> int:
        """Validate training count parameters are positive."""
        field_name = getattr(info, "field_name", "count")
        if v <= 0:
            raise ValueError(
                f"{field_name} must be > 0, got {v}. Training parameters must be positive integers."
            )
        return v

    @field_validator("gamma", "gae_lambda")
    @classmethod
    def discount_factors_must_be_valid(cls, v: float, info: object) -> float:
        """Validate discount factors are in (0, 1]."""
        field_name = getattr(info, "field_name", "discount")
        if not (0 < v <= 1.0):
            raise ValueError(
                f"{field_name} must be in (0, 1], got {v}. "
                "Discount factors must be positive and at most 1. "
                "Typical values: gamma=0.99, gae_lambda=0.95."
            )
        return v

    @field_validator("clip_range")
    @classmethod
    def clip_range_must_be_valid(cls, v: float) -> float:
        """Validate PPO clip range is in (0, 0.5]."""
        if not (0 < v <= 0.5):
            raise ValueError(
                f"clip_range must be in (0, 0.5], got {v}. "
                "PPO clip range limits policy updates. Typical value: 0.2. "
                "Values > 0.5 defeat the purpose of clipping."
            )
        return v

    @field_validator("ent_coef", "vf_coef", "max_grad_norm")
    @classmethod
    def coefficients_must_be_non_negative(cls, v: float, info: object) -> float:
        """Validate coefficients are non-negative."""
        field_name = getattr(info, "field_name", "coefficient")
        if v < 0:
            raise ValueError(
                f"{field_name} must be >= 0, got {v}. "
                "Loss coefficients and gradient norms cannot be negative."
            )
        return v

    @field_validator("policy_hidden", "value_hidden")
    @classmethod
    def hidden_layers_must_be_valid(cls, v: list[int], info: object) -> list[int]:
        """Validate hidden layer sizes are positive."""
        field_name = getattr(info, "field_name", "hidden_layers")
        if not v:
            raise ValueError(
                f"{field_name} must have at least one layer. "
                "Use a list of positive integers, e.g., [64, 64]."
            )
        for i, size in enumerate(v):
            if size <= 0:
                raise ValueError(
                    f"{field_name}[{i}] must be > 0, got {size}. "
                    "Neural network layer sizes must be positive integers."
                )
        return v

    @field_validator("clip_obs", "clip_reward")
    @classmethod
    def clip_values_must_be_positive(cls, v: float, info: object) -> float:
        """Validate clip values are positive."""
        field_name = getattr(info, "field_name", "clip_value")
        if v <= 0:
            raise ValueError(
                f"{field_name} must be > 0, got {v}. "
                "Clip values for VecNormalize must be positive. "
                "Typical value: 10.0."
            )
        return v
