import { useRef, useEffect, useCallback } from 'react'
import { Stage, Layer, Rect, Circle, Arrow, Line, Image, Group } from 'react-konva'
import Konva from 'konva'
import { useAppStore } from '../store/appStore'
import { publishMoveEntity } from '../ros/connection'
import { ROBOT_SPRITE, CRATE_SPRITES, FLOOR_TILE, ZONE_MARKER, WALL_TEXTURE } from '../assets/sprites'
import { useSprite, useSprites } from '../hooks/useSprite'

const WORLD_SIZE = 10
const CANVAS_SIZE = 600
const SCALE = CANVAS_SIZE / WORLD_SIZE

// Sprite sizes (in pixels)
const ROBOT_SIZE = 40
const FLOOR_TILE_SIZE = 60

// Animation settings
const ANIMATION_DURATION = 0.08 // seconds, slightly less than 50Hz update interval
const ANIMATION_EASING = Konva.Easings.EaseOut

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

  // Load sprites
  const robotImage = useSprite(ROBOT_SPRITE)
  const floorImage = useSprite(FLOOR_TILE)
  const zoneImage = useSprite(ZONE_MARKER)
  const wallImage = useSprite(WALL_TEXTURE)
  const crateImages = useSprites(CRATE_SPRITES)

  // Refs for animated entities
  const robotRef = useRef<Konva.Image>(null)
  const robotCarryIndicatorRef = useRef<Konva.Circle>(null)
  const robotFallbackRef = useRef<Konva.Circle>(null)
  const robotArrowRef = useRef<Konva.Arrow>(null)
  const objectRefs = useRef<Map<string, Konva.Image | Konva.Circle>>(new Map())
  const lidarCenterRef = useRef<Konva.Circle>(null)

  // Track if this is the first render for each entity (skip animation on initial position)
  const robotInitialized = useRef(false)
  const objectsInitialized = useRef<Set<string>>(new Set())

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

  // Convert radians to degrees for Konva rotation
  // ROS theta: 0 = facing +X, counter-clockwise positive
  // Canvas: Y is flipped, so we need to negate and adjust
  const thetaToDegrees = (theta: number): number => {
    // In canvas coords (Y-down), we negate theta and convert to degrees
    // Also offset by -90 since sprite faces "up" (forward) by default
    return (-theta * 180) / Math.PI - 90
  }

  // Helper to convert canvas X coordinate
  const toCanvasX = (x: number): number => x * SCALE
  const toCanvasY = (y: number): number => CANVAS_SIZE - y * SCALE

  // Robot animation effect
  useEffect(() => {
    if (!robot) {
      robotInitialized.current = false
      return
    }

    const targetX = toCanvasX(robot.x)
    const targetY = toCanvasY(robot.y)
    const targetRotation = thetaToDegrees(robot.theta ?? 0)

    // Skip animation on first render - position immediately
    if (!robotInitialized.current) {
      robotInitialized.current = true
      // Set initial positions directly without animation
      if (robotRef.current) {
        robotRef.current.x(targetX)
        robotRef.current.y(targetY)
        robotRef.current.rotation(targetRotation)
      }
      if (robotCarryIndicatorRef.current) {
        robotCarryIndicatorRef.current.x(targetX)
        robotCarryIndicatorRef.current.y(targetY)
      }
      if (robotFallbackRef.current) {
        robotFallbackRef.current.x(targetX)
        robotFallbackRef.current.y(targetY)
      }
      if (robotArrowRef.current) {
        robotArrowRef.current.x(targetX)
        robotArrowRef.current.y(targetY)
      }
      if (lidarCenterRef.current) {
        lidarCenterRef.current.x(targetX)
        lidarCenterRef.current.y(targetY)
      }
      return
    }

    // Animate robot sprite
    if (robotRef.current) {
      robotRef.current.to({
        x: targetX,
        y: targetY,
        rotation: targetRotation,
        duration: ANIMATION_DURATION,
        easing: ANIMATION_EASING,
      })
    }

    // Animate carrying indicator
    if (robotCarryIndicatorRef.current) {
      robotCarryIndicatorRef.current.to({
        x: targetX,
        y: targetY,
        duration: ANIMATION_DURATION,
        easing: ANIMATION_EASING,
      })
    }

    // Animate fallback robot circle
    if (robotFallbackRef.current) {
      robotFallbackRef.current.to({
        x: targetX,
        y: targetY,
        duration: ANIMATION_DURATION,
        easing: ANIMATION_EASING,
      })
    }

    // Animate fallback direction arrow
    if (robotArrowRef.current) {
      robotArrowRef.current.to({
        x: targetX,
        y: targetY,
        duration: ANIMATION_DURATION,
        easing: ANIMATION_EASING,
      })
    }

    // Animate lidar center glow
    if (lidarCenterRef.current) {
      lidarCenterRef.current.to({
        x: targetX,
        y: targetY,
        duration: ANIMATION_DURATION,
        easing: ANIMATION_EASING,
      })
    }
  }, [robot?.x, robot?.y, robot?.theta])

  // Object animation effect - animate non-carried objects
  useEffect(() => {
    objects.forEach((obj) => {
      const ref = objectRefs.current.get(obj.id)
      if (!ref) return

      const targetX = toCanvasX(obj.x)
      const targetY = toCanvasY(obj.y)

      // Skip animation on first render for this object
      if (!objectsInitialized.current.has(obj.id)) {
        objectsInitialized.current.add(obj.id)
        ref.x(targetX)
        ref.y(targetY)
        return
      }

      // Animate object position
      ref.to({
        x: targetX,
        y: targetY,
        duration: ANIMATION_DURATION,
        easing: ANIMATION_EASING,
      })
    })

    // Clean up refs for removed objects
    const currentIds = new Set(objects.map((o) => o.id))
    objectRefs.current.forEach((_, id) => {
      if (!currentIds.has(id)) {
        objectRefs.current.delete(id)
        objectsInitialized.current.delete(id)
      }
    })
  }, [objects])

  // Callback to set object ref
  const setObjectRef = useCallback((id: string, node: Konva.Image | Konva.Circle | null) => {
    if (node) {
      objectRefs.current.set(id, node)
    }
  }, [])

  return (
    <Stage width={CANVAS_SIZE} height={CANVAS_SIZE} className="border border-gray-600 bg-gray-900">
      <Layer>
        {/* Floor tiles */}
        {floorImage && Array.from({ length: Math.ceil(CANVAS_SIZE / FLOOR_TILE_SIZE) }).map((_, row) =>
          Array.from({ length: Math.ceil(CANVAS_SIZE / FLOOR_TILE_SIZE) }).map((_, col) => (
            <Image
              key={`floor-${row}-${col}`}
              image={floorImage}
              x={col * FLOOR_TILE_SIZE}
              y={row * FLOOR_TILE_SIZE}
              width={FLOOR_TILE_SIZE}
              height={FLOOR_TILE_SIZE}
            />
          ))
        )}
        {/* Fallback grid when floor not loaded */}
        {!floorImage && Array.from({ length: WORLD_SIZE + 1 }).map((_, i) => (
          <Line
            key={`grid-v-${i}`}
            points={[i * SCALE, 0, i * SCALE, CANVAS_SIZE]}
            stroke="#333"
            strokeWidth={1}
          />
        ))}
        {!floorImage && Array.from({ length: WORLD_SIZE + 1 }).map((_, i) => (
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
          const wallWidth = (wall.width || 0.1) * SCALE
          const wallHeight = (wall.height || 0.1) * SCALE
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

        {/* Zones */}
        {zones.map((zone) => {
          const [cx, cy] = toCanvas(zone.x, zone.y)
          const zoneRadius = 0.5 * SCALE
          const zoneDisplaySize = zoneRadius * 2
          return zoneImage ? (
            <Image
              key={zone.id}
              image={zoneImage}
              x={cx}
              y={cy}
              width={zoneDisplaySize}
              height={zoneDisplaySize}
              offsetX={zoneDisplaySize / 2}
              offsetY={zoneDisplaySize / 2}
            />
          ) : (
            <Circle
              key={zone.id}
              x={cx}
              y={cy}
              radius={zoneRadius}
              fill="rgba(100, 200, 100, 0.3)"
              stroke="#4ade80"
              strokeWidth={2}
            />
          )
        })}

        {/* Objects */}
        {objects.map((obj) => {
          const [cx, cy] = toCanvas(obj.x, obj.y)
          const objSize = 0.5 * SCALE // Size in canvas pixels
          const crateImage = crateImages[obj.color || 'red']
          return crateImage ? (
            <Image
              key={obj.id}
              ref={(node) => setObjectRef(obj.id, node)}
              image={crateImage}
              x={cx}
              y={cy}
              width={objSize}
              height={objSize}
              offsetX={objSize / 2}
              offsetY={objSize / 2}
              draggable
              onDragEnd={(e) => {
                const [wx, wy] = toWorld(e.target.x(), e.target.y())
                publishMoveEntity(obj.id, wx, wy)
              }}
            />
          ) : (
            <Circle
              key={obj.id}
              ref={(node) => setObjectRef(obj.id, node)}
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

        {/* Lidar rays - enhanced sensor sweep visualization */}
        {robot && lidarRanges.length > 0 && (
          <>
            {lidarRanges.map((range, i) => {
              const numRays = lidarRanges.length
              const angle =
                robot.theta! + lidarAngleMin + (i * (lidarAngleMax - lidarAngleMin)) / (numRays - 1)
              const [rx, ry] = toCanvas(robot.x, robot.y)
              const ex = rx + Math.cos(-angle + Math.PI / 2) * range * SCALE
              const ey = ry + Math.sin(-angle + Math.PI / 2) * range * SCALE

              // Calculate distance ratio for opacity (closer = brighter endpoint)
              const maxRange = 5.0 // assumed max lidar range
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
                  <Circle
                    x={ex}
                    y={ey}
                    radius={2.5}
                    fill={`rgba(0, 255, 200, ${endpointOpacity})`}
                  />
                </Group>
              )
            })}
            {/* Center glow at robot origin */}
            <Circle
              ref={lidarCenterRef}
              x={toCanvas(robot.x, robot.y)[0]}
              y={toCanvas(robot.x, robot.y)[1]}
              radius={6}
              fill="rgba(0, 255, 200, 0.15)"
            />
          </>
        )}

        {/* Robot */}
        {robot && robotImage && (
          <>
            <Image
              ref={robotRef}
              image={robotImage}
              x={toCanvas(robot.x, robot.y)[0]}
              y={toCanvas(robot.x, robot.y)[1]}
              width={ROBOT_SIZE}
              height={ROBOT_SIZE}
              offsetX={ROBOT_SIZE / 2}
              offsetY={ROBOT_SIZE / 2}
              rotation={thetaToDegrees(robot.theta ?? 0)}
            />
            {/* Carrying indicator */}
            {robot.isCarrying && (
              <Circle
                ref={robotCarryIndicatorRef}
                x={toCanvas(robot.x, robot.y)[0]}
                y={toCanvas(robot.x, robot.y)[1]}
                radius={ROBOT_SIZE / 2 + 4}
                fill="transparent"
                stroke="#f59e0b"
                strokeWidth={3}
              />
            )}
          </>
        )}
        {/* Fallback robot rendering */}
        {robot && !robotImage && (
          <>
            <Circle
              ref={robotFallbackRef}
              x={toCanvas(robot.x, robot.y)[0]}
              y={toCanvas(robot.x, robot.y)[1]}
              radius={0.3 * SCALE}
              fill={robot.isCarrying ? '#f59e0b' : '#60a5fa'}
              stroke="#fff"
              strokeWidth={2}
            />
            {/* Direction arrow */}
            <Arrow
              ref={robotArrowRef}
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
