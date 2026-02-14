import { Image, Line } from 'react-konva'
import { FLOOR_TILE } from '../../assets/sprites'
import { useSprite } from '../../hooks/useSprite'
import { CANVAS_CONFIG } from '../../config'

/**
 * Props for the CanvasFloor component.
 */
interface CanvasFloorProps {
  /** Total canvas size in pixels */
  canvasSize: number
  /** World size in meters (used for fallback grid line count) */
  worldSize: number
  /** Size of each floor tile in pixels */
  tileSize: number
}

/**
 * Renders the floor layer for the warehouse canvas.
 *
 * Displays a tiled floor texture when the sprite loads successfully,
 * or falls back to a simple grid pattern if the image fails to load.
 *
 * @example
 * ```tsx
 * <CanvasFloor
 *   canvasSize={600}
 *   worldSize={10}
 *   tileSize={60}
 * />
 * ```
 */
export function CanvasFloor({
  canvasSize = CANVAS_CONFIG.CANVAS_SIZE,
  worldSize = CANVAS_CONFIG.WORLD_SIZE,
  tileSize,
}: CanvasFloorProps) {
  const floorImage = useSprite(FLOOR_TILE)
  const scale = canvasSize / worldSize

  // Calculate number of tiles needed to cover the canvas
  const tileCount = Math.ceil(canvasSize / tileSize)

  if (floorImage) {
    // Render tiled floor texture
    return (
      <>
        {Array.from({ length: tileCount }).map((_, row) =>
          Array.from({ length: tileCount }).map((_, col) => (
            <Image
              key={`floor-${row}-${col}`}
              image={floorImage}
              x={col * tileSize}
              y={row * tileSize}
              width={tileSize}
              height={tileSize}
            />
          ))
        )}
      </>
    )
  }

  // Fallback: render simple grid lines
  const gridLineCount = worldSize + 1

  return (
    <>
      {/* Vertical grid lines */}
      {Array.from({ length: gridLineCount }).map((_, i) => (
        <Line
          key={`grid-v-${i}`}
          points={[i * scale, 0, i * scale, canvasSize]}
          stroke="#E5E7EB"
          strokeWidth={1}
        />
      ))}
      {/* Horizontal grid lines */}
      {Array.from({ length: gridLineCount }).map((_, i) => (
        <Line
          key={`grid-h-${i}`}
          points={[0, i * scale, canvasSize, i * scale]}
          stroke="#E5E7EB"
          strokeWidth={1}
        />
      ))}
    </>
  )
}
