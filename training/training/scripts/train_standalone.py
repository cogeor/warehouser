"""Train a small PPO model using the standalone environment.

This script trains without requiring ROS2 dependencies. Used for
testing the training pipeline end-to-end.

Usage:
    python -m training.scripts.train_standalone --timesteps 10000
"""

import argparse
import logging
import sys
from pathlib import Path

import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import CheckpointCallback
from stable_baselines3.common.vec_env import DummyVecEnv

from training.envs.standalone_env import StandaloneEnv
from training.models.config import EnvConfig

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stderr)],
)
logger = logging.getLogger(__name__)


def make_env(config: EnvConfig) -> StandaloneEnv:
    """Create a standalone environment."""
    return StandaloneEnv(config)


def train(
    total_timesteps: int = 10000,
    checkpoint_dir: str = "checkpoints",
    save_freq: int = 2000,
) -> PPO:
    """Train a PPO model on the standalone environment.

    Args:
        total_timesteps: Number of timesteps to train.
        checkpoint_dir: Directory to save checkpoints.
        save_freq: Save checkpoint every N steps.

    Returns:
        Trained PPO model.
    """
    logger.info(f"Training for {total_timesteps} timesteps")

    # Create environment
    config = EnvConfig(obs_dim=5, action_dim=4, max_steps=200)
    env = DummyVecEnv([lambda: make_env(config)])

    # Create checkpoint directory
    checkpoint_path = Path(checkpoint_dir)
    checkpoint_path.mkdir(parents=True, exist_ok=True)

    # Checkpoint callback
    checkpoint_callback = CheckpointCallback(
        save_freq=save_freq,
        save_path=str(checkpoint_path),
        name_prefix="ppo_standalone",
    )

    # Create PPO model with small network
    model = PPO(
        "MlpPolicy",
        env,
        learning_rate=3e-4,
        n_steps=256,
        batch_size=64,
        n_epochs=10,
        gamma=0.99,
        gae_lambda=0.95,
        clip_range=0.2,
        ent_coef=0.01,
        verbose=1,
        policy_kwargs={
            "net_arch": [64, 64],  # Small network for quick training
        },
    )

    logger.info("Starting training...")
    model.learn(
        total_timesteps=total_timesteps,
        callback=checkpoint_callback,
    )

    # Save final model
    final_path = checkpoint_path / "ppo_standalone_final"
    model.save(str(final_path))
    logger.info(f"Saved final model to {final_path}")

    return model


def evaluate(model: PPO, num_episodes: int = 5) -> None:
    """Evaluate the trained model.

    Args:
        model: Trained PPO model.
        num_episodes: Number of episodes to run.
    """
    logger.info(f"Evaluating model for {num_episodes} episodes")

    config = EnvConfig(obs_dim=5, action_dim=4, max_steps=200)
    env = StandaloneEnv(config)

    rewards = []
    for ep in range(num_episodes):
        obs, _ = env.reset()
        total_reward = 0.0
        done = False

        while not done:
            action, _ = model.predict(obs, deterministic=True)
            obs, reward, terminated, truncated, info = env.step(action)
            total_reward += reward
            done = terminated or truncated

        rewards.append(total_reward)
        logger.info(f"Episode {ep + 1}: reward={total_reward:.2f}, steps={info['step']}")

    logger.info(f"Mean reward: {np.mean(rewards):.2f} (+/- {np.std(rewards):.2f})")


def main() -> None:
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Train standalone PPO model")
    parser.add_argument(
        "--timesteps",
        type=int,
        default=10000,
        help="Total training timesteps",
    )
    parser.add_argument(
        "--checkpoint-dir",
        type=str,
        default="checkpoints",
        help="Directory for checkpoints",
    )
    parser.add_argument(
        "--eval-episodes",
        type=int,
        default=5,
        help="Number of evaluation episodes",
    )

    args = parser.parse_args()

    # Train
    model = train(
        total_timesteps=args.timesteps,
        checkpoint_dir=args.checkpoint_dir,
    )

    # Evaluate
    evaluate(model, num_episodes=args.eval_episodes)


if __name__ == "__main__":
    main()
