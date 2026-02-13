import { Rect } from 'react-konva'
import { Entity } from '../../store/appStore'
import { CoordinateTransform } from '../../utils/transforms'
import { useSprite } from '../../hooks/useSprite'
import { WALL_TEXTURE } from '../../assets/sprites'

interface CanvasWallsProps {
  walls: Entity[]
  scale: number
  canvasSize: number
}

/**
 * Renders walls on the canvas with texture or fallback solid color.
 * Uses fillPatternImage for textured walls when the sprite is loaded.
 */
export function CanvasWalls({ walls, scale, canvasSize }: CanvasWallsProps) {
  const wallImage = useSprite(WALL_TEXTURE)
  const transform = new CoordinateTransform(canvasSize / scale, canvasSize)

  return (
    <>
      {walls.map((wall) => {
        // Convert world coordinates to canvas coordinates
        // Wall y position is adjusted by height for proper top-left positioning
        const [cx, cy] = transform.worldToCanvas(wall.x, wall.y + (wall.height || 0.1))
        const wallWidth = (wall.width || 0.1) * scale
        const wallHeight = (wall.height || 0.1) * scale

        return wallImage ? (
          <Rect
            key={wall.id}
            x={cx}
            y={cy}
            width={wallWidth}
            height={wallHeight}
            fillPatternImage={wallImage}
            fillPatternScaleX={wallWidth / 20}
            fillPatternScaleY={wallHeight / 60}
          />
        ) : (
          <Rect
            key={wall.id}
            x={cx}
            y={cy}
            width={wallWidth}
            height={wallHeight}
            fill="#666"
          />
        )
      })}
    </>
  )
}
