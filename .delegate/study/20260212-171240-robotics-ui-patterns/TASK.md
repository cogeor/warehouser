# TASK: Modularize Frontend Architecture with Type-Safe ROS Integration

Created: 2026-02-12T18:00:00Z
Build: FAIL (TypeScript errors in tests and connection layer)
Tests: 9/9 passing (with act() warnings)

## Summary

Refactor the web_frontend architecture to improve modularity, type safety, and maintainability by implementing industry-standard robotics UI patterns from Foxglove Studio and RViz2. This includes splitting the monolithic Canvas component, implementing type-safe ROS message interfaces, extracting coordinate transformations into reusable utilities, and creating a panel-based architecture for better component composition.

## Current State Analysis

**Strengths:**
- Modern React 18 + TypeScript 5 stack with strict mode enabled
- Zustand for lightweight state management (94 lines, well-tested)
- Konva/react-konva for 2D canvas rendering with smooth animations
- roslib for WebSocket communication with ROS2 backend
- Zero external sprite dependencies (inline SVG data URLs)
- Robust reconnection logic with exponential backoff

**Critical Issues:**

1. **Canvas.tsx is 449 lines** - Violates single responsibility principle
   - Mixed concerns: rendering, animation, coordinate math, event handling
   - Seven separate refs for animation tracking (lines 43-52)
   - Hard-coded constants (WORLD_SIZE=10, SCALE=60, ROBOT_SIZE=0.6)
   - No separation between static (floor, walls) and dynamic (robots, objects) rendering

2. **No TypeScript interfaces for ROS messages** - All type safety lost at ROS boundary
   - `connection.ts` lines 133-194: Manual type assertions from `unknown`
   - Entity type mapping hardcoded (lines 148-153) instead of using message constants
   - Missing fields from backend messages (TaskStatus has 5 fields, frontend uses 2)
   - No compile-time validation of message structure changes

3. **Hardcoded configuration** - No environment-based configuration
   - WebSocket URL hardcoded to `ws://localhost:9090` (line 58)
   - Animation duration, world size, canvas size scattered throughout code
   - No support for different deployment environments

4. **Missing multi-robot support** - Backend supports it (recent commits), frontend doesn't
   - Assumes single robot: `entities.find(e => e.type === 'robot')`
   - No robot ID tracking or selection
   - Lidar visualization only supports one robot

5. **Build failures** - TypeScript compilation errors prevent production build
   - roslib has implicit `any` type (no @types package)
   - Canvas.test.tsx has type errors (lines 105, 118, 159-160)
   - Unused variables in tests

**Missing Features:**
- No zoom/pan controls for canvas (common in RViz2, Foxglove)
- No entity selection/highlighting (state exists but unused)
- No performance metrics (FPS, latency, entity count)
- No configuration panel for connection settings
- No trajectory/path history visualization

## Target Architecture

Based on analysis of production robotics UIs (Foxglove Studio, RViz2, Robot Web Tools), implement:

### 1. Panel System (Foxglove Pattern)
Each visualization is an isolated, reusable React component registered in a central PanelRegistry. Panels receive standardized props (config, connection, dimensions) and can be composed into layouts.

### 2. Type-Safe ROS Bridge (Industry Standard)
Generate TypeScript interfaces from .msg files to provide compile-time validation and IntelliSense support. Eliminate all type assertions and manual type mappings.

### 3. Coordinate Transform Utilities (RViz2 Pattern)
Extract coordinate transformation logic (ROS REP-103 to Canvas Y-down) into a CoordinateTransform class with comprehensive test coverage.

### 4. Modular Canvas Components
Split Canvas.tsx into composable sub-components:
- `<CanvasFloor>` - Static floor tile rendering
- `<CanvasWalls>` - Wall rendering
- `<CanvasZones>` - Zone rendering
- `<CanvasObjects>` - Object rendering with drag interaction
- `<CanvasRobot>` - Robot sprite and carrying indicator
- `<CanvasLidar>` - Lidar ray visualization
- `<Canvas>` - Composition layer

### 5. Enhanced Connection Management
Encapsulate ROS connection in a class with proper lifecycle management, subscription tracking, and React context integration.

## Implementation Plan

### Phase 1: Foundation - Type Safety and Utilities

**Objective:** Establish type-safe foundation and reusable utilities without breaking existing functionality.

- [ ] Create `web_frontend/src/types/warehouser_msgs.ts` with all ROS message interfaces
  - EntityInfo, EntityType enum, WorldState, LidarDebug, TaskStatus
  - Match exact structure from `warehouser_msgs` package
  - Document field types and units (float32, degrees vs radians)

- [ ] Create `web_frontend/src/utils/transforms.ts` - CoordinateTransform class
  - `worldToCanvas(x, y)` - ROS coords to canvas pixels
  - `canvasToWorld(cx, cy)` - Canvas pixels to ROS coords
  - `worldThetaToCanvasRotation(theta)` - Radians CCW to degrees CW
  - `canvasRotationToWorldTheta(rotation)` - Inverse transform
  - `transformPoint(point, pose)` - Apply pose transformation
  - Unit tests for all transformations (critical for correctness)

- [ ] Create `web_frontend/src/config/index.ts` - Centralized configuration
  - ROS connection settings (URL from env var `VITE_ROS_WS_URL`)
  - Canvas settings (world size, canvas size, animation duration)
  - Demo mode settings (interval, color list)
  - Export as const for type safety

- [ ] Fix build errors
  - Install `@types/roslib` or create declaration file
  - Fix Canvas.test.tsx type errors (Entity interface mismatches)
  - Remove unused variables

### Phase 2: ROS Connection Refactor

**Objective:** Replace module-level singleton with class-based connection manager.

- [ ] Create `web_frontend/src/ros/RosConnection.ts` class
  - Constructor accepts ConnectionConfig (url, reconnect settings)
  - `connect()` - Returns Promise, emits status events
  - `disconnect()` - Cleanup, unsubscribe all topics
  - `subscribe<T>(topic, messageType, callback)` - Type-safe subscription
  - `publish<T>(topic, messageType, message)` - Type-safe publishing
  - `callService<TReq, TRes>(service, serviceType, request)` - Promise-based service calls
  - `getStatus()` - Current connection status
  - `onStatusChange(callback)` - Status change listener
  - Private methods: scheduleReconnect, calculateBackoff, clearReconnectTimeout

- [ ] Create `web_frontend/src/hooks/useRosConnection.ts` - React integration
  - RosConnectionProvider context component
  - useRosConnection() hook
  - useRosTopic<T>(topic, messageType) hook
  - useThrottledTopic<T>(topic, messageType, fps) hook for high-frequency data

- [ ] Create `web_frontend/src/ros/subscriptions.ts` - Topic subscription setup
  - subscribeToTopics(connection) - Set up all subscriptions
  - Use generated types from warehouser_msgs.ts
  - Eliminate all type assertions

- [ ] Update App.tsx to use RosConnectionProvider wrapper

### Phase 3: Canvas Modularization

**Objective:** Break down 449-line Canvas component into maintainable sub-components.

- [ ] Create `web_frontend/src/components/canvas/` directory structure
  - `transforms.ts` - Re-export from utils (for convenience)
  - `useEntityAnimation.ts` - Custom hook for Konva animation refs
  - `CanvasFloor.tsx` - Floor tile rendering (static)
  - `CanvasWalls.tsx` - Wall rendering (static)
  - `CanvasZones.tsx` - Zone rendering (static)
  - `CanvasObjects.tsx` - Object rendering with drag handlers
  - `CanvasRobot.tsx` - Robot sprite with rotation and carrying indicator
  - `CanvasLidar.tsx` - Lidar ray visualization
  - `Canvas.tsx` - Main composition component

- [ ] Implement useEntityAnimation hook
  - Manages Map<string, Konva.Node> refs
  - Handles first-render vs subsequent animation logic
  - Reusable across Robot/Object components
  - Returns: { getRef, setRef, animateTo }

- [ ] Update Canvas.tsx to compose sub-components
  - Import CoordinateTransform from utils
  - Use useThrottledTopic for world state (30 FPS)
  - Pass only necessary props to each sub-component
  - Target: <150 lines for main Canvas component

- [ ] Add unit tests for each sub-component
  - Test coordinate transformations in isolation
  - Test animation logic
  - Test drag interaction
  - Mock Konva where necessary

### Phase 4: Panel System Architecture

**Objective:** Enable modular, configurable UI components with registration system.

- [ ] Create `web_frontend/src/types/panels.ts` - Panel interfaces
  - PanelProps<TConfig> interface
  - PanelDescriptor interface
  - PanelRegistry class (singleton)
  - ConnectionStatus type

- [ ] Refactor existing components as panels
  - MapPanel (wrap Canvas with configuration toolbar)
    - Config: showGrid, showRobots, showObjects, showLidar, backgroundColor
  - StatusPanel (existing StatusPanel with PanelProps)
    - Config: showPosition, showBattery, showTaskState
  - ControlPanel (existing ControlPanel with PanelProps)
    - Config: enableAutoDemo, demoInterval
  - ObjectivePanel (existing ObjectivePanel with PanelProps)
    - Config: availableColors

- [ ] Register panels in panelRegistry
  - Each panel self-registers on module load
  - Panel IDs: 'map-2d', 'status', 'control', 'objective'
  - Categories: 'visualization', 'status', 'control'

- [ ] Create ConnectionStatus component
  - Displays connection state with colored indicator
  - Shows reconnection attempts
  - Provides manual reconnect button
  - Uses useRosConnection hook

### Phase 5: Enhanced Features

**Objective:** Add missing features identified from research.

- [ ] Multi-robot support
  - Update Entity interface to use discriminated union types
  - Change store to Map<string, RobotInfo> instead of single robot
  - Filter robots by ID in components
  - Add robot selector dropdown to StatusPanel

- [ ] Canvas controls
  - Zoom in/out buttons (+/- or mouse wheel)
  - Pan support (drag background or arrow keys)
  - Camera reset button
  - Layer visibility toggles (floor, lidar, zones)

- [ ] Performance monitoring
  - FPS counter component
  - Entity count display
  - Connection latency metric
  - Render time tracking

- [ ] Configuration persistence
  - Save panel configs to localStorage
  - Load saved configs on startup
  - Reset to defaults button

## Interface Definitions

### Core ROS Message Types

```typescript
// types/warehouser_msgs.ts

export interface EntityInfo {
  id: string
  type: EntityType
  x: number // float32, meters
  y: number // float32, meters
  theta?: number // float32, radians (optional for non-robots)
  color?: string // hex color (optional, for objects/zones)
  width?: number // float32, meters (optional, for walls)
  height?: number // float32, meters (optional, for walls)
  is_carrying?: boolean // optional, robot only
  carried_id?: string // optional, robot only
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
  sim_time: number // float64, seconds
  running: boolean
}

export interface LidarDebug {
  header: Header
  ranges: number[] // float32[], meters
  angle_min: number // float32, radians
  angle_max: number // float32, radians
  robot_x: number // float32, meters
  robot_y: number // float32, meters
  robot_theta: number // float32, radians
}

export interface TaskStatus {
  header: Header
  state: TaskState
  intent: string
  task_id: string
  target_color?: string
  distance_to_goal: number // float32, meters
}

export enum TaskState {
  IDLE = 0,
  PICKING = 1,
  CARRYING = 2,
  DELIVERING = 3,
}

export interface Header {
  stamp: Time
  frame_id: string
}

export interface Time {
  sec: number
  nanosec: number
}
```

### Coordinate Transform Utilities

```typescript
// utils/transforms.ts

export interface Transform2D {
  x: number
  y: number
  theta: number
}

export class CoordinateTransform {
  constructor(
    private worldSize: number, // meters
    private canvasSize: number // pixels
  )

  get scale(): number // pixels per meter

  worldToCanvas(x: number, y: number): [number, number]
  canvasToWorld(cx: number, cy: number): [number, number]
  worldThetaToCanvasRotation(theta: number): number
  canvasRotationToWorldTheta(rotation: number): number
  transformPoint(point: [number, number], pose: Transform2D): [number, number]
}

// Singleton instance for convenience
export const transform: CoordinateTransform
```

### Panel System Interfaces

```typescript
// types/panels.ts

export interface PanelProps<TConfig = unknown> {
  config: TConfig
  onConfigChange: (config: TConfig) => void
  connection: RosConnection
  width?: number
  height?: number
}

export interface PanelDescriptor<TConfig = unknown> {
  id: string
  title: string
  description: string
  component: React.ComponentType<PanelProps<TConfig>>
  defaultConfig: TConfig
  icon?: React.ReactNode
  category?: 'visualization' | 'control' | 'status' | 'debug'
  minWidth?: number
  minHeight?: number
}

export class PanelRegistry {
  static getInstance(): PanelRegistry

  register<TConfig>(descriptor: PanelDescriptor<TConfig>): void
  get(id: string): PanelDescriptor | undefined
  getAll(): PanelDescriptor[]
  getByCategory(category: string): PanelDescriptor[]
}

export const panelRegistry: PanelRegistry
```

### ROS Connection Interface

```typescript
// ros/RosConnection.ts

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error'

export interface ConnectionConfig {
  url: string
  reconnect: boolean
  maxReconnectAttempts: number
  reconnectBaseDelay: number // milliseconds
  reconnectMaxDelay: number // milliseconds
}

export class RosConnection {
  constructor(config: Partial<ConnectionConfig>)

  connect(): Promise<void>
  disconnect(): void

  subscribe<T>(
    topicName: string,
    messageType: string,
    callback: (message: T) => void
  ): () => void // Returns unsubscribe function

  publish<T>(
    topicName: string,
    messageType: string,
    message: T
  ): void

  callService<TReq, TRes>(
    serviceName: string,
    serviceType: string,
    request: TReq
  ): Promise<TRes>

  getStatus(): ConnectionStatus
  onStatusChange(callback: (status: ConnectionStatus) => void): () => void
}
```

### React Hooks

```typescript
// hooks/useRosConnection.ts

export function RosConnectionProvider({ children }: PropsWithChildren): JSX.Element

export function useRosConnection(): RosConnection

export function useRosTopic<T>(
  topicName: string,
  messageType: string
): T | null

export function useThrottledTopic<T>(
  topicName: string,
  messageType: string,
  fps?: number
): T | null
```

## New Modules to Create

| Module | Purpose | Key Exports |
|--------|---------|-------------|
| `types/warehouser_msgs.ts` | ROS message type definitions | EntityInfo, WorldState, LidarDebug, TaskStatus, enums |
| `types/panels.ts` | Panel system interfaces | PanelProps, PanelDescriptor, PanelRegistry |
| `utils/transforms.ts` | Coordinate transformation utilities | CoordinateTransform class, transform singleton |
| `config/index.ts` | Centralized configuration | config object with ros, canvas, demo settings |
| `ros/RosConnection.ts` | Connection manager class | RosConnection class, ConnectionStatus type |
| `ros/subscriptions.ts` | Topic subscription setup | subscribeToTopics function |
| `hooks/useRosConnection.ts` | React connection integration | RosConnectionProvider, useRosConnection, useRosTopic, useThrottledTopic |
| `hooks/useEntityAnimation.ts` | Konva animation management | useEntityAnimation hook |
| `components/canvas/CanvasFloor.tsx` | Floor tile rendering | CanvasFloor component |
| `components/canvas/CanvasWalls.tsx` | Wall rendering | CanvasWalls component |
| `components/canvas/CanvasZones.tsx` | Zone rendering | CanvasZones component |
| `components/canvas/CanvasObjects.tsx` | Object rendering with drag | CanvasObjects component |
| `components/canvas/CanvasRobot.tsx` | Robot sprite rendering | CanvasRobot component |
| `components/canvas/CanvasLidar.tsx` | Lidar visualization | CanvasLidar component |
| `components/ConnectionStatus.tsx` | Connection indicator | ConnectionStatus component |
| `components/panels/MapPanel.tsx` | 2D map with controls | MapPanel component |

## Files to Modify

| File | Change |
|------|--------|
| `web_frontend/src/components/Canvas.tsx` | Refactor to use sub-components, reduce from 449 to <150 lines |
| `web_frontend/src/components/StatusPanel.tsx` | Convert to panel interface with PanelProps |
| `web_frontend/src/components/ControlPanel.tsx` | Convert to panel interface, fix interval cleanup |
| `web_frontend/src/components/ObjectivePanel.tsx` | Convert to panel interface, add yellow color option |
| `web_frontend/src/ros/connection.ts` | Replace with RosConnection class usage, remove module singleton |
| `web_frontend/src/store/appStore.ts` | Update Entity interface to discriminated union, add TaskState enum |
| `web_frontend/src/App.tsx` | Wrap with RosConnectionProvider, use panel system |
| `web_frontend/package.json` | Add @types/roslib or create declaration file |
| `web_frontend/tsconfig.json` | Add paths alias for cleaner imports (optional) |
| `web_frontend/.env.example` | Add VITE_ROS_WS_URL example |
| `web_frontend/src/components/Canvas.test.tsx` | Fix type errors, wrap state updates in act() |

## Architecture Notes

### Modularity Principles

1. **Single Responsibility**: Each component handles one concern (rendering, state, connection)
2. **Composition over Inheritance**: Build complex UIs from simple components
3. **Dependency Injection**: Pass connection via context, not module singletons
4. **Type Safety**: No `any` types, no type assertions, generate types from source
5. **Testability**: Pure functions, mockable dependencies, isolated components

### Coordinate System (REP 103)

- **ROS World Frame**: X=forward, Y=left, Z=up, theta=CCW from +X axis
- **Canvas Frame**: X=right, Y=down, origin=top-left, rotation=CW from +Y axis
- **Transform**: `cy = CANVAS_SIZE - y * SCALE` (Y-flip)
- **Rotation**: `degrees = -theta * 180/PI - 90` (Y-flip + sprite orientation)

### Performance Strategy

1. **Throttle High-Frequency Topics**: Decouple data receipt (50Hz) from rendering (30 FPS)
2. **Memoize Expensive Calculations**: Use React.memo and useMemo for entity filtering
3. **Optimize Canvas Rendering**: Separate static (floor, walls) from dynamic (robots) layers
4. **Batch State Updates**: Update multiple store fields in single action
5. **Use requestAnimationFrame**: Sync renders to browser refresh rate

### State Management Strategy

- **Global State (Zustand)**: World entities, simulation status, task state
- **Local State (useState)**: Component-specific UI state (panel configs, form inputs)
- **Connection State (RosConnection class)**: Subscription management, connection status
- **Derived State (useMemo)**: Computed values (filtered entities, coordinate transforms)

### Extensibility Plan

1. **Panel Registry**: New panels self-register, no central modification needed
2. **Plugin System (Future)**: Load panels dynamically from external modules
3. **Message Converters (Future)**: Transform non-standard message formats
4. **Theme System (Future)**: Customize panel appearance
5. **Layout Persistence (Future)**: Save/load panel arrangements

## Verification

### Phase 1 Verification
- [ ] TypeScript compilation succeeds with no errors
- [ ] All message types have IntelliSense support
- [ ] CoordinateTransform unit tests pass (10+ test cases)
- [ ] Config values load from environment variables

### Phase 2 Verification
- [ ] RosConnection connects/disconnects cleanly
- [ ] Reconnection logic works (test by stopping rosbridge)
- [ ] useRosTopic hook receives messages
- [ ] useThrottledTopic limits updates to 30 FPS

### Phase 3 Verification
- [ ] Canvas renders correctly with sub-components
- [ ] All entity types display properly (robots, objects, walls, zones)
- [ ] Lidar visualization works
- [ ] Drag interaction functions
- [ ] No performance regression (maintain 30+ FPS)

### Phase 4 Verification
- [ ] Panel registry lists all panels
- [ ] Panel configs persist to localStorage
- [ ] Connection status updates in real-time
- [ ] All panels receive connection context

### Phase 5 Verification
- [ ] Multiple robots display simultaneously
- [ ] Zoom/pan controls work smoothly
- [ ] FPS counter shows accurate metrics
- [ ] Configuration survives page reload

### Overall Verification
- [ ] All existing tests pass (9/9)
- [ ] No new TypeScript errors
- [ ] Build succeeds (npm run build)
- [ ] Application loads in browser
- [ ] ROS connection establishes automatically
- [ ] Real-time visualization updates smoothly
- [ ] Manual testing: pick/deliver task completes successfully

## Sources

### Research Findings
- **Search Phase (S.md)**: Foxglove Studio architecture, rosbridge_suite vs foxglove_bridge, RViz2 plugin system, panel patterns
- **Introspection Phase (I.md)**: Current codebase analysis, 449-line Canvas component, missing type safety, hardcoded configuration
- **Template Phase (T.md)**: Production implementations from Foxglove/RViz2, type generation scripts, RosConnection class pattern, throttling strategies

### Key References
- Foxglove Studio: https://github.com/foxglove/studio
- RViz2 Plugin Development: https://github.com/ros2/rviz/blob/rolling/docs/plugin_development.md
- rosbridge_suite: https://github.com/RobotWebTools/rosbridge_suite
- REP 103 (Coordinate Conventions): https://www.ros.org/reps/rep-0103.html
- Robot Web Tools: http://robotwebtools.org/

## Implementation Notes

### Priority Ordering

**Must Have (Blocking Issues):**
1. Fix build errors (blocking production deployment)
2. Generate ROS message types (blocking type safety)
3. Modularize Canvas (blocking maintainability)

**Should Have (High Value):**
4. RosConnection class (better architecture)
5. Coordinate transform utilities (testability)
6. Panel system (extensibility)

**Could Have (Nice to Have):**
7. Multi-robot support (future-proofing)
8. Canvas controls (UX improvement)
9. Performance monitoring (debugging aid)

### Risk Mitigation

**Risk**: Breaking existing functionality during refactor
**Mitigation**:
- Implement new modules alongside existing code
- Switch incrementally (one component at a time)
- Run tests after each phase
- Keep existing code until new code proven

**Risk**: Performance degradation from additional abstractions
**Mitigation**:
- Profile before/after with React DevTools
- Use React.memo strategically
- Implement throttling from start
- Benchmark canvas render times

**Risk**: Type generation script fails on complex message types
**Mitigation**:
- Start with manual type definitions
- Validate against actual message structure
- Add automated generation later as enhancement

### Development Workflow

1. Create feature branch: `feature/modular-ui-architecture`
2. Implement Phase 1 (foundation) - commit
3. Implement Phase 2 (connection) - commit
4. Implement Phase 3 (canvas) - commit
5. Implement Phase 4 (panels) - commit
6. Implement Phase 5 (features) - commit
7. Update documentation (README, CLAUDE.md)
8. Create pull request with detailed description

### Success Metrics

- **Code Quality**: TypeScript errors: 8 → 0, Lines per component: <150
- **Type Safety**: Type assertions: 6 → 0, Generated interfaces: 0 → 8
- **Maintainability**: Canvas.tsx: 449 lines → <150 lines, Modules created: 15+
- **Performance**: FPS: maintain 30+, Build time: <30s
- **Test Coverage**: Components with tests: 4 → 15+, Transform tests: 0 → 10+

---

**End of TASK.md**
