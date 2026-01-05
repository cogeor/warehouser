"""PPO training script for warehouser robot."""

import argparse
import json
from pathlib import Path

import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import CheckpointCallback, EvalCallback
from stable_baselines3.common.vec_env import DummyVecEnv

from training.envs.ros_env import ROSGymEnv
from training.models.config import EnvConfig, TrainingConfig


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
    """
    # Create directories
    checkpoint_dir = Path(train_config.checkpoint_dir)
    log_dir = Path(train_config.log_dir)
    checkpoint_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    # Create environment
    env = DummyVecEnv([lambda: make_env(env_config)])

    # Create or load model
    if resume_from is not None:
        print(f"Resuming from {resume_from}")
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

    # Callbacks
    checkpoint_callback = CheckpointCallback(
        save_freq=train_config.save_freq,
        save_path=str(checkpoint_dir),
        name_prefix="ppo_warehouser",
    )

    eval_env = DummyVecEnv([lambda: make_env(env_config)])
    eval_callback = EvalCallback(
        eval_env,
        best_model_save_path=str(checkpoint_dir / "best"),
        log_path=str(log_dir / "eval"),
        eval_freq=train_config.eval_freq,
        n_eval_episodes=train_config.n_eval_episodes,
        deterministic=True,
    )

    # Train
    print(f"Starting training for {train_config.total_timesteps} timesteps")
    model.learn(
        total_timesteps=train_config.total_timesteps,
        callback=[checkpoint_callback, eval_callback],
        progress_bar=True,
    )

    # Save final model
    final_path = checkpoint_dir / "ppo_warehouser_final"
    model.save(str(final_path))
    print(f"Training complete. Final model saved to {final_path}")

    return model


def main() -> None:
    """Main entry point for training."""
    parser = argparse.ArgumentParser(description="Train PPO agent for warehouser")
    parser.add_argument(
        "--config", type=str, help="Path to training config JSON file"
    )
    parser.add_argument(
        "--resume", type=str, help="Path to checkpoint to resume from"
    )
    parser.add_argument(
        "--timesteps", type=int, default=1_000_000, help="Total training timesteps"
    )
    args = parser.parse_args()

    # Load or create configs
    if args.config:
        with open(args.config) as f:
            config_data = json.load(f)
        env_config = EnvConfig(**config_data.get("env", {}))
        train_config = TrainingConfig(**config_data.get("training", {}))
    else:
        env_config = EnvConfig()
        train_config = TrainingConfig()

    # Override timesteps if provided
    if args.timesteps:
        train_config.total_timesteps = args.timesteps

    # Train
    train(env_config, train_config, resume_from=args.resume)


if __name__ == "__main__":
    main()
