# Warehouser Web Frontend

React-based visualization frontend for the Warehouser ROS2 robot simulation.

## Features

- Real-time visualization of robot, objects, walls, and zones
- Multi-robot support with robot selection
- Lidar visualization
- Connection status with automatic reconnection
- Zoom and pan controls
- Performance monitoring (FPS counter)
- Collapsible panel system

## Architecture

### Folder Structure

- `src/components/` - React components
  - `canvas/` - Konva canvas sub-components
  - `panels/` - Panel system components
- `src/hooks/` - Custom React hooks
- `src/ros/` - ROS connection and subscription utilities
- `src/store/` - Zustand state management
- `src/types/` - TypeScript type definitions
- `src/config/` - Configuration module
- `src/utils/` - Utility functions

### Key Technologies

- **React 18** - UI framework
- **TypeScript** - Type safety
- **Zustand** - State management
- **react-konva** - 2D canvas rendering
- **roslib** - ROS WebSocket bridge
- **Tailwind CSS** - Styling
- **Vite** - Build tool
- **Vitest** - Testing

## Development

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Run tests
npm test

# Type check
npx tsc --noEmit

# Build for production
npm run build
```

## Environment Variables

See `.env.example` for available configuration options.

## ROS Topics

- `/world/state` - World state (entities, sim time)
- `/observations/lidar_debug` - Lidar visualization data
- `/task/status` - Task state machine status
