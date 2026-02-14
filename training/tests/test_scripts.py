"""Tests for training scripts (train.py and export_onnx.py).

Tests are designed with mocking to work without torch/stable-baselines3.
"""

import json
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# Pre-mock heavy dependencies before importing training modules
torch_mock = MagicMock()
torch_mock.onnx = MagicMock()
torch_mock.nn = MagicMock()
torch_mock.randn = MagicMock()
sys.modules["torch"] = torch_mock
sys.modules["torch.nn"] = torch_mock.nn
sys.modules["torch.onnx"] = torch_mock.onnx

onnx_mock = MagicMock()
onnx_mock.checker = MagicMock()
sys.modules["onnx"] = onnx_mock
sys.modules["onnx.checker"] = onnx_mock.checker

sb3_mock = MagicMock()
sb3_mock.common = MagicMock()
sb3_mock.common.callbacks = MagicMock()
sb3_mock.common.vec_env = MagicMock()
sys.modules["stable_baselines3"] = sb3_mock
sys.modules["stable_baselines3.common"] = sb3_mock.common
sys.modules["stable_baselines3.common.callbacks"] = sb3_mock.common.callbacks
sys.modules["stable_baselines3.common.vec_env"] = sb3_mock.common.vec_env

gym_mock = MagicMock()
sys.modules["gymnasium"] = gym_mock

rclpy_mock = MagicMock()
sys.modules["rclpy"] = rclpy_mock


def test_train_script_syntax() -> None:
    """Test that train.py has valid Python syntax."""
    train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
    assert train_py.exists(), f"train.py not found at {train_py}"

    with open(train_py) as f:
        code = f.read()

    # Should compile without syntax errors
    compile(code, str(train_py), "exec")


def test_export_onnx_script_syntax() -> None:
    """Test that export_onnx.py has valid Python syntax."""
    export_py = Path(__file__).parent.parent / "training" / "scripts" / "export_onnx.py"
    assert export_py.exists(), f"export_onnx.py not found at {export_py}"

    with open(export_py) as f:
        code = f.read()

    # Should compile without syntax errors
    compile(code, str(export_py), "exec")


class TestTrainScriptStructure:
    """Test the structure and logic of train.py."""

    def test_train_script_has_main_function(self) -> None:
        """Verify train.py has a main() entry point."""
        train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
        with open(train_py) as f:
            code = f.read()

        assert "def main()" in code
        assert 'if __name__ == "__main__"' in code

    def test_train_script_imports_config_models(self) -> None:
        """Verify train.py imports config models."""
        train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
        with open(train_py) as f:
            code = f.read()

        assert "EnvConfig" in code
        assert "TrainingConfig" in code

    def test_train_script_parses_arguments(self) -> None:
        """Verify train.py uses argparse."""
        train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
        with open(train_py) as f:
            code = f.read()

        assert "argparse" in code
        assert "ArgumentParser" in code

    def test_train_script_handles_config_file(self) -> None:
        """Verify train.py can load config from file."""
        train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
        with open(train_py) as f:
            code = f.read()

        assert "json.load" in code
        assert "--config" in code

    def test_train_script_resumes_from_checkpoint(self) -> None:
        """Verify train.py supports resuming from checkpoint."""
        train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
        with open(train_py) as f:
            code = f.read()

        assert "--resume" in code
        assert "resume_from" in code

    def test_train_script_saves_model(self) -> None:
        """Verify train.py saves final model."""
        train_py = Path(__file__).parent.parent / "training" / "scripts" / "train.py"
        with open(train_py) as f:
            code = f.read()

        assert "model.save" in code
        assert "final" in code.lower()


class TestExportOnnxScriptStructure:
    """Test the structure and logic of export_onnx.py."""

    def test_export_script_has_main_function(self) -> None:
        """Verify export_onnx.py has a main() entry point."""
        export_py = Path(__file__).parent.parent / "training" / "scripts" / "export_onnx.py"
        with open(export_py) as f:
            code = f.read()

        assert "def main()" in code
        assert 'if __name__ == "__main__"' in code

    def test_export_script_parses_checkpoint_argument(self) -> None:
        """Verify export_onnx.py accepts checkpoint path."""
        export_py = Path(__file__).parent.parent / "training" / "scripts" / "export_onnx.py"
        with open(export_py) as f:
            code = f.read()

        assert "argparse" in code or "ArgumentParser" in code
        assert "checkpoint" in code

    def test_export_script_validates_onnx_model(self) -> None:
        """Verify export_onnx.py validates exported ONNX."""
        export_py = Path(__file__).parent.parent / "training" / "scripts" / "export_onnx.py"
        with open(export_py) as f:
            code = f.read()

        assert "onnx.checker.check_model" in code or "checker.check_model" in code

    def test_export_script_infers_output_path(self) -> None:
        """Verify export_onnx.py infers .onnx output from checkpoint."""
        export_py = Path(__file__).parent.parent / "training" / "scripts" / "export_onnx.py"
        with open(export_py) as f:
            code = f.read()

        assert "output" in code or "output_path" in code

    def test_export_script_exports_to_onnx(self) -> None:
        """Verify export_onnx.py exports model to ONNX."""
        export_py = Path(__file__).parent.parent / "training" / "scripts" / "export_onnx.py"
        with open(export_py) as f:
            code = f.read()

        assert "onnx" in code
        assert "torch.onnx.export" in code or "export" in code


class TestTrainArgumentHandling:
    """Test argument parsing in train.py (with mocked train function)."""

    def test_train_main_with_no_args(self) -> None:
        """Test train.py main() with no arguments."""
        from training.scripts.train import main as train_main

        with patch("sys.argv", ["train.py"]):
            with patch("training.scripts.train.train") as mock_train:
                train_main()
                mock_train.assert_called_once()

    def test_train_main_with_config_file(self, tmp_path: Path) -> None:
        """Test train.py loading config from JSON file."""
        from training.scripts.train import main as train_main

        config = {
            "env": {"max_steps": 1000},
            "training": {"learning_rate": 1e-3},
        }
        config_file = tmp_path / "config.json"
        config_file.write_text(json.dumps(config))

        with patch("sys.argv", ["train.py", "--config", str(config_file)]):
            with patch("training.scripts.train.train") as mock_train:
                train_main()
                mock_train.assert_called_once()
                # Verify custom config was loaded
                call_args = mock_train.call_args
                env_config = call_args[0][0]
                assert env_config.max_steps == 1000

    def test_train_main_with_timesteps_override(self) -> None:
        """Test train.py overrides total timesteps."""
        from training.scripts.train import main as train_main

        with patch("sys.argv", ["train.py", "--timesteps", "500000"]):
            with patch("training.scripts.train.train") as mock_train:
                train_main()
                call_args = mock_train.call_args
                train_config = call_args[0][1]
                assert train_config.total_timesteps == 500000

    def test_train_main_with_resume_argument(self) -> None:
        """Test train.py resumes from checkpoint."""
        from training.scripts.train import main as train_main

        checkpoint = "/path/to/checkpoint.zip"
        with patch("sys.argv", ["train.py", "--resume", checkpoint]):
            with patch("training.scripts.train.train") as mock_train:
                train_main()
                call_args = mock_train.call_args
                assert call_args[1].get("resume_from") == checkpoint

    def test_train_main_with_invalid_json(self, tmp_path: Path) -> None:
        """Test train.py with invalid JSON config file raises SystemExit."""
        from training.scripts.train import main as train_main

        bad_config = tmp_path / "bad.json"
        bad_config.write_text("{invalid json")

        with patch("sys.argv", ["train.py", "--config", str(bad_config)]):
            # The new error handling calls sys.exit(1) with a clear error message
            with pytest.raises(SystemExit) as exc_info:
                train_main()
            assert exc_info.value.code == 1


class TestExportArgumentHandling:
    """Test argument parsing in export_onnx.py (with mocked export function)."""

    def test_export_main_with_checkpoint_only(self, tmp_path: Path) -> None:
        """Test export_onnx.py with checkpoint argument only."""
        from training.scripts.export_onnx import main as export_main

        checkpoint = tmp_path / "model.zip"
        checkpoint.write_text("fake")

        with patch("sys.argv", ["export.py", str(checkpoint)]):
            with patch("training.scripts.export_onnx.export_from_checkpoint") as mock_export:
                export_main()
                mock_export.assert_called_once()
                call_args = mock_export.call_args
                output_path = call_args[0][1]
                assert output_path.endswith(".onnx")

    def test_export_main_with_custom_output(self, tmp_path: Path) -> None:
        """Test export_onnx.py with custom output path."""
        from training.scripts.export_onnx import main as export_main

        checkpoint = tmp_path / "model.zip"
        output = tmp_path / "custom.onnx"
        checkpoint.write_text("fake")

        with patch("sys.argv", ["export.py", str(checkpoint), "--output", str(output)]):
            with patch("training.scripts.export_onnx.export_from_checkpoint") as mock_export:
                export_main()
                call_args = mock_export.call_args
                assert str(output) in call_args[0]

    def test_export_main_with_obs_dim(self, tmp_path: Path) -> None:
        """Test export_onnx.py respects observation dimension."""
        from training.scripts.export_onnx import main as export_main

        checkpoint = tmp_path / "model.zip"
        checkpoint.write_text("fake")

        with patch("sys.argv", ["export.py", str(checkpoint), "--obs-dim", "16"]):
            with patch("training.scripts.export_onnx.export_from_checkpoint") as mock_export:
                export_main()
                call_args = mock_export.call_args
                assert call_args[0][2] == 16


class TestConfigModels:
    """Test that config models validate inputs correctly."""

    def test_env_config_validation(self) -> None:
        """Test EnvConfig enforces constraints."""
        from training.models.config import EnvConfig

        # Valid config should work
        config = EnvConfig(obs_dim=16, action_dim=4, max_steps=1000)
        assert config.obs_dim == 16
        assert config.action_dim == 4
        assert config.max_steps == 1000

    def test_training_config_defaults(self) -> None:
        """Test TrainingConfig provides sensible defaults."""
        from training.models.config import TrainingConfig

        config = TrainingConfig()
        assert config.learning_rate > 0
        assert config.n_steps > 0
        assert config.total_timesteps > 0

    def test_training_config_custom_values(self) -> None:
        """Test TrainingConfig accepts custom values."""
        from training.models.config import TrainingConfig

        config = TrainingConfig(
            learning_rate=1e-4,
            total_timesteps=500_000,
        )
        assert config.learning_rate == 1e-4
        assert config.total_timesteps == 500_000


class TestE2ETraining:
    """End-to-end tests for training pipeline."""

    def test_train_function_creates_directories(self, tmp_path: Path) -> None:
        """Test that train() creates checkpoint and log directories."""
        from training.models.config import EnvConfig, TrainingConfig
        from training.scripts.train import train

        env_config = EnvConfig()
        checkpoint_dir = tmp_path / "checkpoints"
        log_dir = tmp_path / "logs"
        train_config = TrainingConfig(
            total_timesteps=10,
            checkpoint_dir=str(checkpoint_dir),
            log_dir=str(log_dir),
        )

        # Create the expected model file so verification passes
        def mock_save(path: str) -> None:
            Path(path).with_suffix(".zip").touch()

        with patch("training.scripts.train.DummyVecEnv"):
            with patch("training.scripts.train.PPO") as mock_ppo:
                mock_model = MagicMock()
                mock_model.save = mock_save
                mock_ppo.return_value = mock_model

                train(env_config, train_config)

                # Verify directories were created
                assert checkpoint_dir.exists()
                assert log_dir.exists()

    def test_train_function_saves_model(self, tmp_path: Path) -> None:
        """Test that train() saves the final model."""
        from training.models.config import EnvConfig, TrainingConfig
        from training.scripts.train import train

        env_config = EnvConfig()
        checkpoint_dir = tmp_path / "checkpoints"
        train_config = TrainingConfig(
            total_timesteps=10,
            checkpoint_dir=str(checkpoint_dir),
            log_dir=str(tmp_path / "logs"),
        )

        # Track if save was called and create the expected file
        save_called = False

        def mock_save(path: str) -> None:
            nonlocal save_called
            save_called = True
            Path(path).with_suffix(".zip").touch()

        with patch("training.scripts.train.DummyVecEnv"):
            with patch("training.scripts.train.PPO") as mock_ppo:
                mock_model = MagicMock()
                mock_model.save = mock_save
                mock_ppo.return_value = mock_model

                train(env_config, train_config)

                # Verify model.save was called
                assert save_called

    def test_train_resumes_from_checkpoint(self, tmp_path: Path) -> None:
        """Test that train() can resume from a checkpoint."""
        from training.models.config import EnvConfig, TrainingConfig
        from training.scripts.train import train

        env_config = EnvConfig()
        checkpoint_dir = tmp_path / "checkpoints"
        train_config = TrainingConfig(
            total_timesteps=10,
            checkpoint_dir=str(checkpoint_dir),
            log_dir=str(tmp_path / "logs"),
        )

        # Create a fake checkpoint file
        checkpoint_file = tmp_path / "checkpoint.zip"
        checkpoint_file.touch()

        def mock_save(path: str) -> None:
            Path(path).with_suffix(".zip").touch()

        with patch("training.scripts.train.DummyVecEnv"):
            with patch("training.scripts.train.PPO") as mock_ppo:
                mock_model = MagicMock()
                mock_model.save = mock_save
                mock_ppo.load.return_value = mock_model

                train(env_config, train_config, resume_from=str(checkpoint_file))

                # Verify PPO.load was called with the checkpoint path
                mock_ppo.load.assert_called_once()

    def test_train_raises_on_missing_checkpoint(self, tmp_path: Path) -> None:
        """Test that train() raises FileNotFoundError for missing checkpoint."""
        from training.models.config import EnvConfig, TrainingConfig
        from training.scripts.train import train

        env_config = EnvConfig()
        train_config = TrainingConfig(
            total_timesteps=10,
            checkpoint_dir=str(tmp_path / "checkpoints"),
            log_dir=str(tmp_path / "logs"),
        )

        with pytest.raises(FileNotFoundError) as exc_info:
            train(env_config, train_config, resume_from="/nonexistent/checkpoint.zip")

        assert "not found" in str(exc_info.value).lower()
