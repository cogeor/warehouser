# TASK: Train Small Model for Robot Control

Created: 2026-02-14
Status: Completed

## Goal

Train a minimal PPO model to control the robot. The goal is to verify the training pipeline works end-to-end, not to achieve perfect performance.

## Completed Work

### 1. Created Standalone Environment
Created `training/training/envs/standalone_env.py` - a simple warehouse navigation environment that doesn't require ROS2. The robot receives observations about its position relative to a goal and outputs velocity commands.

**Observation space (5D):** `[goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]`
**Action space (4D):** `[linear_vel, angular_vel, pick, place]` in [-1, 1]

### 2. Created Training Script
Created `training/training/scripts/train_standalone.py` for training without ROS dependencies.

### 3. Trained Model
Trained PPO for 5000 timesteps (~5 seconds):
- Network architecture: [64, 64] MLP
- Learning rate: 3e-4
- Batch size: 64
- Evaluation reward: -103 (not optimized, but learns to move)

### 4. Exported Model
Created `training/training/scripts/export_simple.py` for TorchScript export.
Model exported to: `checkpoints/standalone/policy.pt` (55KB)

### 5. Verified Actions
Model produces valid actions in [-1, 1] range for all test cases:
- Goal ahead: moves forward
- Goal to left: turns left
- Goal behind: turns around
- Close to goal: slows down

## Output Files

```
training/checkpoints/standalone/
├── ppo_standalone_final.zip    # SB3 checkpoint
├── ppo_standalone_2000_steps.zip  # Intermediate checkpoint
├── ppo_standalone_4000_steps.zip  # Intermediate checkpoint
└── policy.pt                   # TorchScript model (55KB)
```

## Success Criteria

- [x] Training runs without errors
- [x] Model checkpoint is saved
- [x] Model export succeeds (TorchScript format)
- [x] Model produces valid actions when given observations

## Next Steps

To use this model with ROS2:
1. Convert TorchScript to ONNX (requires onnxscript package)
2. Load in C++ inference node using LibTorch or ONNX Runtime
3. Connect to simulation via ROS2 services
