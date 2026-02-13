# Dockerfile for building and testing Warehouser ROS2 packages
FROM ros:jazzy-ros-base

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-13 \
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

# Set GCC 13 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# Create workspace
WORKDIR /ros_ws

# Copy source files
COPY ros_ws/src /ros_ws/src

# Source ROS2 and build
RUN . /opt/ros/jazzy/setup.sh && \
    colcon build --symlink-install --cmake-args -DBUILD_TESTING=ON

# Run tests (exclude integration tests and linter tests - focus on unit tests)
CMD ["/bin/bash", "-c", ". /opt/ros/jazzy/setup.sh && . /ros_ws/install/setup.sh && colcon test --ctest-args -E '(integration|copyright|cpplint|flake8|pep257|uncrustify|cppcheck|lint_cmake|xmllint)' && colcon test-result --verbose"]
