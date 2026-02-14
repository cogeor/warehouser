"""Export trained PPO model to ONNX format.

This script exports a trained Stable-Baselines3 PPO model to ONNX format
for inference in C++ (ros_inference). All errors are raised with informative
messages - no silent failures.
"""

import argparse
import logging
import sys
from pathlib import Path
from typing import Any, NoReturn

import onnx
import torch
from stable_baselines3 import PPO

# Configure logging to be loud about errors
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(name)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stderr)],
)
logger = logging.getLogger(__name__)


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


def export_to_onnx(
    model: PPO,
    output_path: str,
    obs_dim: int = 8,
    opset_version: int = 17,
) -> None:
    """Export PPO policy to ONNX format.

    Args:
        model: Trained PPO model.
        output_path: Path to save the ONNX file.
        obs_dim: Observation dimension.
        opset_version: ONNX opset version.

    Raises:
        ExportError: If export or validation fails.
        ValueError: If obs_dim is invalid.
    """
    # Validate inputs
    if obs_dim <= 0:
        raise ValueError(
            f"Observation dimension must be positive, got {obs_dim}\n"
            "Check that the model was trained with the correct observation space."
        )

    if opset_version < 9:
        raise ValueError(
            f"ONNX opset version must be >= 9 for dynamic axes support, got {opset_version}"
        )

    logger.info(f"Exporting model to ONNX format: {output_path}")
    logger.info(f"Observation dimension: {obs_dim}, ONNX opset version: {opset_version}")

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
) -> None:
    """Export a checkpoint to ONNX.

    Args:
        checkpoint_path: Path to the SB3 checkpoint.
        output_path: Path to save the ONNX file.
        obs_dim: Observation dimension.

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
    export_to_onnx(model, output_path, obs_dim)


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
  %(prog)s checkpoints/model.zip                    # Export with auto-named output
  %(prog)s checkpoints/model.zip --output model.onnx  # Export with custom output path
  %(prog)s checkpoints/model.zip --obs-dim 16         # Specify observation dimension
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
        help="Output ONNX path (default: checkpoint path with .onnx extension)",
    )
    parser.add_argument(
        "--obs-dim",
        type=int,
        default=8,
        help="Observation dimension (default: 8)",
    )
    args = parser.parse_args()

    # Validate obs_dim early
    if args.obs_dim <= 0:
        _fatal_error(
            f"Observation dimension must be positive, got {args.obs_dim}\n"
            "Use --obs-dim with a positive integer value."
        )

    # Determine output path
    if args.output is None:
        checkpoint_path = Path(args.checkpoint)
        output_path = checkpoint_path.with_suffix(".onnx")
    else:
        output_path = Path(args.output)

    # Export
    try:
        export_from_checkpoint(args.checkpoint, str(output_path), args.obs_dim)
    except FileNotFoundError as e:
        _fatal_error(str(e))
    except ExportError as e:
        _fatal_error(str(e), cause=e.__cause__)
    except Exception as e:
        _fatal_error(f"Unexpected error during export: {e}", cause=e)

    logger.info("Export completed successfully")


if __name__ == "__main__":
    main()
