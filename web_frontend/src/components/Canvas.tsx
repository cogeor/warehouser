import { Stage, Layer, Rect, Circle, Arrow, Line } from 'react-konva'
import { useAppStore } from '../store/appStore'
import { publishMoveEntity } from '../ros/connection'

const WORLD_SIZE = 10
const CANVAS_SIZE = 600
const SCALE = CANVAS_SIZE / WORLD_SIZE

function toCanvas(x: number, y: number): [number, number] {
  return [x * SCALE, CANVAS_SIZE - y * SCALE]
}

function toWorld(cx: number, cy: number): [number, number] {
  return [cx / SCALE, (CANVAS_SIZE - cy) / SCALE]
}

export function Canvas() {
  const entities = useAppStore((s) => s.entities)
  const lidarRanges = useAppStore((s) => s.lidarRanges)
  const lidarAngleMin = useAppStore((s) => s.lidarAngleMin)
  const lidarAngleMax = useAppStore((s) => s.lidarAngleMax)

  const robot = entities.find((e) => e.type === 'robot')
  const objects = entities.filter((e) => e.type === 'object')
  const walls = entities.filter((e) => e.type === 'wall')
  const zones = entities.filter((e) => e.type === 'zone')

  const colorMap: Record<string, string> = {
    red: '#ef4444',
    green: '#22c55e',
    blue: '#3b82f6',
    yellow: '#eab308',
  }

  return (
    <Stage width={CANVAS_SIZE} height={CANVAS_SIZE} className="border border-gray-600 bg-gray-900">
      <Layer>
        {/* Grid */}
        {Array.from({ length: WORLD_SIZE + 1 }).map((_, i) => (
          <Line
            key={`grid-v-${i}`}
            points={[i * SCALE, 0, i * SCALE, CANVAS_SIZE]}
            stroke="#333"
            strokeWidth={1}
          />
        ))}
        {Array.from({ length: WORLD_SIZE + 1 }).map((_, i) => (
          <Line
            key={`grid-h-${i}`}
            points={[0, i * SCALE, CANVAS_SIZE, i * SCALE]}
            stroke="#333"
            strokeWidth={1}
          />
        ))}

        {/* Walls */}
        {walls.map((wall) => {
          const [cx, cy] = toCanvas(wall.x, wall.y + (wall.height || 0.1))
          return (
            <Rect
              key={wall.id}
              x={cx}
              y={cy}
              width={(wall.width || 0.1) * SCALE}
              height={(wall.height || 0.1) * SCALE}
              fill="#666"
            />
          )
        })}

        {/* Zones */}
        {zones.map((zone) => {
          const [cx, cy] = toCanvas(zone.x, zone.y)
          return (
            <Circle
              key={zone.id}
              x={cx}
              y={cy}
              radius={0.5 * SCALE}
              fill="rgba(100, 200, 100, 0.3)"
              stroke="#4ade80"
              strokeWidth={2}
            />
          )
        })}

        {/* Objects */}
        {objects.map((obj) => {
          const [cx, cy] = toCanvas(obj.x, obj.y)
          return (
            <Circle
              key={obj.id}
              x={cx}
              y={cy}
              radius={0.25 * SCALE}
              fill={colorMap[obj.color || 'red'] || '#888'}
              stroke="#fff"
              strokeWidth={2}
              draggable
              onDragEnd={(e) => {
                const [wx, wy] = toWorld(e.target.x(), e.target.y())
                publishMoveEntity(obj.id, wx, wy)
              }}
            />
          )
        })}

        {/* Lidar rays */}
        {robot && lidarRanges.length > 0 && (
          <>
            {lidarRanges.map((range, i) => {
              const numRays = lidarRanges.length
              const angle =
                robot.theta! + lidarAngleMin + (i * (lidarAngleMax - lidarAngleMin)) / (numRays - 1)
              const [rx, ry] = toCanvas(robot.x, robot.y)
              const ex = rx + Math.cos(-angle + Math.PI / 2) * range * SCALE
              const ey = ry + Math.sin(-angle + Math.PI / 2) * range * SCALE
              return (
                <Line
                  key={`lidar-${i}`}
                  points={[rx, ry, ex, ey]}
                  stroke="rgba(0, 255, 0, 0.3)"
                  strokeWidth={1}
                />
              )
            })}
          </>
        )}

        {/* Robot */}
        {robot && (
          <>
            <Circle
              x={toCanvas(robot.x, robot.y)[0]}
              y={toCanvas(robot.x, robot.y)[1]}
              radius={0.3 * SCALE}
              fill={robot.isCarrying ? '#f59e0b' : '#60a5fa'}
              stroke="#fff"
              strokeWidth={2}
            />
            {/* Direction arrow */}
            <Arrow
              x={toCanvas(robot.x, robot.y)[0]}
              y={toCanvas(robot.x, robot.y)[1]}
              points={[0, 0, Math.cos(-robot.theta! + Math.PI / 2) * 0.4 * SCALE, Math.sin(-robot.theta! + Math.PI / 2) * 0.4 * SCALE]}
              pointerLength={8}
              pointerWidth={6}
              fill="#fff"
              stroke="#fff"
              strokeWidth={2}
            />
          </>
        )}
      </Layer>
    </Stage>
  )
}
