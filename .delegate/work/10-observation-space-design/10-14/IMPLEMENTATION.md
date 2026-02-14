# Implementation Log - Loops 10-14 (Ego-Centric Observations)

## Task 10: Refactor V1 to remove absolute positions (ego-centric)

Completed: 2026-02-13T17:00:00Z

### Changes

- `ros_ws/src/warehouser_observations/include/warehouser_observations/observation_builder.hpp`:
  - Updated V1_Position comment: 5 dims [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
  - Updated V3_MultiRobot comment: 5 + 3*max_other_robots dims
  - Updated buildV1 comment: "Build V1 ego-centric observation (5 dims)"
  - Updated buildV3 comment: "Includes ego state (5 dims)"

- `ros_ws/src/warehouser_observations/src/observation_builder.cpp`:
  - `observationDim()`: V1 returns 5, V3 returns 5 + 3*max_other_robots, default returns 5
  - `buildV1()`: Removed obs.data[0-2] (robot_x, robot_y, robot_theta), resized to 5
  - New indices: goal_dx=[0], goal_dy=[1], goal_dist=[2], goal_heading=[3], is_carrying=[4]

### Verification

- [x] Code compiles (syntax verified)
- [x] New V1 observation is 5 dimensions
- [x] Goal-relative data preserved

### Notes

- BREAKING CHANGE: Existing V1 models incompatible (8 dims -> 5 dims)
- Ego-centric observations improve generalization (no absolute positions to overfit)

---

## Task 11: Update V1 unit tests for new 5-dim observation

Completed: 2026-02-13T17:05:00Z

### Changes

- `ros_ws/src/warehouser_observations/test/test_observation_builder.cpp`:
  - `V1ObservationHas8Dimensions` -> `V1ObservationHas5Dimensions`: EXPECT_EQ(obs.data.size(), 5u)
  - `ObservationDimReturnsCorrectSize`: EXPECT_EQ(builder.observationDim(), 5u)
  - `RobotPositionInObservation` -> `GoalDeltaInEgoCentricObservation`: Tests goal_dx/goal_dy at [0]/[1]
  - `GoalDeltaCalculation`: Updated indices to [0]/[1]
  - `GoalDistanceCalculation`: Updated index to [2]
  - `GoalHeadingWhenFacingGoal`: Updated index to [3]
  - `GoalHeadingWhenGoalToLeft`: Updated index to [3]
  - `GoalHeadingWhenGoalBehind`: Updated index to [3]
  - `CarryingFlagFalse/True`: Updated index to [4]
  - `NoRobotReturnsZeros`: EXPECT_EQ(obs.data.size(), 5u)
  - `BuildWithRobotIndexZeroDefault`: Updated size to 5, check goal_dx at [0]
  - `BuildWithExplicitRobotIndex`: Updated to check goal_dx/goal_dy at [0]/[1]
  - `BuildWithInvalidIndexReturnsZeros`: EXPECT_EQ(obs.data.size(), 5u)

### Verification

- [x] Test names updated to reflect ego-centric observations
- [x] All index references updated for 5-dim layout

---

## Task 12: Refactor V3 to remove absolute ego position

Completed: 2026-02-13T17:10:00Z

### Changes

- `ros_ws/src/warehouser_observations/src/observation_builder.cpp`:
  - `buildV3()`: Removed ego absolute position (obs.data[0-2])
  - First 5 dims now match V1: [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
  - Other robot base index: base = 5 + i * 3 (was 8 + i * 3)
  - Total V3 dims: 5 + 3*3 = 14 (was 17)

### Verification

- [x] V3 ego state matches V1 format
- [x] Other robot indices offset correctly

---

## Task 13: Update V3 unit tests for new 14-dim observation

Completed: 2026-02-13T17:15:00Z

### Changes

- `ros_ws/src/warehouser_observations/test/test_observation_builder.cpp`:
  - `ObservationDimWithMaxOtherRobots`: EXPECT_EQ(builder.observationDim(), 14u)
  - `ObservationDimWithDifferentMaxRobots`: 5 + 3*5 = 20 (was 23)
  - `V3ObservationHasCorrectSize`: EXPECT_EQ(obs.data.size(), 14u)
  - `V3EgoStateFirst8DimsMatchV1` -> `V3EgoStateFirst5DimsMatchV1`: Loop to 5
  - `V3OtherRobotRelativePositionNoRotation`: Indices [5,6,7] (was [8,9,10])
  - `V3OtherRobotRelativePositionWithRotation`: Indices [5,6] (was [8,9])
  - `V3PaddingWhenFewerRobots`: Updated indices [5,8-13] (was [8,11-16])
  - `V3FromDifferentRobotPerspective`: Updated for ego-centric state, indices [5,6]
  - `V3WithThreeRobots`: Updated indices [5,6,8-13] (was [8,9,11-16])
  - `V3NoRobotFoundReturnsZeros`: EXPECT_EQ(obs.data.size(), 14u)
  - `V3SingleRobotNoOthers`: Updated for ego-centric state, loop [5,14)

### Verification

- [x] All V3 tests updated for 14-dim layout
- [x] Other robot slot indices correctly adjusted

---

## Task 14: Update Python config defaults for ego-centric observations

Completed: 2026-02-13T17:20:00Z

### Changes

- `training/training/models/config.py`:
  - `ObservationVersion.V1_Basic`: 8 -> 5
  - `ObservationVersion.V3_MultiRobot`: 17 -> 14
  - Updated docstring: V1 is now [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
  - Updated EnvConfig docstring for new dimensions
  - Updated field description: "(V1=5, V2=63, V3=14)"

- `training/tests/test_config.py`:
  - `test_defaults`: assert config.obs_dim == 5
  - `test_obs_dim_v1_basic`: assert config.obs_dim == 5
  - `test_obs_dim_v3_multi_robot`: assert config.obs_dim == 14
  - `test_version_values`: V1=5, V3=14
  - `test_version_is_int_compatible`: int(V1)=5, int(V3)=14

### Verification

- [x] pytest tests/test_config.py: 49 passed
- [x] ObservationVersion enum values updated
- [x] EnvConfig defaults work with new dimensions

### Notes

- Python and C++ observation dimensions now synchronized
- BREAKING CHANGE: Existing models trained with V1 (8-dim) or V3 (17-dim) incompatible

---
