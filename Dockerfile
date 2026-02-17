# Dockerfile for building and testing Warehouser ROS2 packages
FROM ros:jazzy-ros-base

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-13 \
    wget \
    python3-colcon-common-extensions \
    python3-rosdep \
    ros-jazzy-std-msgs \
    ros-jazzy-std-srvs \
    ros-jazzy-geometry-msgs \
    ros-jazzy-nav-msgs \
    ros-jazzy-sensor-msgs \
    ros-jazzy-ament-cmake-gtest \
    ros-jazzy-ament-lint-auto \
    ros-jazzy-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*

# Install ONNX Runtime for policy inference
ARG ONNXRUNTIME_VERSION=1.16.3
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz \
    && tar -xzf onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz \
    && mv onnxruntime-linux-x64-${ONNXRUNTIME_VERSION} /opt/onnxruntime \
    && rm onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz

# Set ONNX Runtime paths for CMake
ENV ONNXRUNTIME_ROOT=/opt/onnxruntime
ENV LD_LIBRARY_PATH=/opt/onnxruntime/lib:${LD_LIBRARY_PATH}

# Set GCC 13 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# Create workspace
WORKDIR /ros_ws

# Copy source files
COPY ros_ws/src /ros_ws/src

# Source ROS2 and build with ONNX Runtime
RUN . /opt/ros/jazzy/setup.sh && \
    colcon build --symlink-install --cmake-args -DBUILD_TESTING=ON -DONNXRUNTIME_ROOT=/opt/onnxruntime

# Run tests (exclude integration tests and linter tests - focus on unit tests)
CMD ["/bin/bash", "-c", ". /opt/ros/jazzy/setup.sh && . /ros_ws/install/setup.sh && colcon test --ctest-args -E '(integration|copyright|cpplint|flake8|pep257|uncrustify|cppcheck|lint_cmake|xmllint)' && colcon test-result --verbose"]
