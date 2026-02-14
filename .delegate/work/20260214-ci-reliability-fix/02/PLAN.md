# Loop 02: Fix Python mypy type errors and ruff lint issues

## Overview

This loop fixes 39 mypy type errors across 8 files and ruff lint errors (import sorting, line length) to make the Python CI checks pass. The fixes are organized by category: generic type parameters, ndarray type assignments, enum comparisons, Space attribute access, and import/formatting issues.

## Tasks

### Task 1: Add generic type parameters to ActionWrapper subclasses

**Goal:** Fix missing type parameters for `gym.ActionWrapper` generic class.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/training/wrappers/safety.py` |
| MODIFY | `training/training/wrappers/action_smoothing.py` |
| MODIFY | `training/training/wrappers/action_scaling.py` |
| MODIFY | `training/training/wrappers/accel_limit.py` |

**Steps:**
1. In `safety.py` line 15, change:
   ```python
   class SafetyClippingWrapper(gym.ActionWrapper):
   ```
   to:
   ```python
   class SafetyClippingWrapper(gym.ActionWrapper[NDArray[np.float32], NDArray[np.float32]]):
   ```
2. Apply the same pattern to `action_smoothing.py` line 14
3. Apply the same pattern to `action_scaling.py` line 14
4. Apply the same pattern to `accel_limit.py` line 14

**Verify:** `cd training && uv run mypy training/wrappers/ --strict`

---

### Task 2: Fix ndarray vs float type mismatch in safety.py

**Goal:** Fix incompatible types in assignment where `np.clip` returns `NDArray` but is assigned to a scalar index.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/training/wrappers/safety.py` |

**Steps:**
1. At lines 77, 87 (and similar), the issue is that `np.clip(action[0], ...)` returns a numpy scalar. Cast the result explicitly:
   ```python
   clipped_action[0] = np.float32(np.clip(
       action[0],
       self.hard_limits["linear"][0],
       self.hard_limits["linear"][1],
   ))
   ```
2. Apply the same fix to all four clipped assignments (lines 116-135)

**Verify:** `cd training && uv run mypy training/wrappers/safety.py --strict`

---

### Task 3: Fix PettingZoo ParallelEnv subclass typing

**Goal:** Fix "Class cannot subclass value of type Any" and remove unused type: ignore comments.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/training/envs/pettingzoo_env.py` |

**Steps:**
1. At line 25, the issue is that `ParallelEnv` may not be fully typed. Add a type: ignore comment only for the subclass line:
   ```python
   class WarehouseParallelEnv(ParallelEnv[AgentID, Observation, Action]):  # type: ignore[misc]
   ```
2. Remove the unused `# type: ignore[import-not-found]` comments on lines 107, 108, 111, 163, 164, 236, 237 if they are no longer needed, OR verify they are still required for rclpy/warehouser_msgs imports (these are valid since those packages are not available during type checking)
3. Review lines 86, 91: These `# type: ignore[return-value]` comments may be needed because the property returns `dict[AgentID, spaces.Box]` but type is `dict[AgentID, spaces.Space[...]]`. Verify if they are still needed after the class fix.

**Verify:** `cd training && uv run mypy training/envs/pettingzoo_env.py --strict`

---

### Task 4: Fix ObservationVersion enum comparison in test_config.py

**Goal:** Fix non-overlapping equality checks between `ObservationVersion` enum and `int`.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/tests/test_config.py` |

**Steps:**
1. At lines 244-246 and 256-257, the tests compare `ObservationVersion.V1_Basic == 5`. Since `ObservationVersion` is an IntEnum, these comparisons should work. The fix is to use `.value` property or cast to int:
   ```python
   # Line 244-246: Change
   assert ObservationVersion.V1_Basic == 5
   # To:
   assert ObservationVersion.V1_Basic.value == 5
   # Or use int():
   assert int(ObservationVersion.V1_Basic) == 5
   ```
2. Apply the same pattern to lines 256-257

**Verify:** `cd training && uv run mypy tests/test_config.py --strict`

---

### Task 5: Fix Space attribute access in test files

**Goal:** Fix `Space[Any]` has no attribute `low`/`high` errors by using proper type narrowing.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/tests/test_action_wrappers.py` |
| MODIFY | `training/tests/test_pettingzoo_env.py` |

**Steps:**
1. In `test_action_wrappers.py` lines 121-128, 581-588: The `wrapped.action_space` is typed as `Space[Any]` but we access `.low` and `.high` which only exist on `Box`. Add assertions or casts:
   ```python
   # Before accessing .low/.high:
   assert isinstance(wrapped.action_space, gym.spaces.Box)
   assert wrapped.action_space.low[0] == pytest.approx(-1.5)
   ```
2. In `test_pettingzoo_env.py` lines 79-80: Same issue with `space.low` and `space.high`. Add type narrowing:
   ```python
   space = env.action_space(agent)
   assert isinstance(space, spaces.Box)
   assert np.all(space.low == -1.0)
   ```

**Verify:** `cd training && uv run mypy tests/test_action_wrappers.py tests/test_pettingzoo_env.py --strict`

---

### Task 6: Fix create_wrapper_chain argument type in test_action_wrappers.py

**Goal:** Fix argument incompatible type with `create_wrapper_chain`.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/tests/test_action_wrappers.py` |

**Steps:**
1. At line 904, the `**config` unpacking passes incompatible types. The config dict has string keys but the function expects specific keyword arguments. Fix by explicitly passing arguments:
   ```python
   wrapped = self.create_wrapper_chain(
       env,
       velocity_limits=config["velocity_limits"],
       smoothing_alpha=config["smoothing_alpha"],
       accel_limits=config["accel_limits"],
       accel_dt=config["accel_dt"],
       hard_limits=config["hard_limits"],
   )
   ```
   Or add type hints to the config dict using TypedDict.

**Verify:** `cd training && uv run mypy tests/test_action_wrappers.py --strict`

---

### Task 7: Fix ruff import sorting issues

**Goal:** Fix import block sorting errors.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/tests/test_action_wrappers.py` |
| MODIFY | `training/tests/test_scripts.py` |

**Steps:**
1. In `test_action_wrappers.py` line 8: Reorder imports. The `from typing import Any` should be first (standard library), then third-party imports. Current order appears to be:
   ```python
   from typing import Any

   import gymnasium as gym
   import numpy as np
   import pytest
   ```
   Ensure alphabetical order within groups.
2. In `test_scripts.py` line 6: Similar issue. Reorder imports to follow isort conventions (stdlib, then third-party, then local).

**Verify:** `cd training && uv run ruff check tests/test_action_wrappers.py tests/test_scripts.py --select=I`

---

### Task 8: Fix line length issues in test_config.py

**Goal:** Fix lines exceeding 100 character limit.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `training/tests/test_config.py` |

**Steps:**
1. At line 152: Break the long comment across multiple lines or shorten:
   ```python
   # V1 ego-centric: [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
   assert config.obs_dim == 5
   ```
2. At line 210: Similar fix - break the comment:
   ```python
   # Ego-centric: [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
   assert config.obs_dim == 5
   ```

**Verify:** `cd training && uv run ruff check tests/test_config.py --select=E501`

---

## Acceptance Criteria

- [ ] `cd training && uv run mypy training/ tests/ --strict` passes with 0 errors
- [ ] `cd training && uv run ruff check training/ tests/` passes with 0 errors
- [ ] `cd training && uv run pytest tests/ -v` all tests still pass
- [ ] No new type: ignore comments added except where strictly necessary (e.g., imports from untyped packages)
