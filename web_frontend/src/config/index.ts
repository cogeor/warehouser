/**
 * Centralized configuration module for the Warehouser web frontend.
 * All configuration values are frozen for runtime immutability.
 */

// =============================================================================
// ROS Connection Settings
// =============================================================================

/**
 * ROS WebSocket connection URL.
 * Can be overridden via VITE_ROS_WS_URL environment variable.
 */
export const ROS_WS_URL: string =
  import.meta.env.VITE_ROS_WS_URL ?? 'ws://localhost:9090'

/**
 * Reconnection configuration for ROS WebSocket connection.
 * Uses exponential backoff with jitter for robust reconnection handling.
 */
export const RECONNECT_CONFIG = Object.freeze({
  /** Maximum number of reconnection attempts before giving up */
  maxAttempts: 10,
  /** Initial delay between reconnection attempts (ms) */
  baseDelay: 1000,
  /** Maximum delay between reconnection attempts (ms) */
  maxDelay: 30000,
  /** Exponential backoff multiplier */
  factor: 2,
  /** Jitter factor (0.1 = +/- 10% randomization) */
  jitter: 0.1,
} as const)

/** Type for reconnection configuration */
export type ReconnectConfig = typeof RECONNECT_CONFIG

// =============================================================================
// Canvas Settings
// =============================================================================

/**
 * Canvas and world rendering configuration.
 * Coordinates follow REP 103: X forward, Y left, Z up.
 * Canvas rendering flips Y axis for screen coordinates.
 */
export const CANVAS_CONFIG = Object.freeze({
  /** World size in meters (square world) */
  WORLD_SIZE: 10,
  /** Canvas size in pixels (square canvas) */
  CANVAS_SIZE: 600,
  /** Animation duration for entity movements (ms) */
  ANIMATION_DURATION: 100,
  /** Robot diameter in meters */
  ROBOT_SIZE: 0.6,
  /** Object size in meters (pickable items) */
  OBJECT_SIZE: 0.4,
} as const)

/** Type for canvas configuration */
export type CanvasConfig = typeof CANVAS_CONFIG

/**
 * Helper to convert world coordinates to canvas pixels.
 * Accounts for Y-axis flip (world Y-up to canvas Y-down).
 */
export function worldToCanvas(worldX: number, worldY: number): { x: number; y: number } {
  const scale = CANVAS_CONFIG.CANVAS_SIZE / CANVAS_CONFIG.WORLD_SIZE
  return {
    x: worldX * scale,
    y: CANVAS_CONFIG.CANVAS_SIZE - worldY * scale, // Flip Y
  }
}

/**
 * Helper to convert canvas pixels to world coordinates.
 * Accounts for Y-axis flip (canvas Y-down to world Y-up).
 */
export function canvasToWorld(canvasX: number, canvasY: number): { x: number; y: number } {
  const scale = CANVAS_CONFIG.WORLD_SIZE / CANVAS_CONFIG.CANVAS_SIZE
  return {
    x: canvasX * scale,
    y: (CANVAS_CONFIG.CANVAS_SIZE - canvasY) * scale, // Flip Y
  }
}

/**
 * Convert world size (meters) to canvas pixels.
 */
export function worldSizeToCanvas(worldSize: number): number {
  return worldSize * (CANVAS_CONFIG.CANVAS_SIZE / CANVAS_CONFIG.WORLD_SIZE)
}

// =============================================================================
// Demo Mode Settings
// =============================================================================

/**
 * Demo mode configuration for simulating robot activity when ROS is not connected.
 */
export const DEMO_CONFIG = Object.freeze({
  /** Interval between demo state updates (ms) */
  DEMO_INTERVAL: 3000,
  /** Available colors for demo objects */
  COLORS: Object.freeze(['red', 'green', 'blue', 'yellow'] as const),
} as const)

/** Type for demo configuration */
export type DemoConfig = typeof DEMO_CONFIG

/** Type for available demo colors */
export type DemoColor = (typeof DEMO_CONFIG.COLORS)[number]

// =============================================================================
// Aggregated Configuration
// =============================================================================

/**
 * Complete configuration object combining all settings.
 * Useful for passing entire config to components or testing.
 */
export const CONFIG = Object.freeze({
  ros: {
    url: ROS_WS_URL,
    reconnect: RECONNECT_CONFIG,
  },
  canvas: CANVAS_CONFIG,
  demo: DEMO_CONFIG,
} as const)

/** Type for the complete configuration */
export type Config = typeof CONFIG
