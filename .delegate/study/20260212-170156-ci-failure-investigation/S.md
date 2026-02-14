# Search: ROS2 CI/CD Best Practices

Created: 2026-02-12T17:05:00Z

## Query

"ROS2 CI/CD best practices GitHub Actions industrial_ci rosdep 2026"

## Findings

### 1. Official ROS Tooling Working Group Actions

The ROS tooling working group maintains two critical GitHub Actions that form the foundation of ROS2 CI pipelines:

**a) setup-ros (v0.7+)**
- Initializes the ROS environment
- Sets up specified ROS distributions
- Required before any ROS build steps

**b) action-ros-ci (v0.4+)**
- Performs `rosdep install` to fetch dependencies
- Runs `colcon build` to compile packages
- Executes `colcon test` for validation
- Requires explicit ROS distribution targeting via `target-ros1-distro` or `target-ros2-distro`
- Can build necessary ROS 2 dependencies from source

**Key Features:**
- `rosdep-check: true` flag to check for missing dependencies
- `skip-rosdep-install: true` for Docker images with pre-installed dependencies
- Automatic dependency resolution through rosdep

**Sources:**
- [ROS 2 CI Action - GitHub Marketplace](https://github.com/marketplace/actions/ros-2-ci-action)
- [GitHub - ros-tooling/action-ros-ci](https://github.com/ros-tooling/action-ros-ci)
- [ROS 2 CI with GitHub Actions | Ubuntu](https://ubuntu.com/blog/ros-2-ci-with-github-actions)

### 2. industrial_ci Alternative

industrial_ci is a mature, widely-used alternative CI framework for ROS projects:

**Features:**
- Simplifies CI tasks for repositories containing ROS packages
- Checks if packages build and install without issues
- Compatibility testing across different ROS distributions
- Supports unit and integration tests
- Built-in linting checks
- More opinionated and comprehensive than action-ros-ci

**Sources:**
- [GitHub - ros-industrial/industrial_ci](https://github.com/ros-industrial/industrial_ci)
- [Continuous Integration for Robotics — agROBOfood](https://agrobofood.github.io/agrobofood-case-studies/case_studies/CICD-for-robotics.html)

### 3. Best Practices Identified

**a) System Consistency**
- CI/CD enforces consistent system setup across contributors
- Ensures code changes are compatible with specified system requirements
- Critical for ROS2 projects with complex dependency graphs

**b) Dependency Management**
- Use `rosdep install` to automatically resolve dependencies from package.xml
- rosdep keys map to apt packages for system dependencies
- Implement `rosdep-check` in CI to catch missing dependencies early
- For Docker-based workflows, pre-install dependencies in images and skip rosdep install

**c) Linting Integration**
- Implement explicit tests for linters (ament_lint, clang-format, etc.)
- Contributors are notified of code style conventions
- Linters should be installable via rosdep for consistency

**d) Scheduled Builds**
- Run periodic builds to reveal flaky tests
- Detect problems from upstream dependency updates
- Recommended: daily or weekly scheduled runs

**e) Workflow Structure**
```
.github/
└── workflows/
    ├── ci.yaml          # Main build and test
    ├── scheduled.yaml   # Periodic dependency checks
    └── lint.yaml        # Code style enforcement
```

**Sources:**
- [Setup CI/CD for a ROS2 project using Github | Medium](https://medium.com/@shantanuparab99/setup-ci-cd-for-a-ros2-project-using-github-121d62bae348)
- [CI for ROS 2 using GitHub Actions - ROS Discourse](https://discourse.openrobotics.org/t/ci-for-ros-2-using-github-actions/19446)

### 4. Multi-Language Project Strategies

For projects like Warehouser (ROS2 C++ + Python + TypeScript):

**Option 1: Unified Workflow**
- Single workflow file with multiple jobs
- Each job targets specific language stack
- Parallel execution for faster CI
- Shared artifact caching between jobs

**Option 2: Separate Workflows**
- `ros2-ci.yaml` for C++ ROS packages
- `python-ci.yaml` for training pipeline
- `typescript-ci.yaml` for web frontend
- Each workflow triggers on relevant path changes

**Recommended Approach for Warehouser:**
- Use action-ros-ci for ROS2 workspace (ros_ws/)
- Separate job for Python training tests (training/)
- Separate job for TypeScript frontend (web_frontend/)
- All three run in parallel on pull requests

## Cloned

None (no reference repositories cloned this cycle)

## Proposal: Apply to Warehouser CI Failure

Based on research, here's how to address the CI failure investigation:

### 1. Implement action-ros-ci for ROS2 Packages

Create `.github/workflows/ros2-ci.yaml`:

```yaml
name: ROS2 CI

on:
  push:
    branches: [main]
    paths:
      - 'ros_ws/**'
  pull_request:
    paths:
      - 'ros_ws/**'

jobs:
  ros2-build-test:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Setup ROS 2
        uses: ros-tooling/setup-ros@v0.7
        with:
          required-ros-distributions: jazzy

      - name: Build and Test ROS 2 Packages
        uses: ros-tooling/action-ros-ci@v0.4
        with:
          target-ros2-distro: jazzy
          package-name: |
            warehouser_msgs
            warehouser_simulation
            warehouser_observations
            warehouser_rl_bridge
          vcs-repo-file-url: ''
          colcon-defaults: |
            {
              "build": {
                "symlink-install": true
              }
            }
```

### 2. Add Python Training CI

Create `.github/workflows/python-ci.yaml`:

```yaml
name: Python Training CI

on:
  push:
    branches: [main]
    paths:
      - 'training/**'
  pull_request:
    paths:
      - 'training/**'

jobs:
  test:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: training
    steps:
      - uses: actions/checkout@v4

      - name: Install uv
        uses: astral-sh/setup-uv@v5

      - name: Set up Python
        run: uv python install 3.12

      - name: Install dependencies
        run: uv pip install -e ".[dev]"

      - name: Run pytest
        run: uv run pytest tests/ -v --ignore=tests/integration/
```

### 3. Add TypeScript Frontend CI

Create `.github/workflows/typescript-ci.yaml`:

```yaml
name: TypeScript Frontend CI

on:
  push:
    branches: [main]
    paths:
      - 'web_frontend/**'
  pull_request:
    paths:
      - 'web_frontend/**'

jobs:
  test:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: web_frontend
    steps:
      - uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: '20'
          cache: 'npm'
          cache-dependency-path: web_frontend/package-lock.json

      - name: Install dependencies
        run: npm ci

      - name: Run tests
        run: npm test
```

### 4. Fix package.xml Dependencies

Ensure all ROS2 packages have complete `package.xml` with proper dependencies:

**Critical rosdep keys to verify:**
- `rclcpp` (C++ ROS2 client library)
- `std_msgs`, `geometry_msgs`, etc. (standard message types)
- `rosidl_default_generators` (for custom messages)
- Build tools: `ament_cmake`, `ament_cmake_gtest`

### 5. Root Cause Hypothesis

Based on best practices, the CI failure likely stems from:

1. **Missing rosdep configuration** - Dependencies not properly declared in package.xml
2. **No GitHub Actions workflow** - CI not set up at all
3. **Docker configuration issue** - If using Docker, dependencies not pre-installed
4. **Build order problems** - Packages not building in correct dependency order

### Next Steps

1. Check if `.github/workflows/` exists and contains valid workflows
2. Validate all `package.xml` files have complete dependency lists
3. Test rosdep locally: `rosdep install --from-paths ros_ws/src --ignore-src -r -y`
4. Implement the three-workflow strategy above for complete coverage

## References

- [Setup CI/CD for a ROS2 project using Github | Medium](https://medium.com/@shantanuparab99/setup-ci-cd-for-a-ros2-project-using-github-121d62bae348)
- [ROS 2 CI with GitHub Actions | Ubuntu](https://ubuntu.com/blog/ros-2-ci-with-github-actions)
- [CI for ROS 2 using GitHub Actions - ROS Discourse](https://discourse.openrobotics.org/t/ci-for-ros-2-using-github-actions/19446)
- [ROS 2 CI Action - GitHub Marketplace](https://github.com/marketplace/actions/ros-2-ci-action)
- [GitHub - ros-tooling/action-ros-ci](https://github.com/ros-tooling/action-ros-ci)
- [GitHub - ros-industrial/industrial_ci](https://github.com/ros-industrial/industrial_ci)
- [GitHub - ros2/ci: ROS 2 CI Infrastructure](https://github.com/ros2/ci)
- [Continuous Integration for Robotics — agROBOfood](https://agrobofood.github.io/agrobofood-case-studies/case_studies/CICD-for-robotics.html)
- [ROS 2 CI with GitHub Actions | Canonical](https://canonical.com/blog/ros-2-ci-with-github-actions)
