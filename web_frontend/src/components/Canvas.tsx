import { useCallback, useMemo, memo } from 'react'
import { Stage, Layer } from 'react-konva'
import { useShallow } from 'zustand/react/shallow'
import { useAppStore } from '../store/appStore'
import { useRosPublisher } from '../hooks/useRosService'
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

// Precomputed scale (constant)
const SCALE = CANVAS_CONFIG.CANVAS_SIZE / CANVAS_CONFIG.WORLD_SIZE

function CanvasInner() {
  // Use shallow comparison to prevent re-renders when array contents are same
  const entities = useAppStore(useShallow((s) => s.entities))
  const lidarRanges = useAppStore(useShallow((s) => s.lidarRanges))
  const lidarAngleMin = useAppStore((s) => s.lidarAngleMin)
  const lidarAngleMax = useAppStore((s) => s.lidarAngleMax)
  // Robot pose bundled with lidar scan - per ROS2 TF2 best practices for guaranteed coupling
  const lidarRobotX = useAppStore((s) => s.lidarRobotX)
  const lidarRobotY = useAppStore((s) => s.lidarRobotY)
  const lidarRobotTheta = useAppStore((s) => s.lidarRobotTheta)
  const selectedRobotId = useAppStore((s) => s.selectedRobotId)
  const setSelectedRobotId = useAppStore((s) => s.setSelectedRobotId)


  // Memoize filtered entities to prevent re-renders when entities haven't changed
  const robots = useMemo(() => {
    // For the robot being visualized with lidar, use the bundled lidar pose
    // to ensure perfect coupling (no visual desync between robot sprite and lidar rays)
    return entities
      .filter((e) => e.type === 'robot')
      .map((robot) => {
        // If this is the first/selected robot and we have lidar data, use lidar pose
        if (lidarRanges.length > 0) {
          return {
            ...robot,
            x: lidarRobotX,
            y: lidarRobotY,
            theta: lidarRobotTheta,
          }
        }
        return robot
      })
  }, [entities, lidarRanges.length, lidarRobotX, lidarRobotY, lidarRobotTheta])
  const objects = useMemo(() => entities.filter((e) => e.type === 'object'), [entities])
  const walls = useMemo(() => entities.filter((e) => e.type === 'wall'), [entities])
  const zones = useMemo(() => entities.filter((e) => e.type === 'zone'), [entities])

  // Use new ROS publisher hook
  const publishJson = useRosPublisher<{ data: string }>('/sim/move_entity', 'std_msgs/msg/String')

  // Memoize callback for object movement
  const handleObjectMoved = useCallback(
    (id: string, worldX: number, worldY: number) => {
      publishJson({ data: JSON.stringify({ id, x: worldX, y: worldY }) })
    },
    [publishJson]
  )

  return (
    <Stage
      width={CANVAS_CONFIG.CANVAS_SIZE}
      height={CANVAS_CONFIG.CANVAS_SIZE}
      className="bg-white"
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
          scale={SCALE}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
        />

        {/* Zones */}
        <CanvasZones
          zones={zones}
          scale={SCALE}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
        />

        {/* Objects */}
        <CanvasObjects
          objects={objects}
          scale={SCALE}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          onObjectMoved={handleObjectMoved}
        />

        {/* Lidar visualization - uses robot pose bundled with scan for guaranteed coupling */}
        {lidarRanges.length > 0 && (
          <CanvasLidar
            robotX={lidarRobotX}
            robotY={lidarRobotY}
            robotTheta={lidarRobotTheta}
            ranges={lidarRanges}
            angleMin={lidarAngleMin}
            angleMax={lidarAngleMax}
            scale={SCALE}
            canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          />
        )}

        {/* Robots */}
        <CanvasRobots
          robots={robots}
          selectedRobotId={selectedRobotId}
          onRobotSelect={setSelectedRobotId}
          scale={SCALE}
          canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
        />
      </Layer>
    </Stage>
  )
}

// Export memoized component to prevent parent re-renders from affecting Canvas
export const Canvas = memo(CanvasInner)
