import { useState, useCallback } from 'react'
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
  CanvasRobots,
} from './canvas/index'

const FLOOR_TILE_SIZE = 60

export function Canvas() {
  const entities = useAppStore((s) => s.entities)
  const lidarRanges = useAppStore((s) => s.lidarRanges)
  const lidarAngleMin = useAppStore((s) => s.lidarAngleMin)
  const lidarAngleMax = useAppStore((s) => s.lidarAngleMax)
  const selectedRobotId = useAppStore((s) => s.selectedRobotId)
  const setSelectedRobotId = useAppStore((s) => s.setSelectedRobotId)

  // Pan state for dragging the canvas
  const [pan, setPan] = useState({ x: 0, y: 0 })

  // Reset pan to origin (exported for future use by MapPanel)
  const resetPan = useCallback(() => {
    setPan({ x: 0, y: 0 })
  }, [])

  // Expose resetPan on window for debugging/future integration
  // TODO: Replace with ref/context when MapPanel needs access
  if (typeof window !== 'undefined') {
    ;(window as unknown as { __canvasResetPan?: () => void }).__canvasResetPan = resetPan
  }

  // Filter entities by type
  const robots = entities.filter((e) => e.type === 'robot')
  const objects = entities.filter((e) => e.type === 'object')
  const walls = entities.filter((e) => e.type === 'wall')
  const zones = entities.filter((e) => e.type === 'zone')

  // Get selected robot for lidar (default to first robot if none selected)
  const selectedRobot = robots.find((r) => r.id === selectedRobotId) ?? robots[0]

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
      draggable
      x={pan.x}
      y={pan.y}
      onDragEnd={(e) => {
        setPan({
          x: e.target.x(),
          y: e.target.y(),
        })
      }}
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
        {selectedRobot && (
          <CanvasLidar
            robotX={selectedRobot.x}
            robotY={selectedRobot.y}
            robotTheta={selectedRobot.theta ?? 0}
            ranges={lidarRanges}
            angleMin={lidarAngleMin}
            angleMax={lidarAngleMax}
            scale={scale}
            canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          />
        )}

        {/* Robots */}
        <CanvasRobots
          robots={robots}
          selectedRobotId={selectedRobotId}
          onRobotSelect={setSelectedRobotId}
          scale={scale}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
        />
      </Layer>
    </Stage>
  )
}
