# Search: Robotics UI Patterns and Visualization Frameworks

Created: 2026-02-12T17:15:00Z

## Query

Primary searches conducted:
1. "Foxglove Studio robotics visualization architecture React TypeScript 2026"
2. "rosbridge suite WebSocket protocol ROS2 web interface real-time 2026"
3. "RViz2 plugin architecture panel system visualization components ROS2"

## Findings

### 1. Foxglove Studio - Modern Web-Based Robotics Visualization

**Primary URL:** [GitHub - foxglove/studio](https://github.com/foxglove/studio)

**Key Insights:**

**Architecture & Tech Stack:**
- Built entirely in TypeScript and React
- Originally forked from Cruise's Webviz project, but significantly evolved
- Targets both browser environments and Electron-based desktop applications
- Uses an "open core" licensing model (Mozilla Public License v2.0 for most functionality)
- Available at studio.foxglove.dev (web) or as desktop downloads

**Extension System:**
- Provides an Extensions SDK for building custom React-based visualizations
- Extensions written in TypeScript can be shared via online registry
- No recompilation required for extensions - hot-loadable
- Leverages Foxglove's existing data and layout management features
- [Building Custom React Panels](https://foxglove.dev/blog/building-a-custom-react-panel-with-foxglove-studio-extensions) provides detailed guidance

**Panel/Layout System:**
- Improved layout management compared to original Webviz
- Modular panel architecture where each visualization is a self-contained React component
- Panels can be arranged, resized, and configured through drag-and-drop interface
- State persisted across sessions

**Data Format:**
- Uses MCAP (Modular Container for Analytic Playback) file format
- Serialization-agnostic container format optimized for pub/sub and robotics
- Performant playback of recorded data

**Relevance to Warehouser:**
- Demonstrates best-in-class TypeScript/React architecture for robotics
- Extension SDK pattern could inspire plugin system for custom visualizations
- Layout management system is production-proven
- Component isolation pattern keeps concerns separated

---

### 2. Rosbridge Suite - WebSocket Bridge to ROS

**Primary URLs:**
- [RobotWebTools/rosbridge_suite](https://github.com/RobotWebTools/rosbridge_suite)
- [Using Rosbridge with ROS 2](https://foxglove.dev/blog/using-rosbridge-with-ros2)
- [What is ROSBridge?](https://foxglove.dev/robotics/rosbridge)

**Key Insights:**

**Protocol & Architecture:**
- Provides JSON interface to ROS pub/sub, services, and parameters
- Supports multiple transport layers: WebSockets (primary), TCP
- Three main components:
  1. `rosbridge_library` - Python API that translates JSON to ROS operations
  2. `rosbridge_server` - WebSocket server implementation
  3. `rosapi` - Service calls for ROS meta-information (topic lists, parameters)

**WebSocket Protocol:**
- Exposes ROS topics/services through WebSocket connections
- Browser-based tools can monitor telemetry, visualize data, control actuators
- No need to be on same ROS network - works across internet
- Real-time bidirectional communication

**Client Libraries:**
- `roslibjs` - JavaScript/TypeScript API (most relevant for web UIs)
- `jrosbridge` - Java API
- `roslibpy` - Python API
- All communicate over WebSockets using JSON protocol

**ROS 2 Support:**
- Humble branch for ROS2 Humble
- `ros2` branch tracks Jazzy and Rolling
- Installation: `sudo apt install ros-$ROS_DISTRO-rosbridge-suite`
- [Setup guide for ROS2](https://medium.com/@rafaazahra_93357/how-setup-rosbridge-suite-for-ros2-roslib-js-library-74b918db1a64)

**Important Note - Foxglove Recommendation:**
- Foxglove no longer recommends rosbridge
- They now recommend `foxglove_bridge` instead
- Installation: `sudo apt install ros-$ROS_DISTRO-foxglove-bridge`
- Launch: `ros2 launch foxglove_bridge foxglove_bridge_launch.xml`
- Likely more performant and better maintained

**Alternative Bridges:**
- [ros2-web-bridge](https://github.com/RobotWebTools/ros2-web-bridge) - Direct ROS2 bridge
- [ros2bridge](https://pypi.org/project/ros2bridge/) - Converts ROS2 DDS to WebSocket
- [Integration Service ROS2-WebSocket](https://integration-service.docs.eprosima.com/en/v3.1.0/examples/different_protocols/pubsub/ros2-websocket.html)

**Benefits for Web UIs:**
- Zero-install user interfaces - just open a web page
- No complex ROS development environment needed for operators
- Create dashboards, operator interfaces, data insights pages
- Modern web development tools and workflows
- Share across organization easily

**Relevance to Warehouser:**
- Essential bridge technology for web_frontend to communicate with ROS2 backend
- JSON protocol is straightforward to work with in TypeScript
- `roslibjs` provides typed interfaces for topic pub/sub
- Foxglove bridge might be better choice than rosbridge for performance

---

### 3. RViz2 - Native ROS2 Visualization Architecture

**Primary URLs:**
- [GitHub - ros2/rviz](https://github.com/ros2/rviz)
- [Building a Custom RViz Panel (Humble)](https://docs.ros.org/en/humble/Tutorials/Intermediate/RViz/RViz-Custom-Panel/RViz-Custom-Panel.html)
- [Plugin Development Guide](https://github.com/ros2/rviz/blob/rolling/docs/plugin_development.md)
- [RViz User Guide](https://docs.ros.org/en/humble/Tutorials/Intermediate/RViz/RViz-User-Guide/RViz-User-Guide.html)

**Key Insights:**

**Plugin Architecture:**
- Highly modular plugin system with multiple extension points
- Pluggable transformation library (frames can be switched dynamically)
- Core foundation in `rviz_common` package
- Four main plugin types:
  1. **Display Plugins** - Draw 3D visualizations (point clouds, robot models, etc.)
  2. **Panel Plugins** - UI controls and information displays
  3. **Tool Plugins** - Interactive tools (selection, navigation)
  4. **View Controller Plugins** - Camera control methods

**Display Plugin Pattern:**
- Base class: `rviz_common::RosTopicDisplay<MessageType>`
- Template on message type for type safety
- Handles subscription/unsubscription automatically
- Provides `Ogre::SceneNode` for adding 3D objects
- Override `onInitialize()` for setup, `processMessage()` for updates
- Status message system for user feedback

**Default Visualization Components:**
- **TF (Transformation):** Coordinate frame tree visualization
- **RobotModel:** 3D robot from URDF/SDF files
- **Camera:** Image/video streams
- **PointCloud:** 3D point cloud data (LiDAR, depth cameras)
- **LaserScan:** 2D/3D laser range finder data
- **Map:** 2D/3D environment maps
- **Path:** Trajectory visualization
- **Marker:** Custom 3D shapes and annotations

**Panel System:**
- Three main UI areas:
  1. 3D Visualization panel (central view)
  2. Display panel (configuration sidebar)
  3. Views panel (camera/tool settings)
- Panels are dockable and rearrangeable
- "Add New Panel" menu discovers all available panel plugins
- Custom panels created in C++ or Python

**Dependencies for Plugin Development:**
```
rclcpp
class_loader
pluginlib
Qt5
rviz2
rviz_common
rviz_default_plugins
rviz_rendering
rviz_ogre_vendor
```

**Rendering System:**
- Uses Ogre3D rendering engine
- Scene graph with visual objects (arrows, shapes, text)
- `rviz_rendering` package contains rendering primitives
- Display class provides SceneNode for adding visuals
- Objects in `ogre_helpers` folder (ported from ROS1)

**Custom Panel Example:**
- [my_rviz2_plugin](https://github.com/githir/my_rviz2_plugin) - Panel for signal display (Humble+)
- Shows typical structure and dependencies

**Relevance to Warehouser:**
- Plugin architecture demonstrates separation of concerns
- Display plugin pattern (templated on message type) could inspire web components
- Panel system shows importance of modular, composable UI
- Default components list guides what visualizations warehouse UI needs
- While RViz2 is C++/Qt, architectural patterns translate to React/TypeScript

---

## Additional Research Recommendations

Based on this initial search, the following areas warrant deeper investigation:

1. **roslibjs API and TypeScript definitions**
   - Actual usage patterns for pub/sub in React components
   - Connection management and reconnection strategies
   - TypeScript type definitions for ROS messages

2. **Robot Web Tools ecosystem**
   - ros3djs for 3D visualization in browser (WebGL)
   - nav2djs for 2D map visualization
   - React component libraries built on these

3. **Real-time state management patterns**
   - How to handle high-frequency updates (lidar, pose) in React
   - Optimistic updates vs server-authoritative state
   - WebSocket connection state management
   - Handling latency and packet loss

4. **Multi-robot fleet visualization**
   - Scalability patterns for 10+ robots
   - Aggregation strategies for sensor data
   - Fleet-level vs individual robot views

5. **Canvas/WebGL rendering libraries**
   - Pixi.js vs Three.js vs custom Canvas API
   - Performance considerations for real-time updates
   - 2D map rendering with occupancy grids

6. **Comparison tools**
   - [Rerun.io](https://www.reduct.store/blog/comparison-rviz-foxglove-rerun) - Modern visualization tool
   - How it compares to Foxglove and RViz2

## Cloned Repositories

None cloned in this search cycle. Candidates for future cloning:

- `foxglove/studio` - If we need deep architectural reference (very large repo)
- `RobotWebTools/rosbridge_suite` - For protocol details
- Example projects using roslibjs + React for practical patterns

## Proposal

Based on this research, I recommend the following architecture for Warehouser's web_frontend:

### 1. Communication Layer

**Use foxglove_bridge instead of rosbridge:**
- Better performance and maintenance
- More modern protocol
- Foxglove team actively developing it
- Fallback to rosbridge if issues arise

**Connection Architecture:**
```typescript
// WebSocket connection manager
class RosConnection {
  private socket: WebSocket;
  private reconnectAttempts: number;
  private subscriptions: Map<string, Subscription>;

  connect(url: string): Promise<void>
  subscribe<T>(topic: string, messageType: string): Observable<T>
  publish<T>(topic: string, messageType: string, message: T): void
  callService<TReq, TRes>(service: string, request: TReq): Promise<TRes>
}
```

### 2. Component Architecture (Inspired by Foxglove)

**Panel System:**
- Each visualization is isolated React component
- Panels register themselves with central registry
- Layout manager handles arrangement and persistence
- Props include topic name, message type, display options

**Example Panel Interface:**
```typescript
interface PanelProps {
  topic?: string;
  config: PanelConfig;
  connection: RosConnection;
  onConfigChange: (config: PanelConfig) => void;
}

interface PanelConfig {
  // Panel-specific configuration
  [key: string]: unknown;
}

interface PanelDescriptor {
  id: string;
  title: string;
  component: React.ComponentType<PanelProps>;
  defaultConfig: PanelConfig;
  icon?: React.ReactNode;
}
```

### 3. Essential Panels for Warehouser

Based on RViz2's default displays, Warehouser needs:

1. **2D Map Panel** - Occupancy grid, robot poses, paths
2. **Robot Status Panel** - Battery, task state, velocity
3. **Sensor Panel** - Lidar visualization (2D rays)
4. **Fleet Overview Panel** - All robots at once
5. **Task Queue Panel** - Pending pickup/delivery tasks
6. **Metrics Panel** - Rewards, episode stats during training

### 4. State Management (Zustand Pattern)

```typescript
// Global state for robot data
interface RobotState {
  robots: Map<string, RobotInfo>;
  worldMap: OccupancyGrid | null;
  tasks: Task[];
  connection: ConnectionStatus;
}

// Actions for updating state
interface RobotActions {
  updateRobotPose: (id: string, pose: Pose) => void;
  updateMap: (map: OccupancyGrid) => void;
  setConnectionStatus: (status: ConnectionStatus) => void;
}

// Zustand store
const useRobotStore = create<RobotState & RobotActions>((set) => ({
  // state
  robots: new Map(),
  worldMap: null,
  tasks: [],
  connection: 'disconnected',

  // actions
  updateRobotPose: (id, pose) => set((state) => {
    const robots = new Map(state.robots);
    robots.set(id, { ...robots.get(id), pose });
    return { robots };
  }),
  // ... other actions
}));
```

### 5. Real-Time Update Strategy

**High-frequency topics (pose, lidar):**
- Throttle updates using requestAnimationFrame
- Only re-render at 30-60 FPS even if receiving 100Hz data
- Use React refs to avoid re-rendering entire component tree

**Low-frequency topics (status, tasks):**
- Direct state updates through Zustand
- Component re-renders on change

**Example throttling:**
```typescript
function usePoseSubscription(topic: string) {
  const [pose, setPose] = useState<Pose | null>(null);
  const latestPoseRef = useRef<Pose | null>(null);
  const connection = useConnection();

  useEffect(() => {
    const sub = connection.subscribe<PoseMsg>(topic, 'geometry_msgs/Pose');

    // Store latest but don't trigger re-render yet
    sub.subscribe((msg) => {
      latestPoseRef.current = msg;
    });

    // Update state at controlled rate
    const interval = setInterval(() => {
      if (latestPoseRef.current) {
        setPose(latestPoseRef.current);
      }
    }, 1000 / 30); // 30 FPS

    return () => {
      sub.unsubscribe();
      clearInterval(interval);
    };
  }, [topic, connection]);

  return pose;
}
```

### 6. Canvas Rendering for 2D Map

**Use Canvas API directly (not Pixi.js/Three.js):**
- Warehouser is primarily 2D visualization
- Canvas API sufficient for occupancy grids, robot positions, paths
- Lower bundle size than heavyweight libraries
- Better control over rendering loop

**Rendering pattern:**
```typescript
function MapCanvas({ worldMap, robots }: MapCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Clear
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw occupancy grid
    if (worldMap) {
      drawOccupancyGrid(ctx, worldMap);
    }

    // Draw robots
    robots.forEach(robot => {
      drawRobot(ctx, robot);
    });

  }, [worldMap, robots]);

  return <canvas ref={canvasRef} width={800} height={600} />;
}
```

### 7. Message Type Definitions

**Generate TypeScript types from ROS messages:**
- Use `warehouser_msgs` package definitions
- Create TypeScript interfaces matching message structure
- Maintain type safety throughout application

**Example:**
```typescript
// Generated from warehouser_msgs/msg/RobotState.msg
interface RobotState {
  header: Header;
  robot_id: string;
  pose: Pose;
  velocity: Twist;
  battery_level: number; // float32
  task_state: number; // uint8
  carrying_object: boolean;
}
```

### 8. Extensibility

**Plugin registration pattern (inspired by RViz2):**
```typescript
// Panel registry
class PanelRegistry {
  private panels = new Map<string, PanelDescriptor>();

  register(descriptor: PanelDescriptor): void {
    this.panels.set(descriptor.id, descriptor);
  }

  get(id: string): PanelDescriptor | undefined {
    return this.panels.get(id);
  }

  getAll(): PanelDescriptor[] {
    return Array.from(this.panels.values());
  }
}

// Usage in panel components
import { PanelRegistry } from './registry';

PanelRegistry.register({
  id: 'map-2d',
  title: '2D Map',
  component: MapPanel,
  defaultConfig: { showGrid: true, showRobots: true },
});
```

## Next Steps

1. **Deep dive into roslibjs** - Understand actual API usage patterns
2. **Review Foxglove Studio source code** - Study their panel system implementation
3. **Prototype WebSocket connection** - Test foxglove_bridge vs rosbridge with Warehouser
4. **Design message type codegen** - Tool to generate TypeScript from .msg files
5. **Implement canvas rendering prototype** - Verify performance for real-time updates

## Sources

- [GitHub - foxglove/studio](https://github.com/foxglove/studio)
- [Building a Custom React Panel with Foxglove Extensions](https://foxglove.dev/blog/building-a-custom-react-panel-with-foxglove-studio-extensions)
- [Comparing Robotics Visualization Tools: RViz, Foxglove, Rerun](https://www.reduct.store/blog/comparison-rviz-foxglove-rerun)
- [RobotWebTools/rosbridge_suite](https://github.com/RobotWebTools/rosbridge_suite)
- [Using Rosbridge with ROS 2](https://foxglove.dev/blog/using-rosbridge-with-ros2)
- [What is ROSBridge?](https://foxglove.dev/robotics/rosbridge)
- [Setup rosbridge_suite for ROS2](https://medium.com/@rafaazahra_93357/how-setup-rosbridge-suite-for-ros2-roslib-js-library-74b918db1a64)
- [RobotWebTools/ros2-web-bridge](https://github.com/RobotWebTools/ros2-web-bridge)
- [GitHub - ros2/rviz](https://github.com/ros2/rviz)
- [Building a Custom RViz Panel - Humble](https://docs.ros.org/en/humble/Tutorials/Intermediate/RViz/RViz-Custom-Panel/RViz-Custom-Panel.html)
- [Building a Custom RViz Panel - Rolling](https://docs.ros.org/en/rolling/Tutorials/Intermediate/RViz/RViz-Custom-Panel/RViz-Custom-Panel.html)
- [RViz Plugin Development Guide](https://github.com/ros2/rviz/blob/rolling/docs/plugin_development.md)
- [RViz User Guide](https://docs.ros.org/en/humble/Tutorials/Intermediate/RViz/RViz-User-Guide/RViz-User-Guide.html)
- [GitHub - githir/my_rviz2_plugin](https://github.com/githir/my_rviz2_plugin)
