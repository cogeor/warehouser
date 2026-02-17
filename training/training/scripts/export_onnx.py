"""Export trained PPO model to ONNX format.

This script exports a trained Stable-Baselines3 PPO model to ONNX format
for inference in C++ (ros_inference). All errors are raised with informative
messages - no silent failures.

Model metadata is embedded in the ONNX file:
- model_version: Semantic version string
- obs_dim: Observation dimension
- action_dim: Action dimension
- export_timestamp: ISO8601 timestamp
"""

import argparse
import logging
import re
import shutil
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any, NoReturn

import onnx
import torch
from onnx import StringStringEntryProto
from stable_baselines3 import PPO

# Configure logging to be loud about errors
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(name)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stderr)],
)
logger = logging.getLogger(__name__)

# Semantic version regex pattern
VERSION_PATTERN = re.compile(r"^\d+\.\d+\.\d+$")


class ExportError(Exception):
    """Raised when ONNX export fails."""

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


def validate_version(version: str) -> bool:
    """Validate semantic version format.

    Args:
        version: Version string to validate.

    Returns:
        True if version matches X.Y.Z format.
    """
    return bool(VERSION_PATTERN.match(version))


def add_metadata_to_model(
    model_path: str,
    version: str,
    obs_dim: int,
    action_dim: int,
) -> None:
    """Add metadata to an existing ONNX model.

    Args:
        model_path: Path to the ONNX model file.
        version: Model version string.
        obs_dim: Observation dimension.
        action_dim: Action dimension.

    Raises:
        ExportError: If metadata cannot be added.
    """
    try:
        onnx_model = onnx.load(model_path)
    except Exception as e:
        raise ExportError(f"Failed to load ONNX model for metadata: {e}") from e

    # Generate ISO8601 timestamp
    timestamp = datetime.now(UTC).isoformat()

    # Add metadata properties
    metadata_entries = [
        ("model_version", version),
        ("obs_dim", str(obs_dim)),
        ("action_dim", str(action_dim)),
        ("export_timestamp", timestamp),
    ]

    for key, value in metadata_entries:
        onnx_model.metadata_props.append(StringStringEntryProto(key=key, value=value))

    # Save the model with metadata
    try:
        onnx.save(onnx_model, model_path)
    except Exception as e:
        raise ExportError(f"Failed to save ONNX model with metadata: {e}") from e

    logger.info(
        f"Added metadata: version={version}, obs_dim={obs_dim}, "
        f"action_dim={action_dim}, timestamp={timestamp}"
    )


def export_vecnormalize_stats(
    checkpoint_path: str,
    output_path: str,
) -> str | None:
    """Export VecNormalize statistics alongside the ONNX model.

    Args:
        checkpoint_path: Path to the SB3 checkpoint.
        output_path: Path to the ONNX output file.

    Returns:
        Path to the exported stats file, or None if not found.
    """
    checkpoint = Path(checkpoint_path)

    # Construct VecNormalize stats path (follows train.py convention)
    if checkpoint.suffix == ".zip":
        base_path = str(checkpoint.with_suffix(""))
    else:
        base_path = str(checkpoint)
    vecnorm_path = Path(base_path + "_vecnormalize.pkl")

    if not vecnorm_path.exists():
        logger.info("No VecNormalize stats file found, skipping stats export")
        return None

    # Copy stats file to output directory with matching name
    output_file = Path(output_path)
    output_stats_path = output_file.with_suffix(".vecnormalize.pkl")

    try:
        shutil.copy2(vecnorm_path, output_stats_path)
        logger.info(f"VecNormalize stats exported to: {output_stats_path}")
        return str(output_stats_path)
    except Exception as e:
        logger.warning(f"Failed to export VecNormalize stats: {e}")
        return None


def export_to_onnx(
    model: PPO,
    output_path: str,
    obs_dim: int = 8,
    action_dim: int = 4,
    version: str = "1.0.0",
    opset_version: int = 17,
) -> None:
    """Export PPO policy to ONNX format.

    Args:
        model: Trained PPO model.
        output_path: Path to save the ONNX file.
        obs_dim: Observation dimension.
        action_dim: Action dimension.
        version: Model version string (semantic versioning).
        opset_version: ONNX opset version.

    Raises:
        ExportError: If export or validation fails.
        ValueError: If obs_dim is invalid or version format is wrong.
    """
    # Validate inputs
    if obs_dim <= 0:
        raise ValueError(
            f"Observation dimension must be positive, got {obs_dim}\n"
            "Check that the model was trained with the correct observation space."
        )

    if action_dim <= 0:
        raise ValueError(
            f"Action dimension must be positive, got {action_dim}\n"
            "Check that the model was trained with the correct action space."
        )

    if not validate_version(version):
        raise ValueError(f"Invalid version format: '{version}'. Expected X.Y.Z (e.g., 1.0.0)")

    if opset_version < 9:
        raise ValueError(
            f"ONNX opset version must be >= 9 for dynamic axes support, got {opset_version}"
        )

    logger.info(f"Exporting model to ONNX format: {output_path}")
    logger.info(f"Version: {version}, obs_dim: {obs_dim}, action_dim: {action_dim}")
    logger.info(f"ONNX opset version: {opset_version}")

    # Get the policy network
    policy = model.policy
    if policy is None:
        raise ExportError(
            "Model has no policy network. The model may not have been trained properly."
        )

    # Create a wrapper that only outputs the action (not value)
    class PolicyWrapper(torch.nn.Module):
        def __init__(self, policy: torch.nn.Module) -> None:
            super().__init__()
            self.policy = policy

        def forward(self, obs: torch.Tensor) -> Any:
            # Get action from policy (deterministic)
            return self.policy.actor.get_action_dist_params(obs)[0]  # type: ignore

    wrapper = PolicyWrapper(policy)
    wrapper.eval()

    # Create dummy input
    dummy_input = torch.randn(1, obs_dim, dtype=torch.float32)

    # Export to ONNX
    try:
        torch.onnx.export(
            wrapper,
            (dummy_input,),
            output_path,
            opset_version=opset_version,
            input_names=["observation"],
            output_names=["action"],
            dynamic_axes={
                "observation": {0: "batch_size"},
                "action": {0: "batch_size"},
            },
        )
    except Exception as e:
        raise ExportError(
            f"Failed to export model to ONNX: {e}\n"
            "This may indicate an incompatibility between the model architecture and ONNX.\n"
            "Try updating torch or using a different opset version."
        ) from e

    # Verify output file was created
    output_file = Path(output_path)
    if not output_file.exists():
        raise ExportError(
            f"Export appeared to succeed but output file not found: {output_path}\n"
            "Check disk space and write permissions."
        )

    if output_file.stat().st_size == 0:
        raise ExportError(
            f"Export created empty file: {output_path}\n"
            "This indicates a torch.onnx.export failure. Check model compatibility."
        )

    # Add metadata to the model
    add_metadata_to_model(output_path, version, obs_dim, action_dim)

    # Validate the exported model
    try:
        onnx_model = onnx.load(output_path)
    except Exception as e:
        raise ExportError(
            f"Failed to load exported ONNX model for validation: {e}\n"
            "The export may have produced an invalid file."
        ) from e

    try:
        onnx.checker.check_model(onnx_model)
    except onnx.checker.ValidationError as e:
        raise ExportError(
            f"Exported ONNX model failed validation: {e}\n"
            "The model structure may be invalid. Check model architecture."
        ) from e
    except Exception as e:
        raise ExportError(
            f"ONNX validation failed unexpectedly: {e}\nThis may indicate an ONNX library issue."
        ) from e

    logger.info(f"ONNX model successfully exported and validated: {output_path}")
    logger.info(f"File size: {output_file.stat().st_size} bytes")


def export_from_checkpoint(
    checkpoint_path: str,
    output_path: str,
    obs_dim: int = 8,
    action_dim: int = 4,
    version: str = "1.0.0",
) -> None:
    """Export a checkpoint to ONNX.

    Args:
        checkpoint_path: Path to the SB3 checkpoint.
        output_path: Path to save the ONNX file.
        obs_dim: Observation dimension.
        action_dim: Action dimension.
        version: Model version string.

    Raises:
        FileNotFoundError: If checkpoint does not exist.
        ExportError: If loading or export fails.
    """
    # Validate checkpoint exists
    checkpoint = Path(checkpoint_path)
    if not checkpoint.exists():
        raise FileNotFoundError(
            f"Checkpoint file not found: {checkpoint_path}\n"
            "Please provide a valid path to a trained model checkpoint (.zip file)."
        )

    if not checkpoint.is_file():
        raise FileNotFoundError(
            f"Checkpoint path is not a file: {checkpoint_path}\n"
            "Please provide a path to a .zip file, not a directory."
        )

    logger.info(f"Loading checkpoint: {checkpoint_path}")

    # Load the model
    try:
        model = PPO.load(checkpoint_path)
    except Exception as e:
        raise ExportError(
            f"Failed to load checkpoint: {checkpoint_path}\n"
            f"Error: {e}\n"
            "Ensure this is a valid Stable-Baselines3 PPO checkpoint file."
        ) from e

    # Verify output directory exists (or can be created)
    output_file = Path(output_path)
    output_dir = output_file.parent
    if not output_dir.exists():
        try:
            output_dir.mkdir(parents=True, exist_ok=True)
            logger.info(f"Created output directory: {output_dir}")
        except OSError as e:
            raise ExportError(f"Cannot create output directory: {output_dir}\nError: {e}") from e

    # Export
    export_to_onnx(model, output_path, obs_dim, action_dim, version)

    # Export VecNormalize stats if available
    export_vecnormalize_stats(checkpoint_path, output_path)


def main() -> None:
    """Main entry point for ONNX export.

    Parses command line arguments and exports the model.
    All errors are logged with informative messages before exiting.
    """
    parser = argparse.ArgumentParser(
        description="Export trained PPO model to ONNX format for C++ inference",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s checkpoints/model.zip                       # Export with auto-named output
  %(prog)s checkpoints/model.zip --version 2.0.0       # Export with version
  %(prog)s checkpoints/model.zip --output model.onnx   # Export with custom output path
  %(prog)s checkpoints/model.zip --obs-dim 16          # Specify observation dimension

Output:
  - policy_v{version}.onnx: The ONNX model with embedded metadata
  - policy_v{version}.vecnormalize.pkl: VecNormalize stats (if available)
        """,
    )
    parser.add_argument(
        "checkpoint",
        type=str,
        help="Path to SB3 checkpoint (.zip file)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default=None,
        help="Output ONNX path (default: policy_v{version}.onnx in checkpoint directory)",
    )
    parser.add_argument(
        "--version",
        type=str,
        default="1.0.0",
        help="Model version string in X.Y.Z format (default: 1.0.0)",
    )
    parser.add_argument(
        "--obs-dim",
        type=int,
        default=8,
        help="Observation dimension (default: 8)",
    )
    parser.add_argument(
        "--action-dim",
        type=int,
        default=4,
        help="Action dimension (default: 4)",
    )
    args = parser.parse_args()

    # Validate version format
    if not validate_version(args.version):
        _fatal_error(
            f"Invalid version format: '{args.version}'\n"
            "Use --version with X.Y.Z format (e.g., 1.0.0, 2.1.3)"
        )

    # Validate obs_dim early
    if args.obs_dim <= 0:
        _fatal_error(
            f"Observation dimension must be positive, got {args.obs_dim}\n"
            "Use --obs-dim with a positive integer value."
        )

    # Validate action_dim early
    if args.action_dim <= 0:
        _fatal_error(
            f"Action dimension must be positive, got {args.action_dim}\n"
            "Use --action-dim with a positive integer value."
        )

    # Determine output path
    if args.output is None:
        checkpoint_path = Path(args.checkpoint)
        output_path = checkpoint_path.parent / f"policy_v{args.version}.onnx"
    else:
        output_path = Path(args.output)

    # Export
    try:
        export_from_checkpoint(
            args.checkpoint,
            str(output_path),
            args.obs_dim,
            args.action_dim,
            args.version,
        )
    except FileNotFoundError as e:
        _fatal_error(str(e))
    except ExportError as e:
        _fatal_error(str(e), cause=e.__cause__)
    except Exception as e:
        _fatal_error(f"Unexpected error during export: {e}", cause=e)

    logger.info("Export completed successfully")


if __name__ == "__main__":
    main()
