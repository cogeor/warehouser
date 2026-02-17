# Plan: Add ONNX Runtime to Docker

## Objective
Install ONNX Runtime in Docker so the inference node can run real trained models instead of the stub implementation.

## Tasks

### Task 1: Update Dockerfile.demo with ONNX Runtime
- Download and install onnxruntime from GitHub releases (amd64 Linux)
- Set ONNXRUNTIME_ROOT environment variable for CMake to find it
- Ensure colcon build picks up the library

### Task 2: Create a test model for verification
- Create a minimal ONNX model using Python that can be used for testing
- Model should have correct input/output shapes (obs_dim=5, action_dim=4)

### Task 3: Test the Docker build
- Build the Docker image with ONNX Runtime
- Verify the build log shows "Found ONNX Runtime"
- Test model loading works

## Files to Modify
- `Dockerfile.demo` - Add ONNX Runtime installation
- `ros_ws/src/warehouser_inference/models/` - Add test model (optional)

## Verification
- `docker build -f Dockerfile.demo` succeeds
- Build log shows "Found ONNX Runtime: /opt/onnxruntime/lib/libonnxruntime.so"
