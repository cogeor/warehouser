/**
 * CanvasLidar - Renders lidar visualization on the canvas
 *
 * Displays lidar rays emanating from the robot's position with:
 * - Ray lines showing the sensor sweep
 * - Endpoint dots with distance-based opacity (brighter = closer contact)
 * - Center glow at the robot origin
 */

import { Group, Line, Circle } from 'react-konva'
import { CoordinateTransform } from '../../utils/transforms'

export interface CanvasLidarProps {
  robotX: number
  robotY: number
  robotTheta: number
  ranges: number[]
  angleMin: number
  angleMax: number
  scale: number
  canvasSize: number
  maxRange?: number
}

const DEFAULT_MAX_RANGE = 5.0

export function CanvasLidar({
  robotX,
  robotY,
  robotTheta,
  ranges,
  angleMin,
  angleMax,
  scale,
  canvasSize,
  maxRange = DEFAULT_MAX_RANGE,
}: CanvasLidarProps): JSX.Element | null {
  // Render nothing if no lidar data
  if (ranges.length === 0) {
    return null
  }

  // Create coordinate transformer
  const transform = new CoordinateTransform(canvasSize / scale, canvasSize)
  const [rx, ry] = transform.worldToCanvas(robotX, robotY)
  const numRays = ranges.length

  return (
    <>
      {ranges.map((range, i) => {
        // Calculate the angle for this ray in world coordinates
        const angle = robotTheta + angleMin + (i * (angleMax - angleMin)) / (numRays - 1)

        // Calculate endpoint in canvas coordinates
        // Note: Canvas Y is flipped, so we adjust the angle accordingly
        const ex = rx + Math.cos(-angle + Math.PI / 2) * range * scale
        const ey = ry + Math.sin(-angle + Math.PI / 2) * range * scale

        // Calculate distance ratio for opacity (closer = brighter endpoint)
        const distanceRatio = Math.min(range / maxRange, 1.0)
        const endpointOpacity = 0.8 - distanceRatio * 0.4 // 0.8 at close, 0.4 at far

        return (
          <Group key={`lidar-${i}`}>
            {/* Main ray line - soft cyan/green sensor color */}
            <Line
              points={[rx, ry, ex, ey]}
              stroke="rgba(0, 230, 180, 0.25)"
              strokeWidth={1.5}
              lineCap="round"
            />
            {/* Endpoint dot - brighter at contact points */}
            <Circle x={ex} y={ey} radius={2.5} fill={`rgba(0, 255, 200, ${endpointOpacity})`} />
          </Group>
        )
      })}
      {/* Center glow at robot origin */}
      <Circle x={rx} y={ry} radius={6} fill="rgba(0, 255, 200, 0.15)" />
    </>
  )
}
