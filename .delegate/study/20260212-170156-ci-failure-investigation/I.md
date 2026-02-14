# Introspect

Created: 2026-02-12 17:02:00

## Focus

CI pipeline failure investigation across ROS2, Python training, and Docker build systems.

## CI Configuration Analysis

### Primary CI Workflow: `.github/workflows/ci.yml`

The CI pipeline consists of 4 parallel jobs plus a summary job:

1. **ros2-build-test** (ubuntu-24.04, ros:jazzy-ros-base container)
   - Builds all ROS2 packages with colcon
   - Runs unit tests (excluding integration tests)
   - Uses GCC 13 for C++23 support

2. **python-lint** (ubuntu-24.04)
   - Runs ruff check/format
   - Runs mypy strict type checking
   - Uses uv for dependency management

3. **python-test** (ubuntu-24.04)
   - Runs pytest with coverage
   - Excludes integration tests
   - Uploads coverage to codecov

4. **docker-build** (ubuntu-24.04)
   - Builds the Dockerfile
   - Tests caching

5. **ci-success** (summary job)
   - Depends on all above jobs
   - Fails if any dependency fails

### Secondary Workflow: `.github/workflows/release.yml`

Release workflow for tagged versions - not relevant to current CI failures.

## Root Cause: Missing ROS2 Dependencies

### Primary Issue

**Location**: `.github/workflows/ci.yml:24-37`

The CI workflow installs the following ROS2 dependencies:

```yaml
- ros-jazzy-std-msgs
- ros-jazzy-std-srvs
- ros-jazzy-geometry-msgs
- ros-jazzy-ament-cmake-gtest
- ros-jazzy-ament-lint-auto
- ros-jazzy-ament-lint-common
```

However, **two critical dependencies are missing**:
- `ros-jazzy-nav-msgs`
- `ros-jazzy-sensor-msgs`

### Why These Are Required

These dependencies were added in recent commits:

1. **Commit 9bec276** (Feb 2, 2026): "feat(observations): add OdometrySimulator and nav_msgs/Odometry publishing"
   - Added `nav_msgs` dependency to `warehouser_observations`
   - OdometrySimulator publishes `nav_msgs/Odometry` messages

2. **Commit 434ed99** (Feb 2, 2026): "feat(observations): add sensor_msgs/LaserScan publishing for SLAM"
   - Added `sensor_msgs` dependency to `warehouser_observations`
   - LidarSimulator publishes `sensor_msgs/LaserScan` messages

### Package Dependencies

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\package.xml:13-14`

```xml
<depend>nav_msgs</depend>
<depend>sensor_msgs</depend>
```

**File**: `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\CMakeLists.txt:19-20`

```cmake
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
```

Without these packages installed in CI, the build will fail at the CMake configuration stage with:

```
CMake Error at CMakeLists.txt:19 (find_package):
  By not providing "Findnav_msgs.cmake" in CMAKE_MODULE_PATH...
```

### Dockerfile Has Same Issue

**File**: `C:\Users\costa\src\warehouser\Dockerfile:5-17`

The Dockerfile also lacks these dependencies:

```dockerfile
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-13 \
    python3-colcon-common-extensions \
    python3-rosdep \
    ros-jazzy-std-msgs \
    ros-jazzy-std-srvs \
    ros-jazzy-geometry-msgs \
    ros-jazzy-ament-cmake-gtest \
    ros-jazzy-ament-lint-auto \
    ros-jazzy-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*
```

Missing:
- `ros-jazzy-nav-msgs`
- `ros-jazzy-sensor-msgs`

This will cause the `docker-build` job to fail with the same CMake error.

## Additional Findings

### 1. Git Status Shows Modified Files

**Location**: Git working directory

```
M .gitignore
M training/.coverage
M training/tests/__pycache__/...
M training/training/envs/__pycache__/...
M training/training/models/__pycache__/...
M training/training/utils/__pycache__/...
?? web_frontend/src/assets/
?? web_frontend/src/hooks/
```

**Issues**:
- `.gitignore` has uncommitted changes
- `.coverage` file is modified (should be gitignored)
- `__pycache__` directories are modified (should be gitignored)
- `web_frontend/src/assets/` and `web_frontend/src/hooks/` are untracked

**Analysis**:
- The `.gitignore` at line 15 has `*.coverage` which should catch `.coverage` files
- However, `training/.coverage` is already tracked, so the gitignore doesn't apply
- The `__pycache__` pattern at line 5-6 should catch these, but they're already tracked
- The untracked directories are legitimate new code

**Risk**: Low - these won't cause CI failure but indicate repository hygiene issues

### 2. Python Tests Are Well-Structured

**Location**: `C:\Users\costa\src\warehouser\training\tests\`

All Python tests follow best practices:
- Comprehensive test coverage with pytest
- Integration tests properly marked with `@pytest.mark.integration`
- Type hints throughout
- Pydantic validation tests are thorough

**Example**: `test_pettingzoo_env.py:169-204`
```python
@pytest.mark.integration
class TestWarehouseParallelEnvIntegration:
    """Integration tests requiring ROS2 services."""
```

CI correctly excludes integration tests: `pytest tests/ -v --ignore=tests/integration`

No issues found in Python test infrastructure.

### 3. Web Frontend Tests Not Run in CI

**Location**: `.github/workflows/ci.yml`

**Issue**: There is NO job for running web frontend tests.

The `web_frontend/package.json` defines:
```json
"test": "vitest run"
```

And there are test files:
- `web_frontend/src/store/appStore.test.ts`
- `web_frontend/src/components/ObjectivePanel.test.tsx`
- `web_frontend/src/components/StatusPanel.test.tsx`
- `web_frontend/src/components/ControlPanel.test.tsx`
- `web_frontend/src/components/Canvas.test.tsx`

**Risk**: Medium - These tests are not being verified in CI. While not causing the current failure, this is a gap in test coverage.

### 4. ROS2 Package Structure Is Correct

All ROS2 packages follow proper ament_cmake structure:
- CMakeLists.txt uses C++23
- package.xml format 3
- Proper test dependencies
- Integration tests correctly excluded in CI

No structural issues found.

### 5. C++23 Compiler Support

**Location**: `.github/workflows/ci.yml:29`, `Dockerfile:8`

Both use `g++-13` which supports C++23. This is correct for the codebase which uses:
```cmake
set(CMAKE_CXX_STANDARD 23)
```

No issues found.

## Proposal

### Immediate Fix (Required)

**1. Update CI workflow dependencies**

**File**: `.github/workflows/ci.yml:26-37`

Add the missing dependencies:

```yaml
apt-get install -y \
  build-essential \
  cmake \
  g++-13 \
  python3-colcon-common-extensions \
  python3-rosdep \
  ros-jazzy-std-msgs \
  ros-jazzy-std-srvs \
  ros-jazzy-geometry-msgs \
  ros-jazzy-nav-msgs \           # ADD THIS
  ros-jazzy-sensor-msgs \         # ADD THIS
  ros-jazzy-ament-cmake-gtest \
  ros-jazzy-ament-lint-auto \
  ros-jazzy-ament-lint-common
```

**2. Update Dockerfile dependencies**

**File**: `Dockerfile:11-16`

Add the same missing dependencies:

```dockerfile
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-13 \
    python3-colcon-common-extensions \
    python3-rosdep \
    ros-jazzy-std-msgs \
    ros-jazzy-std-srvs \
    ros-jazzy-geometry-msgs \
    ros-jazzy-nav-msgs \           # ADD THIS
    ros-jazzy-sensor-msgs \         # ADD THIS
    ros-jazzy-ament-cmake-gtest \
    ros-jazzy-ament-lint-auto \
    ros-jazzy-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*
```

### Secondary Improvements (Recommended)

**3. Add web frontend testing to CI**

Add a new job to `.github/workflows/ci.yml`:

```yaml
web-frontend-test:
  name: Web Frontend Test
  runs-on: ubuntu-24.04
  steps:
    - name: Checkout
      uses: actions/checkout@v4

    - name: Set up Node.js
      uses: actions/setup-node@v4
      with:
        node-version: '20'
        cache: 'npm'
        cache-dependency-path: web_frontend/package-lock.json

    - name: Install dependencies
      working-directory: web_frontend
      run: npm ci

    - name: Run tests
      working-directory: web_frontend
      run: npm test

    - name: Run lint
      working-directory: web_frontend
      run: npm run lint
```

Update the `ci-success` job to depend on this new job.

**4. Clean up repository tracked files**

Remove improperly tracked cache files:

```bash
git rm --cached training/.coverage
git rm -r --cached training/tests/__pycache__
git rm -r --cached training/training/envs/__pycache__
git rm -r --cached training/training/models/__pycache__
git rm -r --cached training/training/utils/__pycache__
```

Update `.gitignore` to be more explicit:

```gitignore
# Python
__pycache__/
*.py[cod]
*$py.class
*.so
.Python
*.coverage
.coverage
.coverage.*
.pytest_cache/
htmlcov/
.venv/
venv/
```

**5. Consider using rosdep for dependency management**

Instead of manually listing dependencies, use rosdep to automatically install them from package.xml:

```bash
rosdep update
rosdep install --from-paths src --ignore-src -r -y
```

This would eliminate the need to manually sync CI/Dockerfile dependencies with package.xml files.

## Summary

**Root Cause**: Recent commits (9bec276, 434ed99) added `nav_msgs` and `sensor_msgs` dependencies to `warehouser_observations` package, but CI workflow and Dockerfile were not updated to install these ROS2 packages.

**Impact**:
- `ros2-build-test` job fails at CMake configuration
- `docker-build` job fails at build stage
- CI cannot pass, blocking merges

**Fix Complexity**: Trivial - add two lines to CI and Dockerfile

**Verification**: After fix, run `colcon build` in CI and Docker to verify successful compilation.

**Timeline**: This should have been caught when commits 9bec276 and 434ed99 were added 10 days ago (Feb 2, 2026). The fact that CI is still failing suggests either:
1. CI was not run on those commits
2. CI was not required to pass
3. The commits were pushed directly to main without PR review
