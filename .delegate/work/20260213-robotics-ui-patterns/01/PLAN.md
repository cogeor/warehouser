# Loop 01: Create ROS Message TypeScript Interfaces

## Objective

Create TypeScript interfaces that mirror the ROS2 message definitions from `warehouser_msgs` package, enabling type-safe communication between the web frontend and ROS2 backend.

## Source Files

- `ros_ws/src/warehouser_msgs/msg/Entity.msg`
- `ros_ws/src/warehouser_msgs/msg/WorldState.msg`
- `ros_ws/src/warehouser_msgs/msg/LidarDebug.msg`
- `ros_ws/src/warehouser_msgs/msg/TaskStatus.msg`
- `ros_ws/src/warehouser_msgs/msg/Observation.msg`
- `ros_ws/src/warehouser_msgs/msg/Goal.msg`
- `ros_ws/src/warehouser_msgs/msg/Action.msg`

## Output File

- `web_frontend/src/types/warehouser_msgs.ts`

## Tasks

### Task 1: Create Standard ROS2 Type Interfaces

**Files to modify:** `web_frontend/src/types/warehouser_msgs.ts` (new)

**Implementation:**
- Create `Time` interface (sec, nanosec)
- Create `Header` interface (stamp, frame_id)
- Add JSDoc comments for field types

### Task 2: Create EntityInfo Interface with EntityType Enum

**Files to modify:** `web_frontend/src/types/warehouser_msgs.ts`

**Implementation:**
- Create `EntityType` const object with TYPE_ROBOT, TYPE_OBJECT, TYPE_WALL, TYPE_ZONE
- Create `EntityInfo` interface with all fields from Entity.msg
- Group fields by entity type (common, robot-specific, object-specific, wall-specific, zone-specific)
- Document coordinate system (REP 103) and units (meters, radians)

### Task 3: Create WorldState Interface

**Files to modify:** `web_frontend/src/types/warehouser_msgs.ts`

**Implementation:**
- Create `WorldState` interface with entities array, sim_time, running flag

### Task 4: Create LidarDebug Interface

**Files to modify:** `web_frontend/src/types/warehouser_msgs.ts`

**Implementation:**
- Create `LidarDebug` interface with ranges array, angle/range bounds, robot pose
- Document angle conventions (radians) and range units (meters)

### Task 5: Create TaskStatus Interface with TaskState Enum

**Files to modify:** `web_frontend/src/types/warehouser_msgs.ts`

**Implementation:**
- Create `TaskState` const object with state values
- Create `TaskStatus` interface with task_id, state, intent, target_color, distance_to_goal

### Task 6: Create Supporting Interfaces

**Files to modify:** `web_frontend/src/types/warehouser_msgs.ts`

**Implementation:**
- Create `Observation` interface with version enum
- Create `Goal` interface
- Create `Action` interface with velocity normalization documentation

## Verification

1. TypeScript compilation passes with no errors
2. All ROS message fields are represented
3. Enums match ROS constant values exactly
4. JSDoc comments document types and units

## Dependencies

- TypeScript 5+ strict mode
- No runtime dependencies (pure type definitions)
