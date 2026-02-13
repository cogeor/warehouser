import type { LegacyRef } from 'react'
import { Image, Circle, Arrow } from 'react-konva'
import Konva from 'konva'
import { Entity } from '../../store/appStore'
import { CoordinateTransform } from '../../utils/transforms'
import { useSprite } from '../../hooks/useSprite'
import { useEntityAnimation } from '../../hooks/useEntityAnimation'
import { ROBOT_SPRITE } from '../../assets/sprites'

/**
 * Default robot sprite size in pixels
 */
const DEFAULT_ROBOT_SIZE_PIXELS = 40

/**
 * Props for the CanvasRobot component.
 */
interface CanvasRobotProps {
  /** Robot entity to render */
  robot: Entity
  /** Scale factor (pixels per meter) */
  scale: number
  /** Total canvas size in pixels */
  canvasSize: number
  /** Robot sprite size in pixels (default: 40) */
  robotSizePixels?: number
}

/**
 * Renders a robot entity on the canvas with sprite or fallback rendering.
 *
 * Features:
 * - Loads and displays ROBOT_SPRITE when available
 * - Falls back to circle with direction arrow if sprite fails to load
 * - Smooth animation between positions using useEntityAnimation
 * - Orange ring indicator when robot is carrying an object
 *
 * @example
 * ```tsx
 * const robot = entities.find(e => e.type === 'robot');
 * if (robot) {
 *   return (
 *     <CanvasRobot
 *       robot={robot}
 *       scale={60}
 *       canvasSize={600}
 *     />
 *   );
 * }
 * ```
 */
export function CanvasRobot({
  robot,
  scale,
  canvasSize,
  robotSizePixels = DEFAULT_ROBOT_SIZE_PIXELS,
}: CanvasRobotProps) {
  const robotImage = useSprite(ROBOT_SPRITE)

  // Create coordinate transformer
  const worldSize = canvasSize / scale
  const transform = new CoordinateTransform(worldSize, canvasSize)

  // Convert world coordinates to canvas coordinates
  const [canvasX, canvasY] = transform.worldToCanvas(robot.x, robot.y)
  const rotation = transform.worldThetaToCanvasRotation(robot.theta ?? 0)

  // Animate robot position and rotation
  const robotRef = useEntityAnimation<Konva.Image>({
    x: canvasX,
    y: canvasY,
    rotation,
  })

  // Animate carrying indicator
  const carryIndicatorRef = useEntityAnimation<Konva.Circle>({
    x: canvasX,
    y: canvasY,
  })

  // Animate fallback circle
  const fallbackCircleRef = useEntityAnimation<Konva.Circle>({
    x: canvasX,
    y: canvasY,
  })

  // Animate direction arrow
  const directionArrowRef = useEntityAnimation<Konva.Arrow>({
    x: canvasX,
    y: canvasY,
  })

  // Calculate fallback rendering values
  const fallbackRadius = 0.3 * scale
  const arrowLength = 0.4 * scale

  // Calculate arrow direction in canvas coordinates
  // In canvas coords (Y-down), we negate theta and offset by PI/2
  const arrowDirX = Math.cos(-robot.theta! + Math.PI / 2) * arrowLength
  const arrowDirY = Math.sin(-robot.theta! + Math.PI / 2) * arrowLength

  if (robotImage) {
    // Render with sprite
    return (
      <>
        <Image
          ref={robotRef as unknown as LegacyRef<Konva.Image>}
          image={robotImage}
          x={canvasX}
          y={canvasY}
          width={robotSizePixels}
          height={robotSizePixels}
          offsetX={robotSizePixels / 2}
          offsetY={robotSizePixels / 2}
          rotation={rotation}
        />
        {/* Carrying indicator - orange ring when robot is carrying an object */}
        {robot.isCarrying && (
          <Circle
            ref={carryIndicatorRef as unknown as LegacyRef<Konva.Circle>}
            x={canvasX}
            y={canvasY}
            radius={robotSizePixels / 2 + 4}
            fill="transparent"
            stroke="#f59e0b"
            strokeWidth={3}
          />
        )}
      </>
    )
  }

  // Fallback rendering - circle with direction arrow
  return (
    <>
      <Circle
        ref={fallbackCircleRef as unknown as LegacyRef<Konva.Circle>}
        x={canvasX}
        y={canvasY}
        radius={fallbackRadius}
        fill={robot.isCarrying ? '#f59e0b' : '#60a5fa'}
        stroke="#fff"
        strokeWidth={2}
      />
      {/* Direction arrow pointing in robot's heading direction */}
      <Arrow
        ref={directionArrowRef as unknown as LegacyRef<Konva.Arrow>}
        x={canvasX}
        y={canvasY}
        points={[0, 0, arrowDirX, arrowDirY]}
        pointerLength={8}
        pointerWidth={6}
        fill="#fff"
        stroke="#fff"
        strokeWidth={2}
      />
    </>
  )
}
