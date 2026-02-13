# Build Failures Report

## Summary

During Docker build of the demo image, two compilation errors were discovered that were not caught by existing tests.

---

## Issue 1: Missing `sensor_msgs` Dependency in Library Target

**File:** `ros_ws/src/warehouser_observations/CMakeLists.txt:34-37`

**Error:**
```
fatal error: sensor_msgs/msg/laser_scan.hpp: No such file or directory
```

**Root Cause:**
The `warehouser_observations_lib` library includes headers from `sensor_msgs` (via `lidar_simulator.hpp`), but the CMakeLists.txt only linked `sensor_msgs` to the executable (`observations_node`), not the library.

**Fix:**
```cmake
# Before
ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  warehouser_msgs
)

# After
ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  sensor_msgs  # Added
  warehouser_msgs
)
```

**Why Tests Didn't Catch This:**

| Gap | Explanation |
|-----|-------------|
| **No CI with clean Docker build** | Local builds may have cached dependencies or different include paths |
| **Incremental builds** | `colcon build` reuses previous artifacts; error only appears on clean build |
| **Tests link against pre-built library** | If library was built before the header dependency was added, tests pass with stale object files |
| **No header-only compilation test** | CMake doesn't verify all transitive includes are satisfied |

**Recommended Fix:**
1. Add CI job that builds in fresh Docker container
2. Add `colcon build --cmake-clean-cache` to CI pipeline
3. Consider using `ament_target_dependencies` consistently for all targets that use a dependency

---

## Issue 2: Wrong Message Field Name (`color` vs `target_color`)

**File:** `ros_ws/src/warehouser_command/src/command_node.cpp:87`

**Error:**
```
error: 'Goal_<std::allocator<void>>' has no member named 'color'
   87 |         goal.color = obj->color;
      |              ^~~~~
```

**Root Cause:**
The `Goal.msg` defines the field as `target_color`, but the C++ code used `color`. This is a simple typo/mismatch that went undetected.

**Fix:**
```cpp
// Before
goal.color = obj->color;

// After
goal.target_color = obj->color;
```

**Why Tests Didn't Catch This:**

| Gap | Explanation |
|-----|-------------|
| **No unit tests for `CommandNode`** | The `command_node.cpp` has no corresponding test file |
| **Integration tests skip command flow** | Tests focus on world state, not command execution |
| **Message field rename without grep** | When `Goal.msg` was created/modified, code using it wasn't updated |
| **No compile-time coverage check** | C++ templates defer error until instantiation |

**Recommended Fix:**
1. Add unit test for `CommandNode::executeCommand()`
2. Add integration test that sends JSON command and verifies goal publication
3. When modifying `.msg` files, run `grep -r "old_field_name"` across C++ sources
4. Consider using a linter that checks message field usage

---

## Test Coverage Analysis

### Current Test Files for Affected Packages

**warehouser_observations:**
- `test/test_observation_builder.cpp` - Tests ObservationBuilder logic
- `test/test_lidar_simulator.cpp` - Tests lidar simulation
- `test/test_odometry_simulator.cpp` - Tests odometry
- `test/test_noise_model.cpp` - Tests noise models
- `test/integration/test_observations_node.cpp` - Integration tests

**Issue:** All tests compile against the *already-built* library. They don't verify the library can be built from scratch.

**warehouser_command:**
- `test/test_command_parser.cpp` - Tests JSON parsing only
- `test/test_object_resolver.cpp` - Tests object resolution
- `test/test_zone_resolver.cpp` - Tests zone resolution

**Issue:** No test coverage for `CommandNode` class itself. The main node logic is untested.

---

## Recommendations

### Immediate Actions
1. **Add clean build to CI** - `docker build` or `colcon build --cmake-clean-cache`
2. **Add CommandNode tests** - Unit tests for `executeCommand()` method

### Long-term Improvements
1. **Message compatibility linter** - Script that parses `.msg` files and checks C++ usage
2. **Header dependency scanner** - Verify all `#include` statements have corresponding CMake dependencies
3. **Integration test suite** - End-to-end tests that exercise full command flow

### CI Pipeline Addition
```yaml
# .github/workflows/build.yml
jobs:
  clean-build:
    runs-on: ubuntu-latest
    container: ros:jazzy-ros-base
    steps:
      - uses: actions/checkout@v4
      - name: Clean build
        run: |
          cd ros_ws
          colcon build --cmake-clean-cache
          colcon test
```

---

---

## Issue 3: Same `color` vs `target_color` Mismatch in TaskManagerNode

**File:** `ros_ws/src/warehouser_task/src/task_manager_node.cpp:89,95,203`

**Error:**
```
error: 'Goal_<std::allocator<void>>' has no member named 'color'
   89 |     task.target_color = msg->color;
   95 |     if (msg->color.empty()) {
  203 |     goal_msg.color = task->target_color;
```

**Root Cause:**
Same issue as Issue 2 - code referencing `color` field instead of `target_color`. This is in a different package (`warehouser_task`), showing the mismatch propagated across multiple packages.

**Fix:**
```cpp
// Line 89: msg->color -> msg->target_color
// Line 95: msg->color.empty() -> msg->target_color.empty()
// Line 203: goal_msg.color -> goal_msg.target_color
```

**Why Tests Didn't Catch This:**

| Gap | Explanation |
|-----|-------------|
| **No unit tests for TaskManagerNode** | The `task_manager_node.cpp` has no corresponding test file |
| **Mock-based tests don't use real messages** | If any tests exist, they likely mock the Goal message |
| **Cross-package dependency not tested** | Changes to `warehouser_msgs` don't trigger `warehouser_task` tests |

---

## Pattern Analysis

All three issues share common characteristics:

1. **No cross-package integration tests** - Packages are tested in isolation
2. **No clean build in CI** - Incremental builds hide dependency issues
3. **Node classes lack unit tests** - Business logic in nodes is untested
4. **Message field changes not validated** - `.msg` modifications don't trigger consumer validation

---

## Timeline

| Date | Issue | Discovered By | Time to Fix |
|------|-------|---------------|-------------|
| 2026-02-13 | sensor_msgs missing | Docker build | 2 minutes |
| 2026-02-13 | goal.color typo (command) | Docker build | 1 minute |
| 2026-02-13 | goal.color typo (task) | Docker build | 1 minute |
| 2026-02-13 | WorldState.width/height | Docker build | 1 minute |
| 2026-02-13 | Missing cmath include | Docker build | 1 minute |
| 2026-02-13 | std::expected unsupported | Docker build | 5 minutes |

All issues were trivial to fix but should have been caught automatically.

---

## Issue 7: ROS2 Message Type Format in Frontend

**Files:** `web_frontend/src/ros/subscriptions.ts`, `connection.ts`, `hooks/useRosTopic.ts`

**Error:**
Frontend connects to rosbridge but immediately disconnects. No data flows.

**Root Cause:**
ROS2 rosbridge requires message types in format `package/msg/Type` or `package/srv/Type`, not `package/Type`.

**Examples:**
- Wrong: `warehouser_msgs/WorldState`
- Correct: `warehouser_msgs/msg/WorldState`

- Wrong: `std_srvs/Trigger`
- Correct: `std_srvs/srv/Trigger`

**Fix:**
Updated all message type strings to use `/msg/` or `/srv/` format.

**Why Tests Didn't Catch This:**

| Gap | Explanation |
|-----|-------------|
| **No frontend integration tests** | No tests that actually connect to rosbridge |
| **ROS1 vs ROS2 difference** | ROS1 doesn't require `/msg/`, only ROS2 does |
| **Silent failure** | rosbridge doesn't log clear error for wrong message type |
| **Manual browser testing only** | No automated end-to-end tests |

**This is a documentation gap:** The ROS2 rosbridge message type format isn't well documented for frontend developers.

---

## Issue 4: Test Code Uses Non-Existent Message Fields

**File:** `ros_ws/src/warehouser_observations/test/test_observation_builder.cpp:245-246,332-333`

**Error:**
```
error: 'WorldState_<std::allocator<void>>' has no member named 'width'
  245 |         world_.width = 20.0f;
error: 'WorldState_<std::allocator<void>>' has no member named 'height'
  246 |         world_.height = 20.0f;
```

**Root Cause:**
Test code was written assuming `WorldState.msg` has `width` and `height` fields, but the actual message only has `entities`, `sim_time`, and `running`.

**Fix:**
Removed the invalid lines since they're not needed for the test logic.

**Why Tests Didn't Catch This:**

| Gap | Explanation |
|-----|-------------|
| **Tests only run locally** | Local builds use cached message artifacts; tests pass with stale generated code |
| **Incremental builds** | If message was built before, test compilation uses old `.hpp` files |
| **No full rebuild in CI** | CI doesn't `colcon build --cmake-clean-cache` before running tests |
| **Wishful coding** | Developer wrote test assuming fields that would be added later |

**This is particularly ironic:** The very test meant to validate observations used invalid message fields.

---

## Issue 5: Missing `<cmath>` Include for `std::sqrt`

**File:** `ros_ws/src/warehouser_rl_bridge/test/test_multi_robot.cpp:81`

**Error:**
```
error: 'sqrt' is not a member of 'std'; did you mean 'sort'?
   81 |                 float dist = std::sqrt(dx * dx + dy * dy);
```

**Root Cause:**
Test uses `std::sqrt` but doesn't include `<cmath>`.

**Fix:**
Added `#include <cmath>` to the includes.

**Why Tests Didn't Catch This:**
- **Header pollution**: Other includes may transitively include `<cmath>` on some compilers
- **Platform differences**: MSVC may include math functions differently than GCC

---

## Issue 6: C++23 `std::expected` Not Fully Supported

**File:** `ros_ws/src/warehouser_inference/include/warehouser_inference/policy_inference.hpp`

**Error:**
```
error: 'expected' in namespace 'std' does not name a template type
   42 |     std::expected<void, std::string> loadModel(const std::string& model_path);
```

**Root Cause:**
`std::expected` is C++23 but Ubuntu 24.04's libstdc++ doesn't fully support it even with GCC 13 and `-std=c++23`.

**Fix:**
Replaced `std::expected` with a custom `Result<T>` template using `std::variant` and `std::optional`.

**Why Tests Didn't Catch This:**

| Gap | Explanation |
|-----|-------------|
| **Windows vs Linux** | MSVC may have different C++23 support than GCC |
| **Local GCC version** | Developer's GCC may be newer than Docker image |
| **No Docker CI** | Clean Docker builds would have caught this immediately |

**This is a systemic issue:** C++23 adoption requires careful checking of library support, not just compiler flags.
