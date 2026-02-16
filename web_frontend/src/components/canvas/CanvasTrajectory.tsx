/**
 * CanvasTrajectory - Renders the robot's trajectory path on the canvas
 *
 * Displays a polyline showing the robot's movement history with:
 * - Smooth line rendering with rounded caps and joins
 * - Configurable color, stroke width, and opacity
 */

import { Line } from 'react-konva'
import { CoordinateTransform } from '../../utils/transforms'
import type { TrajectoryPoint } from '../../store/appStore'

export interface CanvasTrajectoryProps {
  points: TrajectoryPoint[]
  scale: number
  canvasSize: number
  color?: string
  strokeWidth?: number
  opacity?: number
}

const DEFAULT_COLOR = '#3b82f6' // Tailwind blue-500
const DEFAULT_STROKE_WIDTH = 2
const DEFAULT_OPACITY = 0.6

export function CanvasTrajectory({
  points,
  scale,
  canvasSize,
  color = DEFAULT_COLOR,
  strokeWidth = DEFAULT_STROKE_WIDTH,
  opacity = DEFAULT_OPACITY,
}: CanvasTrajectoryProps): JSX.Element | null {
  // Need at least 2 points to draw a line
  if (points.length < 2) {
    return null
  }

  // Create coordinate transformer
  const transform = new CoordinateTransform(canvasSize / scale, canvasSize)

  // Convert world coordinates to canvas coordinates and flatten to [x1, y1, x2, y2, ...]
  const flattenedPoints: number[] = []
  for (const point of points) {
    const [canvasX, canvasY] = transform.worldToCanvas(point.x, point.y)
    flattenedPoints.push(canvasX, canvasY)
  }

  return (
    <Line
      points={flattenedPoints}
      stroke={color}
      strokeWidth={strokeWidth}
      opacity={opacity}
      lineCap="round"
      lineJoin="round"
      tension={0}
    />
  )
}
