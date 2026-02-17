# Training and Deployment Guide

This guide covers the full workflow for training and deploying policy models in the warehouser simulation.

## Prerequisites

- Python 3.12+ with `uv` package manager
- Docker for ROS2 simulation
- CUDA (optional, for GPU training)

## Training Pipeline Overview

```
┌─────────────────┐     ┌──────────────────┐     ┌────────────────┐
│  Train Model    │ --> │  Export to ONNX  │ --> │  Deploy Model  │
│  (Python/SB3)   │     │  (with metadata) │     │  (C++/Docker)  │
└─────────────────┘     └──────────────────┘     └────────────────┘
```

## 1. Training

### Standalone Training (No ROS)

For rapid iteration without ROS dependencies:

```bash
cd training

# Install dependencies
uv sync

# Train for 10k timesteps (quick test)
python -m training.scripts.train_standalone --timesteps 10000

# Train for longer (production)
python -m training.scripts.train_standalone --timesteps 100000 --checkpoint-dir checkpoints/production
```

### ROS-based Training (Full Simulation)

For training with the full warehouse environment:

```bash
# Start the ROS simulation
docker run -p 9090:9090 warehouser-demo

# In another terminal
cd training
python -m training.scripts.train
```

## 2. ONNX Export

Export trained models to ONNX format for C++ inference:

```bash
# Basic export with versioning
python -m training.scripts.export_onnx checkpoints/model.zip --version 1.0.0

# With explicit dimensions
python -m training.scripts.export_onnx checkpoints/model.zip \
  --version 1.0.0 \
  --obs-dim 8 \
  --action-dim 4 \
  --output models/policy_v1.0.0.onnx
```

### Model Metadata

The export embeds metadata in the ONNX model:
- `model_version`: Semantic version (X.Y.Z)
- `obs_dim`: Observation dimension
- `action_dim`: Action dimension
- `export_timestamp`: ISO8601 timestamp

This metadata is validated at load time in C++ to catch mismatches early.

### VecNormalize Stats

If VecNormalize was used during training, the normalization stats are exported alongside:
- `model_v1.0.0.onnx` - The model
- `model_v1.0.0.vecnormalize.pkl` - Normalization stats

## 3. Deployment

### Option A: Copy to Docker Image

Build a new Docker image with the model:

```dockerfile
# In Dockerfile.demo
COPY models/policy_v1.0.0.onnx /ros_ws/install/warehouser_inference/share/warehouser_inference/models/
```

Rebuild:
```bash
docker build -f Dockerfile.demo -t warehouser-demo:v1.0.0 .
```

### Option B: Volume Mount

Mount the model at runtime:

```bash
docker run -p 9090:9090 \
  -v ./models:/ros_ws/models:ro \
  warehouser-demo
```

### Option C: Inference Node Configuration

Set the model path via ROS parameter:

```yaml
# In config/inference_params.yaml
inference_node:
  ros__parameters:
    model_path: "/path/to/policy_v1.0.0.onnx"
```

## Troubleshooting

### Model Load Fails

1. **"Model file not found"**: Check the file path exists
2. **"obs_dim mismatch"**: Model was trained with different observation space
3. **"action_dim mismatch"**: Model was trained with different action space
4. **"ONNX Runtime error"**: Check ONNX Runtime version compatibility

### Training Issues

1. **NaN rewards**: Learning rate too high, try 1e-4
2. **No improvement**: Check reward function, increase timesteps
3. **Crash on GPU**: Try CPU training first (`CUDA_VISIBLE_DEVICES=`)

## Version Compatibility

| Model Version | Obs Dim | Action Dim | Notes |
|--------------|---------|------------|-------|
| 1.0.x        | 8       | 4          | Initial version |
| (standalone) | 5       | 4          | Simplified obs |

## Quick Reference

```bash
# Train
python -m training.scripts.train_standalone --timesteps 50000

# Export
python -m training.scripts.export_onnx checkpoints/model.zip --version 1.0.0

# Deploy (Docker build)
docker build -f Dockerfile.demo -t warehouser-demo .

# Run simulation
docker run -p 9090:9090 warehouser-demo
```
