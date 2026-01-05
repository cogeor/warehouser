"""Export trained PPO model to ONNX format."""

import argparse
from pathlib import Path

import onnx
import torch
from stable_baselines3 import PPO


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
    """
    # Get the policy network
    policy = model.policy

    # Create a wrapper that only outputs the action (not value)
    class PolicyWrapper(torch.nn.Module):
        def __init__(self, policy: torch.nn.Module) -> None:
            super().__init__()
            self.policy = policy

        def forward(self, obs: torch.Tensor) -> torch.Tensor:
            # Get action from policy (deterministic)
            return self.policy.actor.get_action_dist_params(obs)[0]

    wrapper = PolicyWrapper(policy)
    wrapper.eval()

    # Create dummy input
    dummy_input = torch.randn(1, obs_dim, dtype=torch.float32)

    # Export to ONNX
    torch.onnx.export(
        wrapper,
        dummy_input,
        output_path,
        opset_version=opset_version,
        input_names=["observation"],
        output_names=["action"],
        dynamic_axes={
            "observation": {0: "batch_size"},
            "action": {0: "batch_size"},
        },
    )

    # Validate the exported model
    onnx_model = onnx.load(output_path)
    onnx.checker.check_model(onnx_model)

    print(f"ONNX model exported to {output_path}")


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
    """
    # Load the model
    model = PPO.load(checkpoint_path)

    # Export
    export_to_onnx(model, output_path, obs_dim)


def main() -> None:
    """Main entry point for ONNX export."""
    parser = argparse.ArgumentParser(description="Export PPO model to ONNX")
    parser.add_argument(
        "checkpoint", type=str, help="Path to SB3 checkpoint (.zip file)"
    )
    parser.add_argument(
        "--output", type=str, default=None, help="Output ONNX path"
    )
    parser.add_argument(
        "--obs-dim", type=int, default=8, help="Observation dimension"
    )
    args = parser.parse_args()

    # Determine output path
    if args.output is None:
        checkpoint_path = Path(args.checkpoint)
        output_path = checkpoint_path.with_suffix(".onnx")
    else:
        output_path = Path(args.output)

    # Export
    export_from_checkpoint(args.checkpoint, str(output_path), args.obs_dim)


if __name__ == "__main__":
    main()
