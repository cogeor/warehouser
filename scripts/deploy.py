#!/usr/bin/env python3
"""One-command deployment: train, export, and build Docker image.

Usage:
    # Quick deploy (5k timesteps, for testing)
    python scripts/deploy.py --quick

    # Full training (50k timesteps)
    python scripts/deploy.py --timesteps 50000

    # Skip training, just export and build
    python scripts/deploy.py --skip-train --checkpoint training/checkpoints/standalone/ppo_standalone_final.zip

    # Build Docker image after training
    python scripts/deploy.py --build-docker
"""

import argparse
import logging
import shutil
import subprocess
import sys
from pathlib import Path

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger(__name__)

# Project root (parent of scripts/)
PROJECT_ROOT = Path(__file__).parent.parent.resolve()
TRAINING_DIR = PROJECT_ROOT / "training"
MODELS_DIR = PROJECT_ROOT / "models"
CHECKPOINTS_DIR = TRAINING_DIR / "checkpoints" / "standalone"


def run_command(cmd: list[str], cwd: Path | None = None) -> bool:
    """Run a command and return success status."""
    logger.info(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode == 0


def train_model(timesteps: int) -> Path | None:
    """Train a PPO model using standalone environment.

    Args:
        timesteps: Number of training timesteps.

    Returns:
        Path to trained checkpoint, or None on failure.
    """
    logger.info(f"Training model for {timesteps} timesteps...")

    cmd = [
        sys.executable,
        "-m",
        "training.scripts.train_standalone",
        "--timesteps",
        str(timesteps),
        "--checkpoint-dir",
        str(CHECKPOINTS_DIR),
        "--eval-episodes",
        "3",
    ]

    if not run_command(cmd, cwd=TRAINING_DIR):
        logger.error("Training failed")
        return None

    checkpoint = CHECKPOINTS_DIR / "ppo_standalone_final.zip"
    if not checkpoint.exists():
        logger.error(f"Checkpoint not found: {checkpoint}")
        return None

    logger.info(f"Training complete: {checkpoint}")
    return checkpoint


def export_model(checkpoint: Path, version: str) -> Path | None:
    """Export checkpoint to ONNX format.

    Args:
        checkpoint: Path to SB3 checkpoint.
        version: Semantic version for the model.

    Returns:
        Path to ONNX model, or None on failure.
    """
    logger.info(f"Exporting model to ONNX (version {version})...")

    # Create models directory
    MODELS_DIR.mkdir(parents=True, exist_ok=True)

    output_path = MODELS_DIR / f"policy_v{version}.onnx"

    cmd = [
        sys.executable,
        "-m",
        "training.scripts.export_onnx",
        str(checkpoint),
        "--version",
        version,
        "--obs-dim",
        "5",  # Standalone env uses 5D observations
        "--action-dim",
        "4",
        "--output",
        str(output_path),
    ]

    if not run_command(cmd, cwd=TRAINING_DIR):
        logger.error("Export failed")
        return None

    if not output_path.exists():
        logger.error(f"ONNX model not found: {output_path}")
        return None

    # Also copy as "latest" for Docker to pick up
    latest_path = MODELS_DIR / "policy_latest.onnx"
    shutil.copy2(output_path, latest_path)
    logger.info(f"Copied to {latest_path}")

    logger.info(f"Export complete: {output_path}")
    return output_path


def build_docker() -> bool:
    """Build the Docker demo image."""
    logger.info("Building Docker image...")

    cmd = [
        "docker",
        "build",
        "-f",
        "Dockerfile.demo",
        "-t",
        "warehouser-demo",
        ".",
    ]

    if not run_command(cmd, cwd=PROJECT_ROOT):
        logger.error("Docker build failed")
        return False

    logger.info("Docker build complete: warehouser-demo")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(
        description="One-command deployment: train, export, build",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --quick                    # Quick test (5k steps)
  %(prog)s --timesteps 100000         # Full training
  %(prog)s --skip-train --build-docker  # Just rebuild Docker with existing model
        """,
    )
    parser.add_argument(
        "--timesteps",
        type=int,
        default=10000,
        help="Training timesteps (default: 10000)",
    )
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Quick mode: 5000 timesteps",
    )
    parser.add_argument(
        "--version",
        type=str,
        default="1.0.0",
        help="Model version (default: 1.0.0)",
    )
    parser.add_argument(
        "--skip-train",
        action="store_true",
        help="Skip training, use existing checkpoint",
    )
    parser.add_argument(
        "--checkpoint",
        type=str,
        help="Path to existing checkpoint (for --skip-train)",
    )
    parser.add_argument(
        "--build-docker",
        action="store_true",
        help="Build Docker image after export",
    )
    parser.add_argument(
        "--skip-export",
        action="store_true",
        help="Skip export (use existing ONNX model)",
    )

    args = parser.parse_args()

    if args.quick:
        args.timesteps = 5000

    checkpoint: Path | None = None

    # Step 1: Train (unless skipped)
    if not args.skip_train:
        checkpoint = train_model(args.timesteps)
        if checkpoint is None:
            sys.exit(1)
    else:
        if args.checkpoint:
            checkpoint = Path(args.checkpoint)
        else:
            checkpoint = CHECKPOINTS_DIR / "ppo_standalone_final.zip"

        if not checkpoint.exists():
            logger.error(f"Checkpoint not found: {checkpoint}")
            sys.exit(1)
        logger.info(f"Using existing checkpoint: {checkpoint}")

    # Step 2: Export to ONNX (unless skipped)
    if not args.skip_export:
        onnx_path = export_model(checkpoint, args.version)
        if onnx_path is None:
            sys.exit(1)
    else:
        onnx_path = MODELS_DIR / f"policy_v{args.version}.onnx"
        if not onnx_path.exists():
            logger.error(f"ONNX model not found: {onnx_path}")
            sys.exit(1)
        logger.info(f"Using existing ONNX model: {onnx_path}")

    # Step 3: Build Docker (if requested)
    if args.build_docker:
        if not build_docker():
            sys.exit(1)

    # Summary
    logger.info("=" * 50)
    logger.info("Deployment complete!")
    logger.info(f"  Model: {MODELS_DIR / 'policy_latest.onnx'}")
    if args.build_docker:
        logger.info("  Docker: warehouser-demo")
        logger.info("")
        logger.info("Run with:")
        logger.info("  docker run -p 9090:9090 warehouser-demo")
    else:
        logger.info("")
        logger.info("Next steps:")
        logger.info("  1. Build Docker: python scripts/deploy.py --skip-train --build-docker")
        logger.info("  2. Run: docker run -p 9090:9090 warehouser-demo")


if __name__ == "__main__":
    main()
