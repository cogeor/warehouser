# Warehouser

A ROS2-based warehouse robot simulation with reinforcement learning training pipeline.

## Overview

Warehouser is a modular simulation environment for training autonomous warehouse robots using PPO (Proximal Policy Optimization). The robot learns to navigate, pick up objects, and deliver them to designated zones.

## Architecture

```
warehouser/
├── ros_ws/src/
│   ├── warehouser_msgs/        # ROS2 message/service definitions
│   ├── warehouser_simulation/  # Core simulation (entities, world manager)
│   ├── warehouser_observations/# Observation builder, lidar simulator
│   └── warehouser_rl_bridge/   # RL step/reset services, reward calculator
├── training/                   # Python PPO training pipeline
│   ├── envs/                   # Gymnasium environment wrapper
│   ├── models/                 # Pydantic config models
│   ├── scripts/                # Training and ONNX export scripts
│   └── tests/                  # Unit and integration tests
└── run.md                      # Detailed usage documentation
```

## Requirements

- **ROS2**: Jazzy or Humble
- **C++**: GCC 13+ or MSVC 2022 (C++23)
- **Python**: 3.11+
- **Docker**: For containerized builds (optional)

## Quick Start

### Build ROS2 Packages

```bash
cd ros_ws
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install
source install/setup.bash
```

### Setup Python Environment

```bash
cd training
uv venv && source .venv/bin/activate
uv pip install -e ".[dev]"
```

### Run Tests

```bash
# C++ tests (with Docker)
docker build -t warehouser-ros:test .
docker run --rm warehouser-ros:test

# Python tests
cd training && pytest tests/ -v --ignore=tests/integration/
```

### Train a Policy

```bash
# Start ROS2 nodes first, then:
cd training
python -m training.scripts.train --timesteps 100000
```

### Export to ONNX

```bash
python -m training.scripts.export_onnx checkpoints/model.zip -o policy.onnx
```

## Documentation

See [run.md](run.md) for detailed instructions on building, running, and configuring the system.

## License

This project is licensed under the GNU General Public License v3.0 - see [LICENSE](LICENSE) for details.
