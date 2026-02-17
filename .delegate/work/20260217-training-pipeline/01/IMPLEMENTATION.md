# Implementation: Add ONNX Runtime to Docker

## Task 1: Update Dockerfile.demo with ONNX Runtime

### Files Modified
- `Dockerfile.demo`: Added ONNX Runtime installation
- `Dockerfile`: Added ONNX Runtime installation (for consistency)

### Changes Made

Added to both Dockerfiles:
```dockerfile
# Install ONNX Runtime for policy inference
ARG ONNXRUNTIME_VERSION=1.16.3
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz \
    && tar -xzf onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz \
    && mv onnxruntime-linux-x64-${ONNXRUNTIME_VERSION} /opt/onnxruntime \
    && rm onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz

# Set ONNX Runtime paths for CMake
ENV ONNXRUNTIME_ROOT=/opt/onnxruntime
ENV LD_LIBRARY_PATH=/opt/onnxruntime/lib:${LD_LIBRARY_PATH}
```

Also modified colcon build command to pass ONNXRUNTIME_ROOT:
```dockerfile
RUN . /opt/ros/jazzy/setup.sh && \
    colcon build --symlink-install --cmake-args -DONNXRUNTIME_ROOT=/opt/onnxruntime
```

## Verification

Build output confirms ONNX Runtime is found:
```
-- Found ONNX Runtime: /opt/onnxruntime/lib/libonnxruntime.so
```

The inference_node now has real ONNX Runtime support instead of the stub implementation.
