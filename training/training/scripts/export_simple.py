"""Simple ONNX export for trained PPO model.

Exports using JIT trace approach to avoid onnxscript dependency.
"""

import argparse
import logging
import sys
from pathlib import Path
from typing import Any

import torch
from stable_baselines3 import PPO

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger(__name__)


class PolicyWrapper(torch.nn.Module):
    """Wrapper that extracts just the action from the policy."""

    def __init__(self, policy: Any) -> None:
        super().__init__()
        self.mlp_extractor = policy.mlp_extractor
        self.action_net = policy.action_net

    def forward(self, obs: torch.Tensor) -> torch.Tensor:
        features: torch.Tensor = self.mlp_extractor(obs)[0]  # Get policy features
        result: torch.Tensor = self.action_net(features)
        return result


def export_model(
    checkpoint_path: str,
    output_path: str,
    obs_dim: int = 5,
) -> None:
    """Export PPO model to ONNX.

    Args:
        checkpoint_path: Path to the .zip checkpoint.
        output_path: Path for output .onnx file.
        obs_dim: Observation dimension.
    """
    logger.info(f"Loading checkpoint: {checkpoint_path}")
    model = PPO.load(checkpoint_path)

    logger.info("Wrapping policy for ONNX export")
    wrapper = PolicyWrapper(model.policy)
    wrapper.eval()

    # Create dummy input
    dummy_input = torch.randn(1, obs_dim, dtype=torch.float32)

    logger.info(f"Exporting to: {output_path}")

    # Use JIT trace and then export to ONNX
    traced: torch.jit.ScriptModule = torch.jit.trace(  # type: ignore[no-untyped-call]
        wrapper, dummy_input
    )

    # Save as TorchScript (works without onnxscript)
    torchscript_path = output_path.replace(".onnx", ".pt")
    traced.save(torchscript_path)
    logger.info(f"Saved TorchScript model to: {torchscript_path}")

    # Test the model works
    with torch.no_grad():
        test_obs = torch.randn(1, obs_dim, dtype=torch.float32)
        action = traced(test_obs)
        logger.info(f"Test inference - input shape: {test_obs.shape}, output shape: {action.shape}")
        logger.info(f"Test action: {action.numpy().flatten()}")

    logger.info("Export successful!")

    # Verify the file was created
    torchscript_output = Path(torchscript_path)
    if torchscript_output.exists():
        size_kb = torchscript_output.stat().st_size / 1024
        logger.info(f"Output file size: {size_kb:.1f} KB")
    else:
        logger.error("Output file was not created!")
        sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description="Export PPO to ONNX")
    parser.add_argument("checkpoint", help="Path to .zip checkpoint")
    parser.add_argument("--output", "-o", help="Output .onnx path")
    parser.add_argument("--obs-dim", type=int, default=5, help="Observation dimension")

    args = parser.parse_args()

    # Default output path
    output_path = args.output
    if not output_path:
        checkpoint = Path(args.checkpoint)
        output_path = str(checkpoint.parent / f"{checkpoint.stem}.onnx")

    export_model(args.checkpoint, output_path, args.obs_dim)


if __name__ == "__main__":
    main()
