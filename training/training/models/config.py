"""Pydantic configuration models for training."""

from pydantic import BaseModel, Field


class RobotState(BaseModel):
    """Robot state representation."""

    x: float
    y: float
    theta: float
    v: float = 0.0
    omega: float = 0.0
    is_carrying: bool = False
    carried_object_id: str | None = None


class Goal(BaseModel):
    """Goal/target representation."""

    x: float
    y: float
    color: str | None = None
    active: bool = True


class RewardConfig(BaseModel):
    """Reward shaping configuration."""

    progress_weight: float = Field(default=1.0, description="Weight for progress toward goal")
    collision_penalty: float = Field(default=-100.0, description="Penalty for collision")
    success_bonus: float = Field(default=100.0, description="Bonus for reaching goal")
    pickup_bonus: float = Field(default=50.0, description="Bonus for picking up object")
    time_penalty: float = Field(default=-0.1, description="Penalty per timestep")
    goal_threshold: float = Field(default=0.5, description="Distance to consider goal reached")


class EnvConfig(BaseModel):
    """Environment configuration."""

    obs_dim: int = Field(default=8, description="Observation dimension")
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
