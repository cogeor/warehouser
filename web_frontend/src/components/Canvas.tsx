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
  CanvasTrajectory,
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
  // Trajectory trace state
  const traceEnabled = useAppStore((s) => s.traceEnabled)
  const trajectoryHistory = useAppStore(useShallow((s) => s.trajectoryHistory))


  // Memoize filtered entities - WorldState is source of truth for robot positions
  // Lidar visualization uses bundled pose from LidarDebug message (already correct coupling)
  const robots = useMemo(
    () => entities.filter((e) => e.type === 'robot'),
    [entities]
  )
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

        {/* Trajectory trace - rendered between zones and objects */}
        {traceEnabled && trajectoryHistory.length >= 2 && (
          <CanvasTrajectory
            points={trajectoryHistory}
            scale={SCALE}
            canvasSize={CANVAS_CONFIG.CANVAS_SIZE}
          />
        )}

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
