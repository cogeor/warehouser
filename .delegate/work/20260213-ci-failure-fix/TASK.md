# TASK: Fix CI Pipeline Failures - Missing ROS2 Dependencies

Created: 2026-02-12 17:30:00
Build: SKIPPED (local environment issue - missing pydantic)
Tests: N/A (local test failed on dependency import)

## Summary

The CI pipeline is failing because the GitHub Actions workflow and Dockerfile are missing critical ROS2 package dependencies that were added in recent commits. Specifically, `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` were added to the `warehouser_observations` package on February 2, 2026 (commits 9bec276 and 434ed99), but the CI configuration files were not updated accordingly.

## Root Cause

On February 2, 2026, two commits introduced new ROS2 message dependencies:

1. **Commit 9bec276**: Added OdometrySimulator publishing `nav_msgs/Odometry` messages
2. **Commit 434ed99**: Added LidarSimulator publishing `sensor_msgs/LaserScan` messages

These dependencies were correctly added to `warehouser_observations/package.xml` and `CMakeLists.txt`, but the CI infrastructure was not updated:

- `.github/workflows/ci.yml` lacks `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs`
- `Dockerfile` lacks the same two dependencies

**Impact**: Both `ros2-build-test` and `docker-build` jobs fail at CMake configuration with:
```
CMake Error: By not providing "Findnav_msgs.cmake" in CMAKE_MODULE_PATH...
```

## Objective

Fix the immediate CI failures by adding missing ROS2 dependencies, then implement modern CI best practices to prevent similar issues in the future.

## Implementation Plan

### Phase 1: Immediate Fix (Required - Unblocks CI)

**Priority: CRITICAL - Blocks all PR merges**

- [ ] Add `ros-jazzy-nav-msgs` to `.github/workflows/ci.yml` dependency list
- [ ] Add `ros-jazzy-sensor-msgs` to `.github/workflows/ci.yml` dependency list
- [ ] Add `ros-jazzy-nav-msgs` to `Dockerfile` dependency list
- [ ] Add `ros-jazzy-sensor-msgs` to `Dockerfile` dependency list
- [ ] Verify CI passes after changes

### Phase 2: Modernize CI Infrastructure (Recommended)

**Priority: HIGH - Prevents future dependency sync issues**

- [ ] Create `.github/workflows/ros2-ci.yaml` using `action-ros-ci`
- [ ] Create `.github/workflows/python-ci.yaml` for training tests
- [ ] Create `.github/workflows/typescript-ci.yaml` for web frontend tests
- [ ] Update `.github/workflows/ci.yml` to call the three new workflows
- [ ] Add path-based triggers to prevent unnecessary CI runs

### Phase 3: Repository Hygiene (Optional but Recommended)

**Priority: MEDIUM - Improves repository cleanliness**

- [ ] Remove tracked cache files: `git rm --cached training/.coverage`
- [ ] Remove tracked `__pycache__` directories
- [ ] Update `.gitignore` to be more explicit about coverage files

## Files to Modify

| File | Change |
|------|--------|
| `.github/workflows/ci.yml:26-37` | Add `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` to apt install list |
| `Dockerfile:11-16` | Add `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` to apt install list |
| `.gitignore` | Add explicit patterns: `.coverage`, `.coverage.*`, `__pycache__/` |

## Files to Create (Phase 2)

| File | Purpose |
|------|---------|
| `.github/workflows/ros2-ci.yaml` | Modern ROS2 CI using action-ros-ci with automatic rosdep |
| `.github/workflows/python-ci.yaml` | Python training tests with uv, mypy, pytest |
| `.github/workflows/typescript-ci.yaml` | TypeScript frontend tests with npm, vitest |
