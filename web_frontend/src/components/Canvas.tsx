import { Stage, Layer } from 'react-konva'
import { useAppStore } from '../store/appStore'
import { publishMoveEntity } from '../ros/connection'
import { CANVAS_CONFIG } from '../config'
import {
  CanvasFloor,
  CanvasWalls,
  CanvasZones,
  CanvasObjects,
  CanvasLidar,
  CanvasRobot,
} from './canvas/index'

const FLOOR_TILE_SIZE = 60

export function Canvas() {
  const entities = useAppStore((s) => s.entities)
  const lidarRanges = useAppStore((s) => s.lidarRanges)
  const lidarAngleMin = useAppStore((s) => s.lidarAngleMin)
  const lidarAngleMax = useAppStore((s) => s.lidarAngleMax)

  // Filter entities by type
  const robot = entities.find((e) => e.type === 'robot')
  const objects = entities.filter((e) => e.type === 'object')
  const walls = entities.filter((e) => e.type === 'wall')
  const zones = entities.filter((e) => e.type === 'zone')

  // Compute scale from config values
  const scale = CANVAS_CONFIG.CANVAS_SIZE / CANVAS_CONFIG.WORLD_SIZE

  // Callback for when objects are dragged
  const handleObjectMoved = (id: string, worldX: number, worldY: number) => {
    publishMoveEntity(id, worldX, worldY)
  }

  return (
    <Stage
      width={CANVAS_CONFIG.CANVAS_SIZE}
      height={CANVAS_CONFIG.CANVAS_SIZE}
      className="border border-gray-600 bg-gray-900"
    >
      <Layer>
        {/* Floor tiles */}
        <CanvasFloor
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          worldSize={CANVAS_CONFIG.WORLD_SIZE}
          tileSize={FLOOR_TILE_SIZE}
        />

        {/* Walls */}
        <CanvasWalls
          walls={walls}
          scale={scale}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
        />

        {/* Zones */}
        <CanvasZones
          zones={zones}
          scale={scale}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
        />

        {/* Objects */}
        <CanvasObjects
          objects={objects}
          scale={scale}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          onObjectMoved={handleObjectMoved}
        />

        {/* Lidar visualization */}
        {robot && (
          <CanvasLidar
            robotX={robot.x}
            robotY={robot.y}
            robotTheta={robot.theta ?? 0}
            ranges={lidarRanges}
            angleMin={lidarAngleMin}
            angleMax={lidarAngleMax}
            scale={scale}
            canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          />
        )}

        {/* Robot */}
        {robot && (
          <CanvasRobot
            robot={robot}
            scale={scale}
            canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          />
        )}
      </Layer>
    </Stage>
  )
}
