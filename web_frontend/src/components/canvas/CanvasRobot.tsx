import { Image, Circle, Arrow, Group } from 'react-konva'
import { Entity } from '../../store/appStore'
import { CoordinateTransform } from '../../utils/transforms'
import { useSprite } from '../../hooks/useSprite'
import { ROBOT_SPRITE } from '../../assets/sprites'

/**
 * Default robot sprite size in pixels
 */
const DEFAULT_ROBOT_SIZE_PIXELS = 40

/**
 * Stroke width for selected robot highlight
 */
const SELECTED_STROKE_WIDTH = 4

/**
 * Stroke width for normal robots
 */
const NORMAL_STROKE_WIDTH = 2

/**
 * Props for the CanvasRobots component.
 */
export interface CanvasRobotsProps {
  /** Array of robot entities to render */
  robots: Entity[]
  /** ID of the currently selected robot */
  selectedRobotId?: string | null
  /** Callback when a robot is clicked */
  onRobotSelect?: (id: string) => void
  /** Scale factor (pixels per meter) */
  scale: number
  /** Total canvas size in pixels */
  canvasSize: number
  /** Robot sprite size in pixels (default: 40) */
  robotSizePixels?: number
}

/**
 * Renders multiple robot entities on the canvas with sprite or fallback rendering.
 *
 * Features:
 * - Loads and displays ROBOT_SPRITE when available
 * - Falls back to circle with direction arrow if sprite fails to load
 * - Direct rendering without animation for instant updates (no lag with lidar)
 * - Orange ring indicator when robot is carrying an object
 * - Highlights selected robot with thicker stroke
 * - Click handler to select robots
 *
 * @example
 * ```tsx
 * const robots = entities.filter(e => e.type === 'robot');
 * return (
 *   <CanvasRobots
 *     robots={robots}
 *     selectedRobotId={selectedRobotId}
 *     onRobotSelect={(id) => setSelectedRobotId(id)}
 *     scale={60}
 *     canvasSize={600}
 *   />
 * );
 * ```
 */
export function CanvasRobots({
  robots,
  selectedRobotId,
  onRobotSelect,
  scale,
  canvasSize,
  robotSizePixels = DEFAULT_ROBOT_SIZE_PIXELS,
}: CanvasRobotsProps) {
  const robotImage = useSprite(ROBOT_SPRITE)

  // Create coordinate transformer
  const worldSize = canvasSize / scale
  const transform = new CoordinateTransform(worldSize, canvasSize)

  // Calculate fallback rendering values
  const fallbackRadius = 0.3 * scale
  const arrowLength = 0.4 * scale

  return (
    <>
      {robots.map((robot) => {
        const [canvasX, canvasY] = transform.worldToCanvas(robot.x, robot.y)
        const rotation = transform.worldThetaToCanvasRotation(robot.theta ?? 0)
        const isSelected = robot.id === selectedRobotId

        // Calculate arrow direction in canvas coordinates
        // In canvas coords (Y-down), we negate theta and offset by PI/2
        const arrowDirX = Math.cos(-robot.theta! + Math.PI / 2) * arrowLength
        const arrowDirY = Math.sin(-robot.theta! + Math.PI / 2) * arrowLength

        const handleClick = () => {
          if (onRobotSelect) {
            onRobotSelect(robot.id)
          }
        }

        if (robotImage) {
          // Render with sprite - direct rendering without animation
          return (
            <Group
              key={robot.id}
              x={canvasX}
              y={canvasY}
              rotation={rotation}
              onClick={handleClick}
              onTap={handleClick}
            >
              <Image
                image={robotImage}
                width={robotSizePixels}
                height={robotSizePixels}
                offsetX={robotSizePixels / 2}
                offsetY={robotSizePixels / 2}
              />
              {/* Selection highlight - cyan glow ring for selected robot */}
              {isSelected && (
                <Circle
                  radius={robotSizePixels / 2 + 6}
                  fill="transparent"
                  stroke="#22d3ee"
                  strokeWidth={SELECTED_STROKE_WIDTH}
                />
              )}
              {/* Carrying indicator - orange ring when robot is carrying an object */}
              {robot.isCarrying && (
                <Circle
                  radius={robotSizePixels / 2 + 4}
                  fill="transparent"
                  stroke="#f59e0b"
                  strokeWidth={3}
                />
              )}
            </Group>
          )
        }

        // Fallback rendering - circle with direction arrow
        return (
          <Group
            key={robot.id}
            x={canvasX}
            y={canvasY}
            onClick={handleClick}
            onTap={handleClick}
          >
            <Circle
              radius={fallbackRadius}
              fill={robot.isCarrying ? '#f59e0b' : '#60a5fa'}
              stroke={isSelected ? '#22d3ee' : '#fff'}
              strokeWidth={isSelected ? SELECTED_STROKE_WIDTH : NORMAL_STROKE_WIDTH}
            />
            {/* Direction arrow pointing in robot's heading direction */}
            <Arrow
              points={[0, 0, arrowDirX, arrowDirY]}
              pointerLength={8}
              pointerWidth={6}
              fill="#fff"
              stroke="#fff"
              strokeWidth={2}
            />
          </Group>
        )
      })}
    </>
  )
}

// Re-export for backward compatibility during transition
export { CanvasRobots as CanvasRobot }
