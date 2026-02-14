# Loop 02: Create Modern ROS2 CI Workflow

## Objective

Create a new GitHub Actions workflow that uses `ros-tooling/action-ros-ci` for automatic dependency management via rosdep, replacing manual package installation.

## Context

The existing CI workflow in `.github/workflows/ci.yml` manually installs ROS2 dependencies using apt-get. This approach:
- Requires explicit listing of all dependencies
- Can fall out of sync with package.xml declarations
- Does not leverage rosdep for automatic dependency resolution

## Tasks

### Task 1: Create ros2-ci.yaml workflow

**File:** `.github/workflows/ros2-ci.yaml`

**Requirements:**
- Use `ros-tooling/action-ros-ci@v0.3` for automatic rosdep dependency resolution
- Run on `ubuntu-24.04`
- Use ROS2 Jazzy distribution
- Trigger on pushes and PRs modifying `ros_ws/**` paths
- Build all packages in `ros_ws/src`
- Run unit tests (excluding integration and lint tests)
- Use colcon defaults compatible with existing project

**Implementation Details:**
- `action-ros-ci` automatically:
  - Sets up ROS2 environment
  - Runs rosdep to install dependencies from package.xml
  - Builds packages with colcon
  - Runs tests
- Configure `package-name` to build all warehouser packages
- Use `colcon-defaults` for custom build/test arguments

## Verification

1. YAML syntax is valid
2. Workflow references correct paths
3. All 9 ROS2 packages are included
4. Trigger paths are correctly configured
