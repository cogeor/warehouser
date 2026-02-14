/**
 * Coordinate transformation utilities for converting between:
 * - ROS coordinates (X forward, Y left, Z up, theta CCW from X-axis)
 * - Canvas coordinates (X right, Y down, origin top-left, rotation CW)
 */

/** 2D transform representing position and orientation */
export interface Transform2D {
  x: number
  y: number
  theta: number
}

/** Default constants matching Canvas.tsx */
export const WORLD_SIZE = 10 // meters
export const CANVAS_SIZE = 600 // pixels
export const SCALE = CANVAS_SIZE / WORLD_SIZE // 60 pixels per meter

/**
 * CoordinateTransform handles conversion between ROS world coordinates
 * and canvas pixel coordinates.
 *
 * ROS Coordinate System (REP 103):
 * - X: forward (positive)
 * - Y: left (positive)
 * - Z: up (positive)
 * - Theta: counter-clockwise from X-axis
 *
 * Canvas Coordinate System:
 * - X: right (positive)
 * - Y: down (positive)
 * - Origin: top-left corner
 * - Rotation: clockwise from positive X-axis (right)
 */
export class CoordinateTransform {
  readonly worldSize: number
  readonly canvasSize: number
  readonly scale: number

  /**
   * Create a coordinate transformer
   * @param worldSize - Size of the world in meters (assumes square)
   * @param canvasSize - Size of the canvas in pixels (assumes square)
   */
  constructor(worldSize: number = WORLD_SIZE, canvasSize: number = CANVAS_SIZE) {
    if (worldSize <= 0) {
      throw new Error('worldSize must be positive')
    }
    if (canvasSize <= 0) {
      throw new Error('canvasSize must be positive')
    }
    this.worldSize = worldSize
    this.canvasSize = canvasSize
    this.scale = canvasSize / worldSize
  }

  /**
   * Convert world coordinates to canvas coordinates
   * - World X maps to Canvas X (scaled)
   * - World Y is flipped and maps to Canvas Y (Y-down in canvas)
   *
   * @param x - World X coordinate (meters)
   * @param y - World Y coordinate (meters)
   * @returns [canvasX, canvasY] tuple in pixels
   */
  worldToCanvas(x: number, y: number): [number, number] {
    const canvasX = x * this.scale
    const canvasY = this.canvasSize - y * this.scale
    return [canvasX, canvasY]
  }

  /**
   * Convert canvas coordinates to world coordinates
   * - Canvas X maps to World X (unscaled)
   * - Canvas Y is flipped and maps to World Y
   *
   * @param cx - Canvas X coordinate (pixels)
   * @param cy - Canvas Y coordinate (pixels)
   * @returns [worldX, worldY] tuple in meters
   */
  canvasToWorld(cx: number, cy: number): [number, number] {
    const worldX = cx / this.scale
    const worldY = (this.canvasSize - cy) / this.scale
    return [worldX, worldY]
  }

  /**
   * Convert world theta (CCW from X-axis) to canvas rotation (CW degrees)
   *
   * ROS theta: 0 = facing +X (forward/right), positive = CCW
   * Canvas rotation: in degrees, positive = CW
   * Sprite default: facing UP (-Y in canvas)
   *
   * The conversion accounts for:
   * 1. Y-axis flip (negates angle direction)
   * 2. Sprite orientation offset (+90 degrees to rotate UP-facing sprite to RIGHT)
   * 3. Radians to degrees conversion
   *
   * @param theta - World theta in radians (CCW from X-axis)
   * @returns Canvas rotation in degrees (CW from X-axis, adjusted for sprite)
   */
  worldThetaToCanvasRotation(theta: number): number {
    // Negate for Y-flip, convert to degrees, offset by +90 for sprite orientation
    // Sprite faces UP by default, +90° CW rotation makes it face RIGHT (theta=0)
    return (-theta * 180) / Math.PI + 90
  }

  /**
   * Convert canvas rotation (CW degrees) to world theta (CCW radians)
   *
   * Inverse of worldThetaToCanvasRotation
   *
   * @param rotation - Canvas rotation in degrees
   * @returns World theta in radians (CCW from X-axis)
   */
  canvasRotationToWorldTheta(rotation: number): number {
    // Reverse the sprite offset, convert to radians, negate for Y-flip
    return (-(rotation - 90) * Math.PI) / 180
  }

  /**
   * Transform a point by applying a 2D rigid body transformation
   *
   * Applies rotation then translation (standard SE(2) transformation)
   *
   * @param point - [x, y] point to transform in local coordinates
   * @param pose - Transform2D pose defining the transformation
   * @returns [x, y] transformed point in world coordinates
   */
  transformPoint(point: [number, number], pose: Transform2D): [number, number] {
    const [px, py] = point
    const cos = Math.cos(pose.theta)
    const sin = Math.sin(pose.theta)

    // Apply rotation then translation
    const worldX = cos * px - sin * py + pose.x
    const worldY = sin * px + cos * py + pose.y

    return [worldX, worldY]
  }

  /**
   * Inverse transform a point from world coordinates to local coordinates
   *
   * Applies inverse translation then inverse rotation
   *
   * @param worldPoint - [x, y] point in world coordinates
   * @param pose - Transform2D pose defining the reference frame
   * @returns [x, y] point in local coordinates relative to pose
   */
  inverseTransformPoint(worldPoint: [number, number], pose: Transform2D): [number, number] {
    const [wx, wy] = worldPoint
    const cos = Math.cos(pose.theta)
    const sin = Math.sin(pose.theta)

    // Apply inverse translation then inverse rotation
    const dx = wx - pose.x
    const dy = wy - pose.y
    const localX = cos * dx + sin * dy
    const localY = -sin * dx + cos * dy

    return [localX, localY]
  }

  /**
   * Calculate the distance between two world points
   *
   * @param p1 - First point [x, y]
   * @param p2 - Second point [x, y]
   * @returns Euclidean distance in world units (meters)
   */
  distance(p1: [number, number], p2: [number, number]): number {
    const dx = p2[0] - p1[0]
    const dy = p2[1] - p1[1]
    return Math.sqrt(dx * dx + dy * dy)
  }

  /**
   * Normalize an angle to the range [-PI, PI]
   *
   * @param angle - Angle in radians
   * @returns Normalized angle in radians within [-PI, PI]
   */
  normalizeAngle(angle: number): number {
    let normalized = angle % (2 * Math.PI)
    if (normalized > Math.PI) {
      normalized -= 2 * Math.PI
    } else if (normalized < -Math.PI) {
      normalized += 2 * Math.PI
    }
    return normalized
  }
}

/** Default singleton instance with standard world/canvas sizes */
export const defaultTransform = new CoordinateTransform()
