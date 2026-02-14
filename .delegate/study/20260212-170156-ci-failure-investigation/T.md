# Template Analysis: ROS2 CI/CD Workflows

Created: 2026-02-12T17:15:00Z

## Source

Analyzed multiple ROS2 CI/CD template repositories and reference implementations:

1. **ros-tooling/action-ros-ci** - Official ROS 2 CI action (v0.4+)
2. **ros-tooling/action-ros-ci-template** - Official template repository
3. **ros-industrial/industrial_ci** - Mature ROS CI framework
4. **ros-controls/ros2_control_ci** - Reusable workflow with caching
5. **ROS 2 Official Documentation** - rosdep and dependency management

## Pattern 1: action-ros-ci (Recommended for Modern ROS2)

### Template: Basic Docker-based Workflow

```yaml
name: ROS2 CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build_and_test:
    runs-on: ubuntu-latest
    container:
      image: rostooling/setup-ros-docker:ubuntu-noble-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Build and test ROS 2 packages
        uses: ros-tooling/action-ros-ci@v0.4
        with:
          package-name: |
            warehouser_msgs
            warehouser_simulation
            warehouser_observations
            warehouser_rl_bridge
          target-ros2-distro: jazzy
          colcon-defaults: |
            {
              "build": {
                "symlink-install": true
              }
            }
```

**Key Features:**
- Uses pre-built Docker image with ROS dependencies
- Automatically runs `rosdep install` to fetch dependencies
- Runs `colcon build` and `colcon test` in sequence
- Fast startup time with cached Docker image

### Template: Non-Docker Approach (More Flexible)

```yaml
name: ROS2 CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build_and_test:
    runs-on: ubuntu-24.04
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup ROS 2
        uses: ros-tooling/setup-ros@v0.7
        with:
          required-ros-distributions: jazzy

      - name: Build and test ROS 2 packages
        uses: ros-tooling/action-ros-ci@v0.4
        with:
          package-name: |
            warehouser_msgs
            warehouser_simulation
            warehouser_observations
            warehouser_rl_bridge
          target-ros2-distro: jazzy
```

**Key Features:**
- Runs directly on Ubuntu runner (no Docker overhead)
- Uses setup-ros action to install ROS 2 Jazzy
- More flexible for custom dependencies
- Better for projects mixing ROS and non-ROS code

### Template: Multi-Distribution Matrix

```yaml
name: ROS2 CI Matrix

on: [push, pull_request]

jobs:
  build_and_test:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        ros_distro: [humble, jazzy, rolling]
    container:
      image: rostooling/setup-ros-docker:ubuntu-${{ matrix.ros_distro == 'humble' && 'jammy' || 'noble' }}-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ros-tooling/action-ros-ci@v0.4
        with:
          package-name: warehouser_msgs warehouser_simulation
          target-ros2-distro: ${{ matrix.ros_distro }}
```

**Key Features:**
- Tests across multiple ROS distributions
- Uses matrix strategy for parallel builds
- Conditional Docker image selection
- `fail-fast: false` continues testing other distros on failure

## Pattern 2: industrial_ci (Alternative Approach)

### Template: industrial_ci Workflow

```yaml
name: Industrial CI

on: [push, pull_request]

jobs:
  industrial_ci:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        env:
          - {ROS_DISTRO: jazzy, ROS_REPO: main}
          - {ROS_DISTRO: jazzy, ROS_REPO: testing}
    steps:
      - uses: actions/checkout@v4
      - uses: ros-industrial/industrial_ci@master
        env: ${{ matrix.env }}
```

**Key Features:**
- Extremely concise (3 lines for full CI)
- Opinionated defaults (includes linting)
- Tests against both main and testing repos
- Automatic colcon selection for ROS2
- Built-in compatibility testing

**When to Use:**
- Pure ROS projects (C++ only)
- Want comprehensive testing with minimal config
- Need ROS1/ROS2 compatibility

## Pattern 3: Linting Workflows

### Template: C++ Linting Matrix

```yaml
name: Linting

on: [push, pull_request]

jobs:
  ament_lint_cpp:
    name: ament_${{ matrix.linter }}
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        linter: [cppcheck, cpplint, uncrustify]
    steps:
      - uses: actions/checkout@v4
      - uses: ros-tooling/setup-ros@v0.7
        with:
          required-ros-distributions: jazzy
      - uses: ros-tooling/action-ros-lint@v0.1
        with:
          linter: ${{ matrix.linter }}
          package-name: |
            warehouser_simulation
            warehouser_observations
            warehouser_rl_bridge

  ament_copyright:
    name: ament_copyright
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ros-tooling/setup-ros@v0.7
      - uses: ros-tooling/action-ros-lint@v0.1
        with:
          linter: copyright
          package-name: warehouser_msgs warehouser_simulation
```

**Key Features:**
- Separate job for copyright checks
- Matrix for C++ linters (parallel execution)
- Uses official action-ros-lint action
- Enforces ament coding standards

## Pattern 4: Advanced Caching (ros2_control_ci)

### Template: Reusable Workflow with ccache

This is a more advanced pattern from ros-controls/ros2_control_ci showing professional-grade CI:

**Key Concepts:**
1. **Reusable workflow** - Other repos call this via `workflow_call`
2. **Multi-layer caching:**
   - ccache directory for compiled objects
   - Workspace base directory for dependencies
3. **Conditional Docker images:**
   - Different images for main vs testing repos
4. **Automated issue creation:**
   - Creates GitHub issues on scheduled build failures
5. **Compiler selection:**
   - Supports both gcc and clang builds

**Cache Key Strategy:**
```yaml
cache-key: ${{ inputs.ros_distro }}-${{ inputs.upstream_workspace }}-${{ inputs.ros_repo }}
```

**Benefits:**
- Significantly faster rebuild times
- Professional workflow reuse across repositories
- Automatic failure tracking

## Pattern 5: Multi-Language Project Structure

For projects like Warehouser (ROS2 + Python + TypeScript):

### Template: Path-Based Workflow Triggers

```yaml
# .github/workflows/ros2-ci.yaml
name: ROS2 CI
on:
  push:
    paths:
      - 'ros_ws/**'
      - '.github/workflows/ros2-ci.yaml'
  pull_request:
    paths:
      - 'ros_ws/**'

# .github/workflows/python-ci.yaml
name: Python Training CI
on:
  push:
    paths:
      - 'training/**'
      - '.github/workflows/python-ci.yaml'
  pull_request:
    paths:
      - 'training/**'

# .github/workflows/typescript-ci.yaml
name: TypeScript Frontend CI
on:
  push:
    paths:
      - 'web_frontend/**'
      - '.github/workflows/typescript-ci.yaml'
  pull_request:
    paths:
      - 'web_frontend/**'
```

**Key Features:**
- Independent workflows for each language stack
- Path-based triggering (only runs when relevant files change)
- Parallel execution across all three stacks
- Each workflow uses language-specific best practices

### Template: Python Training CI (uv-based)

```yaml
name: Python Training CI

on:
  push:
    branches: [main]
    paths: ['training/**']
  pull_request:
    paths: ['training/**']

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
        run: uv sync --dev

      - name: Run tests
        run: uv run pytest tests/ -v --ignore=tests/integration/

      - name: Type checking
        run: uv run mypy training/
```

**Key Features:**
- Uses modern uv package manager
- Skips integration tests (no ROS in this workflow)
- Includes mypy type checking
- Working directory set to `training/`

### Template: TypeScript Frontend CI

```yaml
name: TypeScript Frontend CI

on:
  push:
    branches: [main]
    paths: ['web_frontend/**']
  pull_request:
    paths: ['web_frontend/**']

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

      - name: Type check
        run: npm run type-check

      - name: Lint
        run: npm run lint

      - name: Run tests
        run: npm test

      - name: Build
        run: npm run build
```

**Key Features:**
- Node 20 LTS with npm caching
- Separate steps for type checking, linting, testing, building
- Uses npm ci for reproducible installs
- Working directory set to `web_frontend/`

## Pattern 6: Package Dependency Management

### Critical rosdep Configuration

Every ROS2 package needs a complete `package.xml` with proper dependencies:

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>warehouser_simulation</name>
  <version>0.1.0</version>
  <description>Warehouse robot simulation core</description>
  <maintainer email="you@example.com">Your Name</maintainer>
  <license>MIT</license>

  <!-- Build tool dependencies -->
  <buildtool_depend>ament_cmake</buildtool_depend>

  <!-- Build dependencies (needed to compile) -->
  <build_depend>rclcpp</build_depend>
  <build_depend>warehouser_msgs</build_depend>
  <build_depend>geometry_msgs</build_depend>
  <build_depend>nav_msgs</build_depend>

  <!-- Export dependencies (needed by downstream packages) -->
  <build_export_depend>rclcpp</build_export_depend>
  <build_export_depend>geometry_msgs</build_export_depend>

  <!-- Runtime dependencies (needed to run) -->
  <exec_depend>rclcpp</exec_depend>
  <exec_depend>warehouser_msgs</exec_depend>
  <exec_depend>geometry_msgs</exec_depend>
  <exec_depend>nav_msgs</exec_depend>

  <!-- Testing dependencies -->
  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>
  <test_depend>ament_cmake_gtest</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

**Dependency Types:**
- `<buildtool_depend>` - Build system tools (ament_cmake)
- `<build_depend>` - Compile-time dependencies
- `<build_export_depend>` - Headers exported to downstream packages
- `<exec_depend>` - Runtime dependencies
- `<test_depend>` - Testing-only dependencies
- `<depend>` - Shorthand for build + export + exec

**Common rosdep Keys for ROS2:**
- `rclcpp` - C++ client library
- `rclpy` - Python client library
- `std_msgs`, `geometry_msgs`, `nav_msgs`, `sensor_msgs` - Standard message types
- `rosidl_default_generators` - For custom message generation
- `ament_cmake` - CMake build system
- `ament_cmake_gtest` - Google Test integration
- `ament_lint_auto`, `ament_lint_common` - Code linting

## Pattern 7: Coverage and Quality Metrics

### Template: Code Coverage with action-ros-ci

```yaml
name: ROS2 CI with Coverage

on: [push, pull_request]

jobs:
  build_test_coverage:
    runs-on: ubuntu-latest
    container:
      image: rostooling/setup-ros-docker:ubuntu-noble-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build and test with coverage
        uses: ros-tooling/action-ros-ci@v0.4
        with:
          package-name: warehouser_simulation
          target-ros2-distro: jazzy
          colcon-defaults: |
            {
              "build": {
                "mixin": ["coverage-gcc", "coverage-pytest"]
              },
              "test": {
                "mixin": ["coverage-pytest"]
              }
            }
          colcon-mixin-repository: https://raw.githubusercontent.com/colcon/colcon-mixin-repository/master/index.yaml

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: ./ros_ws/build/*/coverage.xml
          flags: ros2
          name: ros2-coverage
```

**Key Features:**
- Uses colcon mixins for coverage instrumentation
- Supports both C++ (coverage-gcc) and Python (coverage-pytest)
- Uploads results to Codecov for tracking
- Coverage data stored in build artifacts

## Application to Warehouser

### Recommended CI Strategy

Create **three separate workflow files** for the Warehouser project:

#### 1. `.github/workflows/ros2-ci.yaml`

```yaml
name: ROS2 CI

on:
  push:
    branches: [main]
    paths:
      - 'ros_ws/**'
      - '.github/workflows/ros2-ci.yaml'
  pull_request:
    paths:
      - 'ros_ws/**'

jobs:
  build_and_test:
    runs-on: ubuntu-24.04
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup ROS 2 Jazzy
        uses: ros-tooling/setup-ros@v0.7
        with:
          required-ros-distributions: jazzy

      - name: Build and test ROS 2 packages
        uses: ros-tooling/action-ros-ci@v0.4
        with:
          package-name: |
            warehouser_msgs
            warehouser_simulation
            warehouser_observations
            warehouser_rl_bridge
          target-ros2-distro: jazzy
          colcon-defaults: |
            {
              "build": {
                "symlink-install": true,
                "cmake-args": ["-DCMAKE_BUILD_TYPE=Release"]
              }
            }

      - name: Upload build logs on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: colcon-logs
          path: ros_ws/log/
```

**Why this approach:**
- Non-Docker for compatibility with vcpkg C++23
- setup-ros provides clean ROS 2 Jazzy environment
- Symlink install for faster builds
- Build logs uploaded on failure for debugging

#### 2. `.github/workflows/python-ci.yaml`

```yaml
name: Python Training CI

on:
  push:
    branches: [main]
    paths:
      - 'training/**'
      - '.github/workflows/python-ci.yaml'
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

      - name: Set up Python 3.12
        run: uv python install 3.12

      - name: Install dependencies
        run: uv sync --dev

      - name: Run unit tests
        run: uv run pytest tests/ -v --ignore=tests/integration/

      - name: Type checking with mypy
        run: uv run mypy training/ --strict
```

**Why this approach:**
- Isolated from ROS (no ROS dependency for training code)
- Uses modern uv for fast Python dependency management
- Skips integration tests (require ROS running)
- Strict mypy checking enforced

#### 3. `.github/workflows/typescript-ci.yaml`

```yaml
name: TypeScript Frontend CI

on:
  push:
    branches: [main]
    paths:
      - 'web_frontend/**'
      - '.github/workflows/typescript-ci.yaml'
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

      - name: Setup Node.js 20
        uses: actions/setup-node@v4
        with:
          node-version: '20'
          cache: 'npm'
          cache-dependency-path: web_frontend/package-lock.json

      - name: Install dependencies
        run: npm ci

      - name: Type check
        run: npx tsc --noEmit

      - name: Lint
        run: npm run lint

      - name: Run tests
        run: npm test -- --run

      - name: Build
        run: npm run build
```

**Why this approach:**
- Isolated frontend testing
- npm caching for faster installs
- Explicit type checking before tests
- Vitest runs in CI mode (--run)

### Benefits of This Structure

1. **Parallel Execution** - All three workflows run simultaneously
2. **Fast Feedback** - Only affected workflows trigger on changes
3. **Independent Failures** - Frontend bug doesn't block ROS CI
4. **Clear Separation** - Each stack uses its own best practices
5. **Efficient Resource Usage** - No unnecessary Docker overhead

### Migration Path

1. **Phase 1: Create workflows** - Add all three workflow files
2. **Phase 2: Validate package.xml** - Ensure all ROS dependencies declared
3. **Phase 3: Test locally** - Run `rosdep install --from-paths ros_ws/src --ignore-src -r -y`
4. **Phase 4: Push and observe** - Let GitHub Actions run and fix any issues
5. **Phase 5: Add badges** - Add CI status badges to README.md

### Common Issues and Solutions

**Issue: rosdep install fails**
- **Solution:** Add missing dependencies to package.xml
- **Check:** Run locally: `rosdep check --from-paths ros_ws/src --ignore-src`

**Issue: Build order problems**
- **Solution:** Declare inter-package dependencies in package.xml
- **Example:** warehouser_simulation needs `<depend>warehouser_msgs</depend>`

**Issue: vcpkg dependencies not found**
- **Solution:** Pre-install vcpkg dependencies or use custom Docker image
- **Alternative:** Install via apt if available in Ubuntu repos

**Issue: Tests timing out**
- **Solution:** Skip integration tests in CI, use unit tests only
- **Pattern:** `pytest tests/ --ignore=tests/integration/`

## Comparison: action-ros-ci vs industrial_ci

| Feature | action-ros-ci | industrial_ci |
|---------|---------------|---------------|
| Setup complexity | Moderate (requires setup-ros) | Minimal (2 lines) |
| Flexibility | High | Low (opinionated) |
| Multi-language support | Excellent | ROS-focused |
| Docker required | Optional | Yes |
| Linting | Separate action | Built-in |
| Coverage | Manual setup | Built-in |
| ROS1 support | Yes | Yes |
| ROS2 support | Excellent | Good |
| Best for | Mixed ROS/non-ROS projects | Pure ROS projects |

**Recommendation for Warehouser:** Use action-ros-ci for maximum flexibility with multi-language codebase.

## References

- [GitHub - ros-tooling/action-ros-ci](https://github.com/ros-tooling/action-ros-ci)
- [GitHub - ros-tooling/action-ros-ci-template](https://github.com/ros-tooling/action-ros-ci-template)
- [ROS 2 CI with GitHub Actions | Ubuntu](https://ubuntu.com/blog/ros-2-ci-with-github-actions)
- [GitHub - ros-industrial/industrial_ci](https://github.com/ros-industrial/industrial_ci)
- [GitHub - ros-controls/ros2_control_ci](https://github.com/ros-controls/ros2_control_ci)
- [Managing Dependencies with rosdep - ROS 2 Jazzy](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Rosdep.html)
- [ROS 2 CI Action - GitHub Marketplace](https://github.com/marketplace/actions/ros-2-ci-action)
- [Setup CI/CD for a ROS2 project using Github | Medium](https://medium.com/@shantanuparab99/setup-ci-cd-for-a-ros2-project-using-github-121d62bae348)
