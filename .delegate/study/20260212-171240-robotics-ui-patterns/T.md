# Template Analysis: Robotics UI Patterns

Created: 2026-02-12T17:45:00Z

## Executive Summary

Analysis of production robotics visualization platforms (Foxglove Studio, RViz2, Robot Web Tools) reveals architectural patterns directly applicable to Warehouser's web_frontend. Current implementation uses roslibjs + Zustand + Konva/React, which aligns well with industry practices. Key improvements identified: enhanced panel system, message type safety, connection resilience patterns, and performance optimization strategies.

---

## Source Projects Analyzed

### 1. Foxglove Studio
- **Repository:** https://github.com/foxglove/studio
- **Tech Stack:** TypeScript, React, WebSocket (Foxglove Bridge)
- **Architecture:** Modular panel system with extension SDK
- **License:** Mozilla Public License v2.0

### 2. RViz2
- **Repository:** https://github.com/ros2/rviz
- **Tech Stack:** C++/Qt, Ogre3D, pluginlib
- **Architecture:** Plugin-based displays, panels, tools, view controllers
- **License:** BSD

### 3. Robot Web Tools
- **Repository:** https://github.com/RobotWebTools
- **Key Libraries:** roslibjs, ros3djs, nav2djs
- **Protocol:** rosbridge_suite (JSON over WebSocket)
- **License:** BSD

---

## Pattern 1: Type-Safe Message Interfaces

### Current Warehouser Implementation

```typescript
// connection.ts - Type assertions on unknown
worldStateTopic.subscribe((msg: unknown) => {
  const message = msg as { entities: unknown[]; sim_time: number }
  // Manual mapping...
})
```

### Industry Pattern (Foxglove/RViz2)

**Generate TypeScript types from ROS message definitions:**

```typescript
// types/warehouser_msgs.ts
// AUTO-GENERATED - DO NOT EDIT
// Generated from warehouser_msgs package

import { Header, Pose, Twist } from './std_msgs'

export interface EntityInfo {
  id: string
  type: EntityType
  x: number // float32
  y: number // float32
  theta?: number // float32
  color?: string
  width?: number // float32
  height?: number // float32
  is_carrying?: boolean
  carried_id?: string
}

export enum EntityType {
  ROBOT = 0,
  OBJECT = 1,
  WALL = 2,
  ZONE = 3,
}

export interface WorldState {
  header: Header
  entities: EntityInfo[]
  sim_time: number // float64
}

export interface LidarDebug {
  header: Header
  ranges: number[] // float32[]
  angle_min: number // float32
  angle_max: number // float32
}

export interface TaskStatus {
  header: Header
  state: TaskState
  intent: string
}

export enum TaskState {
  IDLE = 0,
  PICKING = 1,
  CARRYING = 2,
  DELIVERING = 3,
}

export interface RLStep {
  header: Header
  action: number[] // float32[]
}

export interface RLStepResponse {
  observation: number[] // float32[]
  reward: number // float32
  done: boolean
  truncated: boolean
  info: string // JSON-encoded dict
}
```

### Recommended Tool

```typescript
// scripts/generate-types.ts
import * as fs from 'fs'
import * as path from 'path'

/**
 * Parse .msg files and generate TypeScript interfaces
 * Run: npm run generate-types
 */
interface MessageField {
  type: string
  name: string
  isArray: boolean
}

function parseMessageFile(msgPath: string): MessageField[] {
  const content = fs.readFileSync(msgPath, 'utf-8')
  const fields: MessageField[] = []

  for (const line of content.split('\n')) {
    const trimmed = line.trim()
    if (!trimmed || trimmed.startsWith('#')) continue

    const parts = trimmed.split(/\s+/)
    if (parts.length < 2) continue

    const [type, name] = parts
    fields.push({
      type: type.replace('[]', ''),
      name,
      isArray: type.includes('[]'),
    })
  }

  return fields
}

function rosTypeToTS(rosType: string): string {
  const typeMap: Record<string, string> = {
    'bool': 'boolean',
    'int8': 'number',
    'uint8': 'number',
    'int16': 'number',
    'uint16': 'number',
    'int32': 'number',
    'uint32': 'number',
    'int64': 'number',
    'uint64': 'number',
    'float32': 'number',
    'float64': 'number',
    'string': 'string',
  }

  return typeMap[rosType] || rosType
}

function generateInterface(msgName: string, fields: MessageField[]): string {
  let output = `export interface ${msgName} {\n`

  for (const field of fields) {
    const tsType = rosTypeToTS(field.type)
    const arrayNotation = field.isArray ? '[]' : ''
    output += `  ${field.name}: ${tsType}${arrayNotation}\n`
  }

  output += '}\n'
  return output
}

// Usage:
// const fields = parseMessageFile('ros_ws/src/warehouser_msgs/msg/WorldState.msg')
// const code = generateInterface('WorldState', fields)
```

### Application to Warehouser

**Benefits:**
- Compile-time type checking for ROS messages
- IntelliSense support in IDE
- Eliminates manual type assertions
- Catches message format changes early

**Integration:**
```typescript
// ros/connection.ts - IMPROVED VERSION
import { WorldState, LidarDebug, TaskStatus } from '../types/warehouser_msgs'

function subscribeToTopics() {
  if (!ros) return
  const store = useAppStore.getState()

  // Type-safe subscription
  const worldStateTopic = new ROSLIB.Topic<WorldState>({
    ros,
    name: '/world/state',
    messageType: 'warehouser_msgs/WorldState',
  })

  worldStateTopic.subscribe((msg: WorldState) => {
    // TypeScript now knows exact structure - no assertions needed!
    const entities: Entity[] = msg.entities.map((e) => ({
      id: e.id,
      type: entityTypeFromEnum(e.type),
      x: e.x,
      y: e.y,
      theta: e.theta,
      color: e.color,
      width: e.width,
      height: e.height,
      isCarrying: e.is_carrying,
      carriedId: e.carried_id,
    }))
    store.setEntities(entities)
    store.setSimTime(msg.sim_time)
  })
}
```

---

## Pattern 2: Panel System Architecture (Foxglove)

### Core Concept

Each visualization is an **isolated, reusable React component** registered in a central panel registry. Panels are composable, configurable, and can be arranged dynamically.

### Panel Interface Definition

```typescript
// types/panels.ts

/**
 * Base props that every panel receives
 */
export interface PanelProps<TConfig = unknown> {
  // Panel configuration (persisted)
  config: TConfig
  onConfigChange: (config: TConfig) => void

  // Global context
  connection: RosConnection

  // Layout info
  width?: number
  height?: number
}

/**
 * Panel descriptor for registration
 */
export interface PanelDescriptor<TConfig = unknown> {
  id: string
  title: string
  description: string
  component: React.ComponentType<PanelProps<TConfig>>
  defaultConfig: TConfig

  // Optional metadata
  icon?: React.ReactNode
  category?: 'visualization' | 'control' | 'status' | 'debug'
  minWidth?: number
  minHeight?: number
}

/**
 * Panel registry - singleton pattern
 */
export class PanelRegistry {
  private static instance: PanelRegistry
  private panels = new Map<string, PanelDescriptor>()

  static getInstance(): PanelRegistry {
    if (!PanelRegistry.instance) {
      PanelRegistry.instance = new PanelRegistry()
    }
    return PanelRegistry.instance
  }

  register<TConfig>(descriptor: PanelDescriptor<TConfig>): void {
    if (this.panels.has(descriptor.id)) {
      console.warn(`Panel ${descriptor.id} already registered, overwriting`)
    }
    this.panels.set(descriptor.id, descriptor)
  }

  get(id: string): PanelDescriptor | undefined {
    return this.panels.get(id)
  }

  getAll(): PanelDescriptor[] {
    return Array.from(this.panels.values())
  }

  getByCategory(category: string): PanelDescriptor[] {
    return this.getAll().filter(p => p.category === category)
  }
}

// Export singleton instance
export const panelRegistry = PanelRegistry.getInstance()
```

### Example Panel Implementation

```typescript
// components/panels/MapPanel.tsx

interface MapPanelConfig {
  showGrid: boolean
  showRobots: boolean
  showObjects: boolean
  showLidar: boolean
  backgroundColor: string
}

export function MapPanel({ config, onConfigChange, connection }: PanelProps<MapPanelConfig>) {
  const entities = useAppStore((s) => s.entities)
  const lidarRanges = useAppStore((s) => s.lidarRanges)

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="flex gap-2 p-2 bg-gray-800 border-b border-gray-700">
        <label className="flex items-center gap-1 text-sm">
          <input
            type="checkbox"
            checked={config.showGrid}
            onChange={(e) => onConfigChange({ ...config, showGrid: e.target.checked })}
          />
          Grid
        </label>
        <label className="flex items-center gap-1 text-sm">
          <input
            type="checkbox"
            checked={config.showLidar}
            onChange={(e) => onConfigChange({ ...config, showLidar: e.target.checked })}
          />
          Lidar
        </label>
      </div>

      {/* Canvas */}
      <div className="flex-1">
        <Canvas
          showGrid={config.showGrid}
          showRobots={config.showRobots}
          showObjects={config.showObjects}
          showLidar={config.showLidar}
        />
      </div>
    </div>
  )
}

// Register panel
panelRegistry.register({
  id: 'map-2d',
  title: '2D Map',
  description: 'Top-down view of warehouse with robots and objects',
  component: MapPanel,
  category: 'visualization',
  defaultConfig: {
    showGrid: true,
    showRobots: true,
    showObjects: true,
    showLidar: true,
    backgroundColor: '#1a1a1a',
  },
  minWidth: 400,
  minHeight: 400,
})
```

### Application to Warehouser

**Refactor existing components as panels:**

```typescript
// components/panels/StatusPanel.tsx
export function StatusPanel({ config, connection }: PanelProps<StatusPanelConfig>) {
  const robot = useAppStore((s) => s.entities.find(e => e.type === 'robot'))
  const taskState = useAppStore((s) => s.taskState)

  return (
    <div className="bg-gray-800 p-4 rounded">
      <h3 className="font-bold mb-2">Robot Status</h3>
      {robot ? (
        <>
          <div>Position: ({robot.x.toFixed(2)}, {robot.y.toFixed(2)})</div>
          <div>Heading: {((robot.theta ?? 0) * 180 / Math.PI).toFixed(1)}°</div>
          <div>Task: {taskState}</div>
          <div>Carrying: {robot.isCarrying ? 'Yes' : 'No'}</div>
        </>
      ) : (
        <div className="text-gray-500">No robot detected</div>
      )}
    </div>
  )
}

panelRegistry.register({
  id: 'status',
  title: 'Robot Status',
  description: 'Current robot state and task information',
  component: StatusPanel,
  category: 'status',
  defaultConfig: {},
})
```

---

## Pattern 3: ROS Connection Management (Foxglove/roslibjs)

### Enhanced Connection Architecture

```typescript
// ros/RosConnection.ts

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error'

export interface ConnectionConfig {
  url: string
  reconnect: boolean
  maxReconnectAttempts: number
  reconnectBaseDelay: number
  reconnectMaxDelay: number
}

export class RosConnection {
  private ros: ROSLIB.Ros | null = null
  private config: ConnectionConfig
  private status: ConnectionStatus = 'disconnected'
  private reconnectAttempt = 0
  private reconnectTimeout: ReturnType<typeof setTimeout> | null = null
  private subscriptions = new Map<string, ROSLIB.Topic>()
  private listeners = new Set<(status: ConnectionStatus) => void>()

  constructor(config: Partial<ConnectionConfig> = {}) {
    this.config = {
      url: 'ws://localhost:9090',
      reconnect: true,
      maxReconnectAttempts: 10,
      reconnectBaseDelay: 1000,
      reconnectMaxDelay: 30000,
      ...config,
    }
  }

  /**
   * Connect to ROS bridge
   */
  async connect(): Promise<void> {
    if (this.ros) {
      this.ros.close()
    }

    this.setStatus('connecting')

    this.ros = new ROSLIB.Ros({ url: this.config.url })

    return new Promise((resolve, reject) => {
      this.ros!.on('connection', () => {
        console.log('Connected to ROS')
        this.setStatus('connected')
        this.reconnectAttempt = 0
        this.clearReconnectTimeout()
        resolve()
      })

      this.ros!.on('error', (error) => {
        console.error('ROS connection error:', error)
        this.setStatus('error')
        reject(error)
      })

      this.ros!.on('close', () => {
        console.log('Disconnected from ROS')
        this.setStatus('disconnected')
        this.scheduleReconnect()
      })
    })
  }

  /**
   * Disconnect from ROS bridge
   */
  disconnect(): void {
    this.config.reconnect = false // Disable auto-reconnect
    this.clearReconnectTimeout()

    // Unsubscribe all topics
    this.subscriptions.forEach(topic => topic.unsubscribe())
    this.subscriptions.clear()

    if (this.ros) {
      this.ros.close()
      this.ros = null
    }

    this.setStatus('disconnected')
  }

  /**
   * Subscribe to a ROS topic with type safety
   */
  subscribe<T>(
    topicName: string,
    messageType: string,
    callback: (message: T) => void
  ): () => void {
    if (!this.ros) {
      throw new Error('Not connected to ROS')
    }

    // Reuse existing subscription if available
    let topic = this.subscriptions.get(topicName)

    if (!topic) {
      topic = new ROSLIB.Topic({
        ros: this.ros,
        name: topicName,
        messageType,
      })
      this.subscriptions.set(topicName, topic)
    }

    topic.subscribe(callback as (message: unknown) => void)

    // Return unsubscribe function
    return () => {
      topic!.unsubscribe(callback as (message: unknown) => void)
    }
  }

  /**
   * Publish to a ROS topic
   */
  publish<T>(topicName: string, messageType: string, message: T): void {
    if (!this.ros) {
      console.warn('Cannot publish - not connected to ROS')
      return
    }

    const topic = new ROSLIB.Topic({
      ros: this.ros,
      name: topicName,
      messageType,
    })

    const rosMessage = new ROSLIB.Message(message as Record<string, unknown>)
    topic.publish(rosMessage)
  }

  /**
   * Call a ROS service
   */
  async callService<TReq, TRes>(
    serviceName: string,
    serviceType: string,
    request: TReq
  ): Promise<TRes> {
    if (!this.ros) {
      throw new Error('Not connected to ROS')
    }

    return new Promise((resolve, reject) => {
      const service = new ROSLIB.Service({
        ros: this.ros!,
        name: serviceName,
        serviceType,
      })

      const req = new ROSLIB.ServiceRequest(request as Record<string, unknown>)

      service.callService(req, (response) => {
        resolve(response as TRes)
      }, (error) => {
        reject(error)
      })
    })
  }

  /**
   * Get current connection status
   */
  getStatus(): ConnectionStatus {
    return this.status
  }

  /**
   * Listen for status changes
   */
  onStatusChange(callback: (status: ConnectionStatus) => void): () => void {
    this.listeners.add(callback)
    return () => this.listeners.delete(callback)
  }

  // Private methods

  private setStatus(status: ConnectionStatus): void {
    this.status = status
    this.listeners.forEach(listener => listener(status))
  }

  private scheduleReconnect(): void {
    if (!this.config.reconnect) return
    if (this.reconnectAttempt >= this.config.maxReconnectAttempts) {
      console.error('Max reconnection attempts reached')
      return
    }

    const delay = this.calculateBackoff()
    console.log(`Reconnecting in ${delay}ms (attempt ${this.reconnectAttempt + 1})`)

    this.clearReconnectTimeout()
    this.reconnectTimeout = setTimeout(() => {
      this.reconnectAttempt++
      this.connect().catch(err => console.error('Reconnection failed:', err))
    }, delay)
  }

  private calculateBackoff(): number {
    const { reconnectBaseDelay, reconnectMaxDelay } = this.config
    const exponentialDelay = reconnectBaseDelay * Math.pow(2, this.reconnectAttempt)
    const cappedDelay = Math.min(exponentialDelay, reconnectMaxDelay)

    // Add jitter (+/- 10%)
    const jitter = cappedDelay * 0.1 * (Math.random() * 2 - 1)
    return Math.round(cappedDelay + jitter)
  }

  private clearReconnectTimeout(): void {
    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout)
      this.reconnectTimeout = null
    }
  }
}
```

### React Hook Integration

```typescript
// hooks/useRosConnection.ts

const connectionContext = React.createContext<RosConnection | null>(null)

export function RosConnectionProvider({ children }: { children: React.ReactNode }) {
  const connectionRef = useRef<RosConnection>()

  if (!connectionRef.current) {
    connectionRef.current = new RosConnection({
      url: 'ws://localhost:9090',
    })
  }

  useEffect(() => {
    const connection = connectionRef.current!
    connection.connect().catch(err => console.error('Failed to connect:', err))

    return () => {
      connection.disconnect()
    }
  }, [])

  return (
    <connectionContext.Provider value={connectionRef.current}>
      {children}
    </connectionContext.Provider>
  )
}

export function useRosConnection(): RosConnection {
  const connection = useContext(connectionContext)
  if (!connection) {
    throw new Error('useRosConnection must be used within RosConnectionProvider')
  }
  return connection
}

export function useRosTopic<T>(
  topicName: string,
  messageType: string
): T | null {
  const [message, setMessage] = useState<T | null>(null)
  const connection = useRosConnection()

  useEffect(() => {
    const unsubscribe = connection.subscribe<T>(
      topicName,
      messageType,
      (msg) => setMessage(msg)
    )

    return unsubscribe
  }, [connection, topicName, messageType])

  return message
}
```

---

## Pattern 4: Performance Optimization (RViz2/Foxglove)

### High-Frequency Data Throttling

Current Warehouser renders at ROS topic rate (~50Hz). Industry practice: decouple data receipt from rendering.

```typescript
// hooks/useThrottledTopic.ts

/**
 * Subscribe to high-frequency topic but only update state at controlled rate
 */
export function useThrottledTopic<T>(
  topicName: string,
  messageType: string,
  fps: number = 30
): T | null {
  const [state, setState] = useState<T | null>(null)
  const latestRef = useRef<T | null>(null)
  const connection = useRosConnection()

  useEffect(() => {
    // Receive all messages but store in ref (no re-render)
    const unsubscribe = connection.subscribe<T>(
      topicName,
      messageType,
      (msg) => {
        latestRef.current = msg
      }
    )

    // Update state at controlled rate using RAF
    let rafId: number
    let lastUpdate = 0
    const frameTime = 1000 / fps

    const tick = (timestamp: number) => {
      if (timestamp - lastUpdate >= frameTime) {
        if (latestRef.current !== null) {
          setState(latestRef.current)
        }
        lastUpdate = timestamp
      }
      rafId = requestAnimationFrame(tick)
    }

    rafId = requestAnimationFrame(tick)

    return () => {
      unsubscribe()
      cancelAnimationFrame(rafId)
    }
  }, [connection, topicName, messageType, fps])

  return state
}
```

### Canvas Rendering Optimization

```typescript
// components/Canvas.tsx - OPTIMIZED VERSION

export function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const stageRef = useRef<Konva.Stage>(null)

  // Use throttled data for rendering
  const worldState = useThrottledTopic<WorldState>('/world/state', 'warehouser_msgs/WorldState', 30)
  const lidarDebug = useThrottledTopic<LidarDebug>('/observations/lidar_debug', 'warehouser_msgs/LidarDebug', 30)

  // Memoize expensive calculations
  const { robots, objects, walls, zones } = useMemo(() => {
    if (!worldState) return { robots: [], objects: [], walls: [], zones: [] }

    return {
      robots: worldState.entities.filter(e => e.type === EntityType.ROBOT),
      objects: worldState.entities.filter(e => e.type === EntityType.OBJECT),
      walls: worldState.entities.filter(e => e.type === EntityType.WALL),
      zones: worldState.entities.filter(e => e.type === EntityType.ZONE),
    }
  }, [worldState])

  // Use React Konva's built-in optimization
  return (
    <Stage ref={stageRef} width={600} height={600}>
      <Layer>
        {/* Static elements - don't re-render unless changed */}
        <FloorGrid />
        <WallsLayer walls={walls} />
        <ZonesLayer zones={zones} />

        {/* Dynamic elements - re-render each frame */}
        <ObjectsLayer objects={objects} />
        <RobotsLayer robots={robots} />
        {lidarDebug && <LidarLayer lidar={lidarDebug} robot={robots[0]} />}
      </Layer>
    </Stage>
  )
}

// Memoized sub-layers to prevent unnecessary re-renders
const WallsLayer = React.memo(({ walls }: { walls: EntityInfo[] }) => {
  return (
    <>
      {walls.map(wall => (
        <Rect key={wall.id} /* ... wall rendering ... */ />
      ))}
    </>
  )
})
```

---

## Pattern 5: Multi-Robot Support (Fleet Visualization)

### Scalability Pattern from Open-RMF

```typescript
// store/fleetStore.ts

export interface RobotInfo {
  id: string
  name: string
  pose: Pose
  velocity: Twist
  batteryLevel: number
  taskState: TaskState
  isCarrying: boolean
  carriedObjectId?: string
  lastUpdate: number // timestamp
}

export interface FleetState {
  robots: Map<string, RobotInfo>
  selectedRobotId: string | null
  viewMode: 'fleet' | 'individual'
}

export const useFleetStore = create<FleetState>((set) => ({
  robots: new Map(),
  selectedRobotId: null,
  viewMode: 'fleet',

  updateRobot: (id: string, updates: Partial<RobotInfo>) => set((state) => {
    const robots = new Map(state.robots)
    const existing = robots.get(id)
    robots.set(id, {
      ...existing,
      ...updates,
      id,
      lastUpdate: Date.now(),
    } as RobotInfo)
    return { robots }
  }),

  removeRobot: (id: string) => set((state) => {
    const robots = new Map(state.robots)
    robots.delete(id)
    return { robots }
  }),

  selectRobot: (id: string | null) => set({ selectedRobotId: id }),
  setViewMode: (mode: 'fleet' | 'individual') => set({ viewMode: mode }),
}))
```

### Multi-Robot Subscription Pattern

```typescript
// ros/subscriptions.ts

export function subscribeToFleet(connection: RosConnection) {
  const store = useFleetStore.getState()

  // Subscribe to robot list topic
  connection.subscribe<RobotList>('/fleet/robots', 'warehouser_msgs/RobotList', (msg) => {
    // Update robots map
    const activeIds = new Set(msg.robot_ids)

    // Remove robots no longer in list
    store.robots.forEach((_, id) => {
      if (!activeIds.has(id)) {
        store.removeRobot(id)
      }
    })
  })

  // Subscribe to each robot's state topic (namespaced)
  // Pattern: /robot_{id}/state
  connection.subscribe<RobotState>('/+/state', 'warehouser_msgs/RobotState', (msg) => {
    store.updateRobot(msg.robot_id, {
      name: msg.robot_id,
      pose: msg.pose,
      velocity: msg.velocity,
      batteryLevel: msg.battery_level,
      taskState: msg.task_state,
      isCarrying: msg.carrying_object,
      carriedObjectId: msg.carried_object_id,
    })
  })
}
```

---

## Pattern 6: Coordinate System Management (REP 103)

### Transform Utilities (RViz2 Pattern)

```typescript
// utils/transforms.ts

/**
 * REP 103 Coordinate Frames
 * ROS: X=forward, Y=left, Z=up, theta=CCW from X
 * Canvas: X=right, Y=down, origin=top-left
 */

export interface Transform2D {
  x: number
  y: number
  theta: number
}

export class CoordinateTransform {
  constructor(
    private worldSize: number,
    private canvasSize: number
  ) {}

  get scale(): number {
    return this.canvasSize / this.worldSize
  }

  /**
   * Convert ROS coordinates to canvas coordinates
   */
  worldToCanvas(x: number, y: number): [number, number] {
    const cx = x * this.scale
    const cy = this.canvasSize - y * this.scale
    return [cx, cy]
  }

  /**
   * Convert canvas coordinates to ROS coordinates
   */
  canvasToWorld(cx: number, cy: number): [number, number] {
    const x = cx / this.scale
    const y = (this.canvasSize - cy) / this.scale
    return [x, y]
  }

  /**
   * Convert ROS theta (radians, CCW from +X) to canvas rotation (degrees, CW from +Y)
   */
  worldThetaToCanvasRotation(theta: number): number {
    // Canvas Y is flipped, so negate theta
    // Canvas rotation is in degrees
    // Offset by -90 since sprite "forward" is up (+Y) but ROS forward is right (+X)
    return (-theta * 180 / Math.PI) - 90
  }

  /**
   * Convert canvas rotation back to ROS theta
   */
  canvasRotationToWorldTheta(rotation: number): number {
    return -(rotation + 90) * Math.PI / 180
  }

  /**
   * Transform a point by a pose (composition)
   */
  transformPoint(point: [number, number], pose: Transform2D): [number, number] {
    const [px, py] = point
    const { x, y, theta } = pose

    // Rotate then translate
    const cos = Math.cos(theta)
    const sin = Math.sin(theta)

    return [
      x + px * cos - py * sin,
      y + px * sin + py * cos,
    ]
  }
}

// Singleton instance
export const transform = new CoordinateTransform(10, 600)
```

---

## Pattern 7: Extension/Plugin System (Foxglove Extensions SDK)

### Plugin Architecture

```typescript
// plugins/types.ts

export interface PluginManifest {
  id: string
  name: string
  version: string
  description: string
  author: string

  // What this plugin provides
  panels?: PanelDescriptor[]
  messageConverters?: MessageConverterDescriptor[]
  themes?: ThemeDescriptor[]
}

export interface PluginContext {
  connection: RosConnection
  registry: PanelRegistry
  store: ReturnType<typeof useAppStore>
}

export interface Plugin {
  manifest: PluginManifest
  activate(context: PluginContext): void
  deactivate(): void
}

// Example plugin
export class CustomVisualizationPlugin implements Plugin {
  manifest: PluginManifest = {
    id: 'custom-viz',
    name: 'Custom Visualization',
    version: '1.0.0',
    description: 'Custom panels for Warehouser',
    author: 'Your Name',
    panels: [
      // ... panel descriptors
    ],
  }

  activate(context: PluginContext): void {
    // Register panels
    this.manifest.panels?.forEach(panel => {
      context.registry.register(panel)
    })
  }

  deactivate(): void {
    // Cleanup
  }
}
```

---

## Actionable Recommendations for Warehouser

### Immediate (Next Sprint)

1. **Generate TypeScript types from .msg files**
   - Create `scripts/generate-types.ts`
   - Add types to `src/types/warehouser_msgs.ts`
   - Eliminate type assertions in `connection.ts`

2. **Refactor ROS connection into class**
   - Implement `RosConnection` class with proper lifecycle
   - Add React context provider
   - Create `useRosTopic` hook

3. **Add throttled topic subscription**
   - Implement `useThrottledTopic` hook
   - Apply to world state and lidar topics
   - Target 30 FPS rendering

### Medium-Term (This Month)

4. **Implement panel system**
   - Create `PanelRegistry` class
   - Refactor existing components to use `PanelProps`
   - Add panel configuration persistence (localStorage)

5. **Add coordinate transform utility**
   - Create `CoordinateTransform` class
   - Replace inline transform logic in Canvas
   - Document coordinate systems in README

6. **Enhance multi-robot support**
   - Update Zustand store to use `Map<string, RobotInfo>`
   - Subscribe to namespaced robot topics
   - Add fleet view mode

### Long-Term (Future)

7. **Build plugin system**
   - Design plugin API
   - Create example plugins
   - Add plugin manager UI

8. **Add layout persistence**
   - Save panel arrangement to localStorage
   - Support multiple layouts
   - Add layout import/export

---

## Code Snippets Ready for Copy-Paste

### 1. Type-Safe Topic Subscription Hook

```typescript
// hooks/useRosTopic.ts
import { useEffect, useState } from 'react'
import { useRosConnection } from './useRosConnection'

export function useRosTopic<T>(
  topicName: string,
  messageType: string
): T | null {
  const [message, setMessage] = useState<T | null>(null)
  const connection = useRosConnection()

  useEffect(() => {
    if (!connection) return

    const unsubscribe = connection.subscribe<T>(
      topicName,
      messageType,
      (msg) => setMessage(msg)
    )

    return unsubscribe
  }, [connection, topicName, messageType])

  return message
}
```

### 2. Connection Status Component

```typescript
// components/ConnectionStatus.tsx
import { useEffect, useState } from 'react'
import { useRosConnection } from '../hooks/useRosConnection'
import type { ConnectionStatus } from '../ros/RosConnection'

export function ConnectionStatus() {
  const connection = useRosConnection()
  const [status, setStatus] = useState<ConnectionStatus>('disconnected')

  useEffect(() => {
    const unsubscribe = connection.onStatusChange(setStatus)
    setStatus(connection.getStatus())
    return unsubscribe
  }, [connection])

  const statusColors: Record<ConnectionStatus, string> = {
    disconnected: 'text-gray-500',
    connecting: 'text-yellow-500',
    connected: 'text-green-500',
    error: 'text-red-500',
  }

  const statusIcons: Record<ConnectionStatus, string> = {
    disconnected: '○',
    connecting: '◐',
    connected: '●',
    error: '✕',
  }

  return (
    <div className={`flex items-center gap-2 ${statusColors[status]}`}>
      <span>{statusIcons[status]}</span>
      <span className="capitalize">{status}</span>
    </div>
  )
}
```

### 3. Improved World State Subscription

```typescript
// ros/subscriptions.ts (replaces subscribeToTopics)
import { WorldState, LidarDebug, TaskStatus } from '../types/warehouser_msgs'
import { RosConnection } from './RosConnection'
import { useAppStore } from '../store/appStore'

export function subscribeToTopics(connection: RosConnection) {
  const store = useAppStore.getState()

  // World state
  connection.subscribe<WorldState>(
    '/world/state',
    'warehouser_msgs/WorldState',
    (msg) => {
      const entities = msg.entities.map(e => ({
        id: e.id,
        type: entityTypeFromEnum(e.type),
        x: e.x,
        y: e.y,
        theta: e.theta,
        color: e.color,
        width: e.width,
        height: e.height,
        isCarrying: e.is_carrying,
        carriedId: e.carried_id,
      }))
      store.setEntities(entities)
      store.setSimTime(msg.sim_time)
    }
  )

  // Lidar debug
  connection.subscribe<LidarDebug>(
    '/observations/lidar_debug',
    'warehouser_msgs/LidarDebug',
    (msg) => {
      store.setLidar(msg.ranges, msg.angle_min, msg.angle_max)
    }
  )

  // Task status
  connection.subscribe<TaskStatus>(
    '/task/status',
    'warehouser_msgs/TaskStatus',
    (msg) => {
      store.setTaskStatus(msg.state.toString(), msg.intent)
    }
  )
}

function entityTypeFromEnum(type: number): 'robot' | 'object' | 'wall' | 'zone' {
  const map: Record<number, 'robot' | 'object' | 'wall' | 'zone'> = {
    0: 'robot',
    1: 'object',
    2: 'wall',
    3: 'zone',
  }
  return map[type] || 'object'
}
```

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Warehouser Frontend                       │
└─────────────────────────────────────────────────────────────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
        ┌───────────▼─────────┐   ┌──────────▼──────────┐
        │   React Components   │   │   Zustand Stores    │
        │                      │   │                     │
        │  - MapPanel          │   │  - appStore         │
        │  - StatusPanel       │   │  - fleetStore       │
        │  - ControlPanel      │   │  - configStore      │
        │  - ObjectivePanel    │   │                     │
        └───────────┬──────────┘   └──────────┬──────────┘
                    │                         │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   Custom Hooks          │
                    │                         │
                    │  - useRosTopic()        │
                    │  - useThrottledTopic()  │
                    │  - useRosConnection()   │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   RosConnection Class   │
                    │                         │
                    │  - subscribe()          │
                    │  - publish()            │
                    │  - callService()        │
                    │  - reconnection logic   │
                    └────────────┬────────────┘
                                 │
                                 │ WebSocket (JSON)
                                 │
                    ┌────────────▼────────────┐
                    │   rosbridge_suite       │
                    │   OR                    │
                    │   foxglove_bridge       │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   ROS2 Backend          │
                    │                         │
                    │  - ros_simulation       │
                    │  - ros_observations     │
                    │  - ros_rl_bridge        │
                    │  - task_manager         │
                    └─────────────────────────┘
```

---

## Key Takeaways

### What Warehouser Already Does Well

1. Uses Zustand for state management (industry standard)
2. Konva for canvas rendering (good choice for 2D)
3. roslibjs for ROS communication (standard library)
4. Modular component structure
5. TypeScript throughout (type safety)

### What Can Be Improved

1. **Type Safety:** Generate types from .msg files instead of type assertions
2. **Connection Management:** Encapsulate ROS connection in a class with proper lifecycle
3. **Performance:** Throttle high-frequency topics to match render rate
4. **Modularity:** Implement panel system for better component composition
5. **Scalability:** Prepare for multi-robot scenarios with fleet-centric store design

### Industry Best Practices Applied

- **Foxglove:** Panel system, configuration management, extension architecture
- **RViz2:** Plugin pattern, display/panel separation, coordinate transforms
- **Robot Web Tools:** Connection resilience, topic subscription patterns

---

## Next Steps

1. **Review this document** with the team
2. **Prioritize recommendations** based on current sprint goals
3. **Create implementation tickets** for high-priority items
4. **Start with type generation** - foundational improvement with immediate benefits
5. **Iterate on panel system** - enables future extensibility

---

## References

### Documentation
- [Foxglove Studio GitHub](https://github.com/foxglove/studio)
- [Foxglove Extensions SDK](https://foxglove.dev/blog/building-a-custom-react-panel-with-foxglove-studio-extensions)
- [RViz2 Plugin Development](https://github.com/ros2/rviz/blob/rolling/docs/plugin_development.md)
- [rosbridge_suite Documentation](https://github.com/RobotWebTools/rosbridge_suite)
- [roslibjs API Reference](http://robotwebtools.org/jsdoc/roslibjs/current/)
- [REP 103: Standard Units of Measure and Coordinate Conventions](https://www.ros.org/reps/rep-0103.html)

### Comparison Articles
- [Comparing RViz, Foxglove, and Rerun](https://www.reduct.store/blog/comparison-rviz-foxglove-rerun)
- [Using Rosbridge with ROS 2](https://foxglove.dev/blog/using-rosbridge-with-ros2)

### Example Projects
- [my_rviz2_plugin - Custom RViz2 Panel Example](https://github.com/githir/my_rviz2_plugin)
- [Foxglove Studio Source Code](https://github.com/foxglove/studio/tree/main/packages)

---

## Appendix: Message Type Generation Script

```bash
#!/usr/bin/env bash
# scripts/generate-message-types.sh

set -e

MSG_DIR="ros_ws/src/warehouser_msgs/msg"
OUTPUT_FILE="web_frontend/src/types/warehouser_msgs.ts"

echo "// AUTO-GENERATED - DO NOT EDIT" > "$OUTPUT_FILE"
echo "// Generated from warehouser_msgs package" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "import { Header } from './std_msgs'" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

for msg_file in "$MSG_DIR"/*.msg; do
  msg_name=$(basename "$msg_file" .msg)

  echo "Generating interface for $msg_name..."

  # Parse .msg file and generate TypeScript interface
  # (Simplified - real implementation would be more robust)
  echo "export interface $msg_name {" >> "$OUTPUT_FILE"

  while IFS= read -r line; do
    # Skip comments and empty lines
    if [[ "$line" =~ ^# ]] || [[ -z "$line" ]]; then
      continue
    fi

    # Parse field: "type name"
    read -r type name <<< "$line"

    # Convert ROS type to TypeScript type
    case "$type" in
      bool) ts_type="boolean" ;;
      int8|uint8|int16|uint16|int32|uint32|int64|uint64|float32|float64) ts_type="number" ;;
      string) ts_type="string" ;;
      *\[\]) ts_type="${type%\[\]}[]"; ts_type="${ts_type/int*/number}"; ts_type="${ts_type/uint*/number}"; ts_type="${ts_type/float*/number}" ;;
      *) ts_type="$type" ;;
    esac

    echo "  $name: $ts_type" >> "$OUTPUT_FILE"
  done < "$msg_file"

  echo "}" >> "$OUTPUT_FILE"
  echo "" >> "$OUTPUT_FILE"
done

echo "Generated $OUTPUT_FILE"
```

---

**End of Template Analysis**
