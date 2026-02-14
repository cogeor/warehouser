# Loop 01: Add missing ROS2 nav-msgs and sensor-msgs dependencies to CI and Dockerfile

## Overview

This loop fixes the critical CI pipeline failure by adding two missing ROS2 package dependencies (`ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs`) to both the GitHub Actions workflow and the Dockerfile. These dependencies were added to `warehouser_observations/package.xml` in commits 9bec276 and 434ed99, but the CI infrastructure was not updated, causing CMake configuration failures.

## Tasks

### Task 1: Add missing dependencies to GitHub Actions CI workflow

**Goal:** Add `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` to the apt install command in the ros2-build-test job.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\.github\workflows\ci.yml` |

**Steps:**
1. Locate the `Install dependencies` step in the `ros2-build-test` job (lines 23-37)
2. Add `ros-jazzy-nav-msgs \` after line 34 (`ros-jazzy-geometry-msgs \`)
3. Add `ros-jazzy-sensor-msgs \` after the new nav-msgs line
4. Ensure proper line continuation with backslashes

**Before (lines 32-37):**
```yaml
            ros-jazzy-std-msgs \
            ros-jazzy-std-srvs \
            ros-jazzy-geometry-msgs \
            ros-jazzy-ament-cmake-gtest \
            ros-jazzy-ament-lint-auto \
            ros-jazzy-ament-lint-common
```

**After:**
```yaml
            ros-jazzy-std-msgs \
            ros-jazzy-std-srvs \
            ros-jazzy-geometry-msgs \
            ros-jazzy-nav-msgs \
            ros-jazzy-sensor-msgs \
            ros-jazzy-ament-cmake-gtest \
            ros-jazzy-ament-lint-auto \
            ros-jazzy-ament-lint-common
```

**Verify:** Review the YAML syntax is valid (proper indentation and line continuations)

---

### Task 2: Add missing dependencies to Dockerfile

**Goal:** Add `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` to the apt-get install command in the Dockerfile.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\Dockerfile` |

**Steps:**
1. Locate the `apt-get install` command (lines 5-17)
2. Add `ros-jazzy-nav-msgs \` after line 13 (`ros-jazzy-geometry-msgs \`)
3. Add `ros-jazzy-sensor-msgs \` after the new nav-msgs line
4. Ensure proper line continuation with backslashes

**Before (lines 11-17):**
```dockerfile
    ros-jazzy-std-msgs \
    ros-jazzy-std-srvs \
    ros-jazzy-geometry-msgs \
    ros-jazzy-ament-cmake-gtest \
    ros-jazzy-ament-lint-auto \
    ros-jazzy-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*
```

**After:**
```dockerfile
    ros-jazzy-std-msgs \
    ros-jazzy-std-srvs \
    ros-jazzy-geometry-msgs \
    ros-jazzy-nav-msgs \
    ros-jazzy-sensor-msgs \
    ros-jazzy-ament-cmake-gtest \
    ros-jazzy-ament-lint-auto \
    ros-jazzy-ament-lint-common \
    && rm -rf /var/lib/apt/lists/*
```

**Verify:** Run `docker build --no-cache -t warehouser:test .` to confirm the image builds successfully

---

### Task 3: Verify dependency alignment with package.xml

**Goal:** Confirm the added dependencies match what is declared in the ROS2 package manifest.

**Files:**
| Action | Path |
|--------|------|
| READ | `C:\Users\costa\src\warehouser\ros_ws\src\warehouser_observations\package.xml` |

**Steps:**
1. Verify `nav_msgs` dependency exists in package.xml (line 13)
2. Verify `sensor_msgs` dependency exists in package.xml (line 14)
3. Confirm the Jazzy package names follow the pattern `ros-jazzy-{package-name-with-dashes}`

**Verify:** The package.xml already contains:
```xml
<depend>nav_msgs</depend>
<depend>sensor_msgs</depend>
```

These map to `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` respectively.

---

## Acceptance Criteria

- [ ] `.github/workflows/ci.yml` includes `ros-jazzy-nav-msgs` in the apt install list
- [ ] `.github/workflows/ci.yml` includes `ros-jazzy-sensor-msgs` in the apt install list
- [ ] `Dockerfile` includes `ros-jazzy-nav-msgs` in the apt-get install list
- [ ] `Dockerfile` includes `ros-jazzy-sensor-msgs` in the apt-get install list
- [ ] YAML syntax is valid (no indentation errors)
- [ ] Dockerfile syntax is valid (no line continuation errors)
- [ ] Dependencies are placed in alphabetical order among the ros-jazzy packages for consistency

## Commit Message

```
fix(ci): add missing nav-msgs and sensor-msgs dependencies

Add ros-jazzy-nav-msgs and ros-jazzy-sensor-msgs to CI workflow
and Dockerfile. These dependencies were added to warehouser_observations
in commits 9bec276 and 434ed99 but the CI configuration was not updated,
causing CMake configuration failures.
```
