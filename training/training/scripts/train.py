"""PPO training script for warehouser robot.

This script provides the main entry point for training PPO agents.
All errors are raised with informative messages - no silent failures.
"""

import argparse
import json
import logging
import sys
from pathlib import Path
from typing import NoReturn

from pydantic import ValidationError
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import CheckpointCallback, EvalCallback
from stable_baselines3.common.vec_env import DummyVecEnv

from training.envs.ros_env import ROSGymEnv
from training.models.config import EnvConfig, TrainingConfig

# Configure logging to be loud about errors
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(name)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stderr)],
)
logger = logging.getLogger(__name__)


class TrainingError(Exception):
    """Raised when training fails."""

    pass


class ConfigurationError(Exception):
    """Raised when configuration is invalid."""

    pass


def _fatal_error(message: str, cause: BaseException | None = None) -> NoReturn:
    """Log error and exit with non-zero status.

    Args:
        message: Error message to display.
        cause: Original exception that caused the error.

    Raises:
        SystemExit: Always exits with code 1.
    """
    logger.error(message)
    if cause:
        logger.error(f"Caused by: {type(cause).__name__}: {cause}")
    sys.exit(1)


def make_env(config: EnvConfig) -> ROSGymEnv:
    """Create and return a ROSGymEnv instance."""
    return ROSGymEnv(config)


def train(
    env_config: EnvConfig,
    train_config: TrainingConfig,
    resume_from: str | None = None,
) -> PPO:
    """Train a PPO agent.

    Args:
        env_config: Environment configuration.
        train_config: Training hyperparameters.
        resume_from: Path to checkpoint to resume from (optional).

    Returns:
        Trained PPO model.

    Raises:
        TrainingError: If training fails for any reason.
        FileNotFoundError: If resume checkpoint does not exist.
    """
    logger.info("Starting training setup...")
    logger.info(f"Environment config: obs_dim={env_config.obs_dim}, max_steps={env_config.max_steps}")
    logger.info(f"Training config: lr={train_config.learning_rate}, timesteps={train_config.total_timesteps}")

    # Validate resume checkpoint exists before proceeding
    if resume_from is not None:
        resume_path = Path(resume_from)
        if not resume_path.exists():
            raise FileNotFoundError(
                f"Resume checkpoint not found: {resume_from}\n"
                "Please provide a valid path to an existing checkpoint file."
            )

    # Create directories
    checkpoint_dir = Path(train_config.checkpoint_dir)
    log_dir = Path(train_config.log_dir)
    try:
        checkpoint_dir.mkdir(parents=True, exist_ok=True)
        log_dir.mkdir(parents=True, exist_ok=True)
        logger.info(f"Checkpoint directory: {checkpoint_dir.absolute()}")
        logger.info(f"Log directory: {log_dir.absolute()}")
    except OSError as e:
        raise TrainingError(
            f"Failed to create directories: {e}\n"
            f"Checkpoint dir: {checkpoint_dir}, Log dir: {log_dir}"
        ) from e

    # Create environment
    try:
        env = DummyVecEnv([lambda: make_env(env_config)])
    except Exception as e:
        raise TrainingError(
            f"Failed to create training environment: {e}\n"
            "Check that ROS is running and the environment configuration is valid."
        ) from e

    # Create or load model
    try:
        if resume_from is not None:
            logger.info(f"Resuming from checkpoint: {resume_from}")
            model = PPO.load(resume_from, env=env)
        else:
            # Policy kwargs for network architecture
            policy_kwargs = {
                "net_arch": {
                    "pi": train_config.policy_hidden,
                    "vf": train_config.value_hidden,
                }
            }

            model = PPO(
                "MlpPolicy",
                env,
                learning_rate=train_config.learning_rate,
                n_steps=train_config.n_steps,
                batch_size=train_config.batch_size,
                n_epochs=train_config.n_epochs,
                gamma=train_config.gamma,
                gae_lambda=train_config.gae_lambda,
                clip_range=train_config.clip_range,
                ent_coef=train_config.ent_coef,
                vf_coef=train_config.vf_coef,
                max_grad_norm=train_config.max_grad_norm,
                policy_kwargs=policy_kwargs,
                tensorboard_log=str(log_dir),
                verbose=1,
            )
    except Exception as e:
        raise TrainingError(
            f"Failed to create/load PPO model: {e}\n"
            "Check hyperparameters and checkpoint validity."
        ) from e

    # Callbacks
    checkpoint_callback = CheckpointCallback(
        save_freq=train_config.save_freq,
        save_path=str(checkpoint_dir),
        name_prefix="ppo_warehouser",
    )

    try:
        eval_env = DummyVecEnv([lambda: make_env(env_config)])
    except Exception as e:
        raise TrainingError(
            f"Failed to create evaluation environment: {e}\n"
            "Check that ROS is running and the environment configuration is valid."
        ) from e

    eval_callback = EvalCallback(
        eval_env,
        best_model_save_path=str(checkpoint_dir / "best"),
        log_path=str(log_dir / "eval"),
        eval_freq=train_config.eval_freq,
        n_eval_episodes=train_config.n_eval_episodes,
        deterministic=True,
    )

    # Train
    logger.info(f"Starting training for {train_config.total_timesteps} timesteps")
    try:
        model.learn(
            total_timesteps=train_config.total_timesteps,
            callback=[checkpoint_callback, eval_callback],
            progress_bar=True,
        )
    except KeyboardInterrupt:
        logger.warning("Training interrupted by user. Saving current model...")
    except Exception as e:
        raise TrainingError(
            f"Training failed: {e}\n"
            "Check environment stability and hyperparameters."
        ) from e

    # Save final model
    final_path = checkpoint_dir / "ppo_warehouser_final"
    try:
        model.save(str(final_path))
    except Exception as e:
        raise TrainingError(
            f"Failed to save final model to {final_path}: {e}\n"
            "Check disk space and write permissions."
        ) from e

    # Verify model was saved
    expected_file = final_path.with_suffix(".zip")
    if not expected_file.exists():
        raise TrainingError(
            f"Model save appeared to succeed but file not found: {expected_file}\n"
            "This may indicate a disk or filesystem issue."
        )

    logger.info(f"Training complete. Final model saved to {expected_file}")
    return model


def load_config(config_path: str) -> tuple[EnvConfig, TrainingConfig]:
    """Load configuration from JSON file with validation.

    Args:
        config_path: Path to the JSON configuration file.

    Returns:
        Tuple of (EnvConfig, TrainingConfig).

    Raises:
        ConfigurationError: If the config file is invalid or cannot be loaded.
    """
    path = Path(config_path)

    # Check file exists
    if not path.exists():
        raise ConfigurationError(
            f"Configuration file not found: {config_path}\n"
            "Please provide a valid path to a JSON configuration file."
        )

    # Check it's a file, not a directory
    if not path.is_file():
        raise ConfigurationError(
            f"Configuration path is not a file: {config_path}\n"
            "Please provide a path to a JSON file, not a directory."
        )

    # Read and parse JSON
    try:
        with open(path) as f:
            config_data = json.load(f)
    except json.JSONDecodeError as e:
        raise ConfigurationError(
            f"Invalid JSON in configuration file: {config_path}\n"
            f"JSON error at line {e.lineno}, column {e.colno}: {e.msg}\n"
            "Please check the file for syntax errors."
        ) from e
    except OSError as e:
        raise ConfigurationError(
            f"Could not read configuration file: {config_path}\n"
            f"Error: {e}"
        ) from e

    # Validate it's a dict
    if not isinstance(config_data, dict):
        raise ConfigurationError(
            f"Configuration file must contain a JSON object, got {type(config_data).__name__}\n"
            "Expected format: {\"env\": {...}, \"training\": {...}}"
        )

    # Parse and validate configs using Pydantic
    try:
        env_config = EnvConfig(**config_data.get("env", {}))
    except ValidationError as e:
        raise ConfigurationError(
            f"Invalid environment configuration in {config_path}:\n{e}\n"
            "Check that all values are of the correct type and within valid ranges."
        ) from e

    try:
        train_config = TrainingConfig(**config_data.get("training", {}))
    except ValidationError as e:
        raise ConfigurationError(
            f"Invalid training configuration in {config_path}:\n{e}\n"
            "Check that all values are of the correct type and within valid ranges."
        ) from e

    logger.info(f"Loaded configuration from {config_path}")
    return env_config, train_config


def main() -> None:
    """Main entry point for training.

    Parses command line arguments, loads configuration, and starts training.
    All errors are logged with informative messages before exiting.
    """
    parser = argparse.ArgumentParser(
        description="Train PPO agent for warehouser robot",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Train with default config
  %(prog)s --config config.json     # Train with custom config file
  %(prog)s --resume checkpoint.zip  # Resume from checkpoint
  %(prog)s --timesteps 500000       # Train for specific timesteps
        """,
    )
    parser.add_argument(
        "--config",
        type=str,
        help="Path to training config JSON file",
    )
    parser.add_argument(
        "--resume",
        type=str,
        help="Path to checkpoint to resume from",
    )
    parser.add_argument(
        "--timesteps",
        type=int,
        default=1_000_000,
        help="Total training timesteps (default: 1,000,000)",
    )
    args = parser.parse_args()

    # Load or create configs
    try:
        if args.config:
            env_config, train_config = load_config(args.config)
        else:
            logger.info("No config file provided, using defaults")
            env_config = EnvConfig()
            train_config = TrainingConfig()
    except ConfigurationError as e:
        _fatal_error(str(e))

    # Override timesteps if provided
    if args.timesteps:
        train_config.total_timesteps = args.timesteps

    # Train
    try:
        train(env_config, train_config, resume_from=args.resume)
    except (TrainingError, FileNotFoundError) as e:
        _fatal_error(f"Training failed: {e}", cause=e.__cause__)
    except KeyboardInterrupt:
        logger.info("Training interrupted by user")
        sys.exit(130)  # Standard exit code for SIGINT


if __name__ == "__main__":
    main()
