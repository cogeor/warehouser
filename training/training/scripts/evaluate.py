"""Evaluate trained PPO model with frozen normalization.

This script evaluates a trained model by loading VecNormalize statistics
and running deterministic evaluation episodes.
"""

import argparse
import logging
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, NoReturn

import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize

from training.envs.ros_env import ROSGymEnv
from training.models.config import EnvConfig

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(name)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stderr)],
)
logger = logging.getLogger(__name__)


class EvaluationError(Exception):
    """Raised when evaluation fails."""

    pass


@dataclass
class EvaluationResult:
    """Results from evaluation episodes."""

    mean_reward: float
    std_reward: float
    success_rate: float
    mean_episode_length: float
    std_episode_length: float
    num_episodes: int

    def __str__(self) -> str:
        """Format results as a human-readable string."""
        return (
            f"Evaluation Results ({self.num_episodes} episodes):\n"
            f"  Mean Reward: {self.mean_reward:.2f} +/- {self.std_reward:.2f}\n"
            f"  Success Rate: {self.success_rate * 100:.1f}%\n"
            f"  Mean Episode Length: {self.mean_episode_length:.1f} "
            f"+/- {self.std_episode_length:.1f}"
        )


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


def get_vecnormalize_path(model_path: Path) -> Path:
    """Get the VecNormalize stats path for a model.

    Args:
        model_path: Path to the model file (.zip).

    Returns:
        Path to the VecNormalize stats file (.pkl).
    """
    # Remove .zip extension if present, then add _vecnormalize.pkl
    stem = model_path.stem
    if stem.endswith(".zip"):
        stem = stem[:-4]
    return model_path.parent / f"{stem}_vecnormalize.pkl"


def load_model_and_env(
    model_path: str,
    env_config: EnvConfig,
) -> tuple[PPO, VecNormalize]:
    """Load trained model and environment with frozen normalization.

    Args:
        model_path: Path to the trained model checkpoint.
        env_config: Environment configuration.

    Returns:
        Tuple of (model, normalized_env).

    Raises:
        FileNotFoundError: If model or VecNormalize stats not found.
        EvaluationError: If loading fails.
    """
    model_file = Path(model_path)

    # Validate model exists
    if not model_file.exists():
        raise FileNotFoundError(
            f"Model file not found: {model_path}\n"
            "Please provide a valid path to a trained model checkpoint (.zip file)."
        )

    if not model_file.is_file():
        raise FileNotFoundError(
            f"Model path is not a file: {model_path}\n"
            "Please provide a path to a .zip file, not a directory."
        )

    # Find VecNormalize stats
    vecnormalize_path = get_vecnormalize_path(model_file)

    logger.info(f"Loading model from: {model_path}")

    # Create base environment
    try:
        base_env = DummyVecEnv([lambda: make_env(env_config)])
    except Exception as e:
        raise EvaluationError(
            f"Failed to create evaluation environment: {e}\n"
            "Check that ROS is running and the environment configuration is valid."
        ) from e

    # Load or create VecNormalize wrapper
    if vecnormalize_path.exists():
        logger.info(f"Loading VecNormalize stats from: {vecnormalize_path}")
        try:
            env = VecNormalize.load(str(vecnormalize_path), base_env)
        except Exception as e:
            raise EvaluationError(
                f"Failed to load VecNormalize stats from {vecnormalize_path}: {e}\n"
                "The stats file may be corrupted or incompatible."
            ) from e
    else:
        logger.warning(
            f"VecNormalize stats not found at {vecnormalize_path}, using un-normalized environment"
        )
        # Wrap in VecNormalize but disable normalization
        env = VecNormalize(base_env, norm_obs=False, norm_reward=False)

    # Freeze running stats for evaluation (do not update mean/var)
    env.training = False

    # Load model
    try:
        model = PPO.load(model_path, env=env)
    except Exception as e:
        raise EvaluationError(
            f"Failed to load model from {model_path}: {e}\n"
            "Ensure this is a valid Stable-Baselines3 PPO checkpoint file."
        ) from e

    logger.info("Model and environment loaded successfully")
    logger.info(f"VecNormalize training mode: {env.training} (frozen)")

    return model, env


def evaluate(
    model: PPO,
    env: VecNormalize,
    num_episodes: int,
) -> EvaluationResult:
    """Run evaluation episodes with deterministic actions.

    Args:
        model: Trained PPO model.
        env: Evaluation environment with frozen normalization.
        num_episodes: Number of episodes to run.

    Returns:
        Evaluation results including mean reward, success rate, episode lengths.

    Raises:
        EvaluationError: If evaluation fails.
    """
    logger.info(f"Running {num_episodes} evaluation episodes...")

    episode_rewards: list[float] = []
    episode_lengths: list[int] = []
    successes: list[bool] = []

    try:
        for episode in range(num_episodes):
            obs_result = env.reset()
            # Handle different return types from VecEnv.reset()
            obs: Any
            if isinstance(obs_result, tuple):
                obs = obs_result[0]  # (obs, info) tuple
            else:
                obs = obs_result
            done = False
            total_reward = 0.0
            step_count = 0

            while not done:
                # Use deterministic=True for reproducible evaluation
                action, _ = model.predict(obs, deterministic=True)
                obs, reward, done_arr, info_arr = env.step(action)

                # Handle vectorized environment output
                done = done_arr[0] if hasattr(done_arr, "__getitem__") else bool(done_arr)
                reward_val = reward[0] if hasattr(reward, "__getitem__") else float(reward)
                info = info_arr[0] if hasattr(info_arr, "__getitem__") else info_arr

                total_reward += reward_val
                step_count += 1

            episode_rewards.append(total_reward)
            episode_lengths.append(step_count)

            # Check for success in info dict (if available)
            # Success is typically indicated by reaching the goal without collision
            success = info.get("success", False) if isinstance(info, dict) else False
            successes.append(success)

            if (episode + 1) % max(1, num_episodes // 10) == 0:
                logger.info(
                    f"Episode {episode + 1}/{num_episodes}: "
                    f"reward={total_reward:.2f}, length={step_count}"
                )

    except Exception as e:
        raise EvaluationError(
            f"Evaluation failed during episode execution: {e}\n"
            "Check environment stability and ROS connectivity."
        ) from e

    # Calculate statistics
    rewards_array = np.array(episode_rewards, dtype=np.float32)
    lengths_array = np.array(episode_lengths, dtype=np.float32)

    return EvaluationResult(
        mean_reward=float(np.mean(rewards_array)),
        std_reward=float(np.std(rewards_array)),
        success_rate=float(np.mean(successes)) if successes else 0.0,
        mean_episode_length=float(np.mean(lengths_array)),
        std_episode_length=float(np.std(lengths_array)),
        num_episodes=num_episodes,
    )


def main() -> None:
    """Main entry point for evaluation.

    Parses command line arguments, loads model, and runs evaluation.
    """
    parser = argparse.ArgumentParser(
        description="Evaluate trained PPO model with frozen normalization",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --model checkpoints/model.zip                  # Evaluate with default 10 episodes
  %(prog)s --model checkpoints/model.zip --episodes 100   # Evaluate with 100 episodes
        """,
    )
    parser.add_argument(
        "--model",
        type=str,
        required=True,
        help="Path to trained model checkpoint (.zip file)",
    )
    parser.add_argument(
        "--episodes",
        type=int,
        default=10,
        help="Number of evaluation episodes (default: 10)",
    )
    parser.add_argument(
        "--obs-dim",
        type=int,
        default=8,
        help="Observation dimension (default: 8)",
    )
    args = parser.parse_args()

    # Validate arguments
    if args.episodes <= 0:
        _fatal_error(
            f"Number of episodes must be positive, got {args.episodes}\n"
            "Use --episodes with a positive integer value."
        )

    if args.obs_dim <= 0:
        _fatal_error(
            f"Observation dimension must be positive, got {args.obs_dim}\n"
            "Use --obs-dim with a positive integer value."
        )

    # Create environment config
    env_config = EnvConfig(obs_dim=args.obs_dim)

    # Load model and environment
    try:
        model, env = load_model_and_env(args.model, env_config)
    except FileNotFoundError as e:
        _fatal_error(str(e))
    except EvaluationError as e:
        _fatal_error(str(e), cause=e.__cause__)

    # Run evaluation
    try:
        result = evaluate(model, env, args.episodes)
    except EvaluationError as e:
        _fatal_error(str(e), cause=e.__cause__)
    finally:
        env.close()

    # Print results
    logger.info("\n" + str(result))

    # Exit with success
    logger.info("Evaluation completed successfully")


if __name__ == "__main__":
    main()
