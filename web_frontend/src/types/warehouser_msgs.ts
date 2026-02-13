/**
 * TypeScript interfaces for ROS2 warehouser_msgs
 * Generated from: ros_ws/src/warehouser_msgs/msg/
 */

// ============================================================================
// Standard ROS2 Types
// ============================================================================

/**
 * ROS2 builtin_interfaces/Time
 * @property sec - Seconds since epoch (int32)
 * @property nanosec - Nanoseconds since the last second (uint32)
 */
export interface Time {
  sec: number;
  nanosec: number;
}

/**
 * ROS2 std_msgs/Header
 * @property stamp - Timestamp of the message
 * @property frame_id - Frame this data is associated with
 */
export interface Header {
  stamp: Time;
  frame_id: string;
}

// ============================================================================
// Entity Types
// ============================================================================

/**
 * Entity type enumeration matching ROS2 constants
 * - TYPE_ROBOT (0): Robot entity
 * - TYPE_OBJECT (1): Pickable object
 * - TYPE_WALL (2): Wall/obstacle
 * - TYPE_ZONE (3): Named zone area
 */
export const EntityType = {
  TYPE_ROBOT: 0,
  TYPE_OBJECT: 1,
  TYPE_WALL: 2,
  TYPE_ZONE: 3,
} as const;

export type EntityType = (typeof EntityType)[keyof typeof EntityType];

/**
 * Entity.msg - Represents any entity in the simulation world
 *
 * Coordinate system follows REP 103:
 * - X: forward, Y: left, Z: up
 * - Theta: counter-clockwise from X-axis
 */
export interface EntityInfo {
  /** Unique identifier for the entity */
  id: string;

  /** Entity type (see EntityType enum) */
  type: EntityType;

  // ---- Position (all entities) ----

  /** X position in meters (float32) */
  x: number;

  /** Y position in meters (float32) */
  y: number;

  // ---- Robot-specific fields ----

  /** Heading angle in radians, counter-clockwise from X-axis (float32) */
  theta: number;

  /** Linear velocity in m/s (float32) */
  v: number;

  /** Angular velocity in rad/s (float32) */
  omega: number;

  /** Whether robot is carrying an object */
  is_carrying: boolean;

  /** ID of carried object (empty if not carrying) */
  carried_object_id: string;

  // ---- Object-specific fields ----

  /** Object color name */
  color: string;

  /** Pickup interaction radius in meters (float32) */
  pickup_radius: number;

  /** Whether object has been picked up */
  is_picked: boolean;

  // ---- Wall-specific fields ----

  /** Wall width in meters (float32) */
  width: number;

  /** Wall height in meters (float32) */
  height: number;

  // ---- Zone-specific fields ----

  /** Zone display name */
  zone_name: string;

  /** Zone radius in meters (float32) */
  radius: number;
}

// ============================================================================
// World State
// ============================================================================

/**
 * WorldState.msg - Complete state of the simulation world
 */
export interface WorldState {
  /** Array of all entities in the world */
  entities: EntityInfo[];

  /** Simulation time in seconds (float32) */
  sim_time: number;

  /** Whether simulation is currently running */
  running: boolean;
}

// ============================================================================
// Lidar Debug
// ============================================================================

/**
 * LidarDebug.msg - Simulated lidar data for visualization/debugging
 *
 * Angles are in radians, following REP 103 conventions.
 * Ranges are in meters.
 */
export interface LidarDebug {
  /** Distance to obstacle for each ray in meters (float32[]) */
  ranges: number[];

  /** Start angle in radians (float32) */
  angle_min: number;

  /** End angle in radians (float32) */
  angle_max: number;

  /** Minimum valid range in meters (float32) */
  range_min: number;

  /** Maximum valid range in meters (float32) */
  range_max: number;

  /** Robot X position when scan was taken, in meters (float32) */
  robot_x: number;

  /** Robot Y position when scan was taken, in meters (float32) */
  robot_y: number;

  /** Robot heading when scan was taken, in radians (float32) */
  robot_theta: number;
}

// ============================================================================
// Task Status
// ============================================================================

/**
 * Task state enumeration
 */
export const TaskState = {
  IDLE: 'idle',
  SEEKING: 'seeking',
  PICKING: 'picking',
  DELIVERING: 'delivering',
  PLACING: 'placing',
  COMPLETE: 'complete',
  FAILED: 'failed',
} as const;

export type TaskState = (typeof TaskState)[keyof typeof TaskState];

/**
 * TaskStatus.msg - Current task state
 */
export interface TaskStatus {
  /** Unique task identifier */
  task_id: string;

  /** Current task state (see TaskState) */
  state: string;

  /** Task intent/description */
  intent: string;

  /** Target object color */
  target_color: string;

  /** Distance to goal in meters (float32) */
  distance_to_goal: number;
}

// ============================================================================
// Observation
// ============================================================================

/**
 * Observation version constants
 */
export const ObservationVersion = {
  /** Position-based (8 dims): [robot_x, robot_y, robot_theta, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying] */
  V1_POSITION: 1,
  /** Lidar-based (63 dims): [lidar_ranges(60), goal_bearing, goal_dist, is_carrying] */
  V2_LIDAR: 2,
} as const;

export type ObservationVersion =
  (typeof ObservationVersion)[keyof typeof ObservationVersion];

/**
 * Observation.msg - Observation vector for policy input
 */
export interface Observation {
  /** Observation format version */
  version: number;

  /** Observation data array (float32[]) */
  data: number[];
}

// ============================================================================
// Goal
// ============================================================================

/**
 * Goal.msg - Current navigation goal for the robot
 */
export interface Goal {
  /** Goal X position in meters (float32) */
  x: number;

  /** Goal Y position in meters (float32) */
  y: number;

  /** Color of target object, empty if position-only goal */
  target_color: string;

  /** Whether goal is currently active */
  active: boolean;
}

// ============================================================================
// Action
// ============================================================================

/**
 * Action.msg - Policy output action
 *
 * Velocity commands are normalized to [-1, 1] range.
 */
export interface Action {
  /** Linear velocity command, normalized [-1, 1] (float32) */
  linear: number;

  /** Angular velocity command, normalized [-1, 1] (float32) */
  angular: number;

  /** Pick action trigger */
  pick: boolean;

  /** Place action trigger */
  place: boolean;
}
