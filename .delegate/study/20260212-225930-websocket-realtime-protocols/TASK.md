# TASK: Enhance WebSocket Communication for Real-Time Robot Visualization

Created: 2026-02-12T23:15:00Z
Build: FAIL (TypeScript errors in Canvas.test.tsx and connection.ts)
Tests: 29/29 PASS (with warnings - act() wrapper needed)

## Summary

Optimize WebSocket communication between ROS2 and frontend for low-latency, reliable real-time robot visualization. Current implementation uses rosbridge_server with basic reconnection logic but lacks type safety, connection cleanup, message validation, and performance optimizations. Migration path to foxglove_bridge with enhanced client-side patterns (interpolation, throttling, batching) will reduce latency by 30-50% and bandwidth by 60-80%.

## Context

### Consolidated Findings

**[S] Search Phase - Industry Best Practices:**
- foxglove_bridge outperforms rosbridge for ROS2 (C++ vs Python, 30-50% latency reduction)
- WebSocket compression (permessage-deflate) reduces traffic by 60-80% for JSON
- Client-side interpolation enables smooth 60 FPS rendering from 10-20 Hz data
- Message batching for non-critical data reduces network congestion by 70-90%
- Binary protocols (Protobuf/MessagePack) significantly faster than JSON for high-frequency data
- Exponential backoff with jitter (current: 1s, 2s, 4s, 8s... max 30s) is best practice
- Heartbeat/ping every 5-10s with 30s timeout prevents stale connections

**[I] Introspection Phase - Current Implementation:**
- Uses rosbridge_server on port 9090 with roslibjs v1.3.0 frontend
- Good: Exponential backoff reconnection with jitter already implemented
- Good: Clean separation (connection.ts, appStore.ts, Canvas.tsx)
- Gap: No connection cleanup (memory leaks on unmount/reload)
- Gap: No message validation (type assertions without runtime checks)
- Gap: Hardcoded URL (ws://localhost:9090) prevents remote deployment
- Gap: No compression configured despite rosbridge support
- Gap: No QoS profiles specified on ROS publishers
- Gap: World state published at 50 Hz (20ms) with full state (no delta compression)
- Gap: No client-side throttling (state updates on every message)
- Gap: No latency monitoring or performance metrics
- Issue: TypeScript build failures (roslib missing types, test errors)

**[T] Template Phase - Reference Patterns:**
- Complete RosConnection class with subscription pooling
- Client-side interpolation hook for 60 FPS rendering
- Throttled topic subscription to match display rate
- foxglove_bridge launch configuration
- Connection status monitoring with latency tracking
- Message batching patterns for telemetry

## Target State

**Optimized WebSocket Architecture:**
1. Type-safe connection management with proper lifecycle
2. foxglove_bridge for high-performance ROS2 communication
3. Client-side interpolation for smooth 60 FPS visualization
4. Selective compression (JSON: enable, binary: disable)
5. Throttled subscriptions to prevent excessive re-renders
6. Message validation with runtime schema checking
7. Performance monitoring (latency, bandwidth, FPS)
8. Externalized configuration (environment variables)

## Implementation Plan

### Phase 1: Type Safety and Connection Lifecycle (Week 1)

#### 1.1 Fix Build Errors
- [ ] Install @types/roslib: `npm i --save-dev @types/roslib`
- [ ] Fix Canvas.test.tsx TypeScript errors (remove unused vars, fix Entity properties)
- [ ] Add error parameter type in connection.ts:111
- [ ] Wrap Canvas state updates in act() for test warnings

#### 1.2 Enhanced Connection Management
- [ ] Create new RosConnection class (web_frontend/src/ros/RosConnection.ts)
  - Subscription pooling (multiple callbacks per topic)
  - Proper cleanup on disconnect
  - Heartbeat monitoring (10s interval, 30s timeout)
  - Status change listeners
  - Type-safe publish/subscribe/callService methods
- [ ] Replace existing connection.ts with RosConnection-based implementation
- [ ] Add cleanup function to App.tsx useEffect (unsubscribe, close connection)
- [ ] Update StatusPanel to show connection status from RosConnection

#### 1.3 Message Validation
- [ ] Install zod for runtime validation: `npm i zod`
- [ ] Create message schemas (web_frontend/src/ros/schemas.ts):
  - WorldStateSchema (entities, sim_time)
  - LidarDebugSchema (ranges, angle_min, angle_max)
  - TaskStatusSchema (state, intent)
- [ ] Wrap message handlers in try-catch with validation
- [ ] Log validation errors to console
- [ ] Display validation errors in UI when relevant

#### 1.4 Configuration Externalization
- [ ] Create .env file for WebSocket URL
- [ ] Add VITE_ROS_WS_URL environment variable
- [ ] Update connection.ts to use env var with localhost:9090 fallback
- [ ] Document configuration in README

### Phase 2: Performance Optimizations (Week 2)

#### 2.1 Client-Side Interpolation
- [ ] Create useInterpolatedPosition hook (web_frontend/src/hooks/useInterpolatedPosition.ts)
  - Linear interpolation between position updates
  - Angle interpolation with shortest path (handle wraparound)
  - RequestAnimationFrame for 60 FPS rendering
  - Timestamp-based duration calculation
- [ ] Integrate into Canvas.tsx for robot position
- [ ] Remove or reduce Konva animation duration (currently 80ms)
- [ ] Test smooth motion at different ROS publishing rates

#### 2.2 Throttled Topic Subscriptions
- [ ] Create useThrottledTopic hook (web_frontend/src/hooks/useThrottledTopic.ts)
  - Receive all messages in ref (no re-render)
  - Update state at controlled rate (30 FPS via RAF)
  - Configurable FPS parameter
- [ ] Apply to WorldState subscription (30 FPS)
- [ ] Apply to LidarDebug subscription (30 FPS)
- [ ] Measure React re-render reduction (DevTools Profiler)

#### 2.3 Connection Status Monitoring
- [ ] Create ConnectionStatusBar component (web_frontend/src/components/ConnectionStatusBar.tsx)
  - Display connection status (disconnected/connecting/connected/error)
  - Show latency metric (RTT via service call)
  - Retry button for manual reconnection
  - Color-coded status indicator
- [ ] Integrate into App.tsx header
- [ ] Add latency measurement via /rosapi/get_time service (every 5s)

### Phase 3: Bridge Migration (Week 3)

#### 3.1 foxglove_bridge Installation
- [ ] Install: `sudo apt install ros-jazzy-foxglove-bridge`
- [ ] Verify installation: `ros2 pkg list | grep foxglove`
- [ ] Test standalone: `ros2 run foxglove_bridge foxglove_bridge`

#### 3.2 Launch Configuration
- [ ] Add foxglove_bridge to full_system.launch.py (port 8765)
- [ ] Configure topic whitelist: /world/state, /observations/lidar_debug, /task/status
- [ ] Set compression: use_compression=false (enable later for JSON only)
- [ ] Set max_qos_depth: 10
- [ ] Keep rosbridge running during migration (port 9090)

#### 3.3 Frontend Migration
- [ ] Add env variable: VITE_USE_FOXGLOVE=true
- [ ] Update connection URL to port 8765 when enabled
- [ ] Test all subscriptions work with foxglove_bridge
- [ ] Test service calls (/sim/start, /sim/pause, /sim/reset)
- [ ] Test publishing (/command/json, /sim/move_entity)
- [ ] Measure latency improvement (compare before/after)

#### 3.4 Deprecate rosbridge
- [ ] Document migration in CHANGELOG
- [ ] Remove rosbridge from launch files after 1 week testing
- [ ] Update package.json dependencies if needed
- [ ] Update documentation (run.md, CLAUDE.md)

### Phase 4: Advanced Optimizations (Week 4)

#### 4.1 Selective Compression
- [ ] Enable compression in foxglove_bridge for JSON topics:
  - /world/state: enable (60-80% reduction)
  - /task/status: enable
  - /observations/lidar_debug: disable (binary data)
- [ ] Set compression_level: 6 (balance speed/ratio)
- [ ] Measure bandwidth reduction (browser DevTools Network tab)

#### 4.2 Message Batching (Optional)
- [ ] Create RobotMetricsBatch message type (if telemetry added)
- [ ] Implement ObservationBatcher node for low-priority data
- [ ] Frontend: useBatchedMetrics hook for aggregate statistics
- [ ] Test bandwidth reduction for batched vs individual messages

#### 4.3 Performance Monitoring
- [ ] Add performance metrics to Zustand store:
  - messageLatency (timestamp comparison)
  - messagesPerSecond (counter)
  - clientFPS (RAF-based measurement)
- [ ] Create PerformancePanel component (optional debug panel)
- [ ] Log performance warnings (latency > 100ms, FPS < 30)

### Phase 5: Testing and Documentation (Ongoing)

#### 5.1 Testing
- [ ] Fix Canvas.test.tsx warnings (wrap updates in act())
- [ ] Add RosConnection unit tests
- [ ] Add integration tests for reconnection logic
- [ ] Test with simulated network delays (Chrome DevTools)
- [ ] Load test with multiple clients connected

#### 5.2 Documentation
- [ ] Update README with WebSocket configuration
- [ ] Document environment variables (.env.example)
- [ ] Add troubleshooting guide for connection issues
- [ ] Document performance optimizations and expected gains

## Interface Definitions

### Message Schemas (Zod)

```typescript
// web_frontend/src/ros/schemas.ts

import { z } from 'zod'

export const EntitySchema = z.object({
  id: z.string(),
  type: z.number().int().min(0).max(3),
  x: z.number(),
  y: z.number(),
  theta: z.number().optional(),
  color: z.string().optional(),
  width: z.number().optional(),
  height: z.number().optional(),
  is_carrying: z.boolean().optional(),
  carried_id: z.string().optional(),
})

export const WorldStateSchema = z.object({
  entities: z.array(EntitySchema),
  sim_time: z.number(),
})

export const LidarDebugSchema = z.object({
  ranges: z.array(z.number()),
  angle_min: z.number(),
  angle_max: z.number(),
})

export const TaskStatusSchema = z.object({
  state: z.string(),
  intent: z.string(),
})

export type WorldState = z.infer<typeof WorldStateSchema>
export type LidarDebug = z.infer<typeof LidarDebugSchema>
export type TaskStatus = z.infer<typeof TaskStatusSchema>
```

### Connection Types

```typescript
// web_frontend/src/ros/types.ts

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error'

export interface ConnectionConfig {
  url: string
  reconnect: boolean
  maxReconnectAttempts: number
  reconnectBaseDelay: number
  reconnectMaxDelay: number
  reconnectFactor: number
  reconnectJitter: number
}

export interface Position {
  x: number
  y: number
  theta: number
  timestamp: number
}
```

## Files to Create

| File | Purpose |
|------|---------|
| `web_frontend/src/ros/RosConnection.ts` | Type-safe connection manager with pooling and lifecycle |
| `web_frontend/src/ros/RosConnectionProvider.tsx` | React Context provider for RosConnection |
| `web_frontend/src/ros/schemas.ts` | Zod schemas for message validation |
| `web_frontend/src/ros/types.ts` | TypeScript type definitions |
| `web_frontend/src/hooks/useInterpolatedPosition.ts` | Client-side interpolation for 60 FPS |
| `web_frontend/src/hooks/useThrottledTopic.ts` | Throttled topic subscription hook |
| `web_frontend/src/components/ConnectionStatusBar.tsx` | Enhanced connection status with latency |
| `web_frontend/.env.example` | Environment variable template |

## Files to Modify

| File | Change |
|------|--------|
| `web_frontend/src/ros/connection.ts` | Replace with RosConnection-based implementation |
| `web_frontend/src/App.tsx` | Add cleanup, integrate ConnectionStatusBar |
| `web_frontend/src/components/Canvas.tsx` | Use interpolation and throttling hooks |
| `web_frontend/src/components/Canvas.test.tsx` | Fix TypeScript errors, wrap in act() |
| `web_frontend/src/components/StatusPanel.tsx` | Use RosConnection status instead of Zustand |
| `web_frontend/package.json` | Add zod, @types/roslib dependencies |
| `web_frontend/.gitignore` | Add .env to ignore list |
| `ros_ws/src/warehouser_bringup/launch/full_system.launch.py` | Add foxglove_bridge node |
| `ros_ws/src/warehouser_bringup/launch/demo.launch.py` | Add foxglove_bridge node |
| `ros_ws/src/warehouser_bringup/package.xml` | Add foxglove_bridge dependency |
| `run.md` | Document foxglove_bridge installation |

## Architecture Notes

### Subscription Pooling Pattern

Multiple React components can subscribe to the same ROS topic without creating duplicate ROSLIB.Topic instances. RosConnection maintains a Map of subscriptions with Set of callbacks, creating the topic only once and calling all registered callbacks on each message.

**Benefits:**
- Reduced memory usage
- Fewer WebSocket subscriptions
- Easier cleanup (unsubscribe when last callback removed)

### Interpolation vs Animation

Current Konva animation (80ms duration with EaseOut) masks latency but doesn't provide smooth intermediate frames. New approach uses requestAnimationFrame to render at native display rate (60 FPS) with linear interpolation between discrete ROS updates (10-20 Hz).

**Result:** Smoother motion, reduced perceived latency, better visualization accuracy.

### Throttling Strategy

High-frequency topics (50+ Hz) trigger React re-renders on every message. Throttling pattern receives all messages but only updates React state at controlled rate (30 FPS) using requestAnimationFrame, preventing excessive re-renders while maintaining data freshness.

**Key principle:** Separate data reception from UI updates.

### foxglove_bridge vs rosbridge

| Feature | rosbridge | foxglove_bridge |
|---------|-----------|-----------------|
| Implementation | Python/JavaScript | C++ |
| Latency | Baseline | 30-50% lower |
| Compression | CBOR, PNG | permessage-deflate |
| ROS2 Support | Basic | Full (params, graph) |
| Schema Support | ROS msgs | ROS msgs, IDL, Protobuf |
| Development | Maintenance mode | Active |

### Migration Safety

Run both bridges simultaneously during transition:
- rosbridge: port 9090 (existing clients)
- foxglove_bridge: port 8765 (new clients)

Frontend checks VITE_USE_FOXGLOVE env var to switch bridges. This allows gradual rollout and easy rollback if issues discovered.

## Verification

### Build and Tests
- [ ] TypeScript compilation passes (npm run build)
- [ ] All tests pass (npm test)
- [ ] No console errors in browser
- [ ] No memory leaks (Chrome DevTools Memory tab)

### Functional Tests
- [ ] Connection establishes on app load
- [ ] Reconnection works after ROS shutdown/restart
- [ ] Robot position updates smoothly at 60 FPS
- [ ] Lidar visualization updates correctly
- [ ] Task status displays current state
- [ ] Service calls succeed (/sim/start, pause, reset)
- [ ] Command publishing works (pick actions)
- [ ] Drag-and-drop entity movement works

### Performance Tests
- [ ] Latency < 50ms (measured via timestamp comparison)
- [ ] Client FPS consistently 60 (measured via RAF)
- [ ] React re-renders reduced by 40-60% (DevTools Profiler)
- [ ] Bandwidth reduced by 60-80% with compression (Network tab)
- [ ] No frame drops during high-frequency updates

### Edge Cases
- [ ] Handles ROS bridge crash gracefully
- [ ] Recovers from network interruption
- [ ] Works on remote server (not just localhost)
- [ ] Multiple browser tabs can connect simultaneously
- [ ] Connection cleanup prevents memory leaks on unmount

## Expected Outcomes

### Performance Improvements
- **Latency:** 30-50% reduction (foxglove_bridge + binary)
- **Bandwidth:** 60-80% reduction (compression + throttling)
- **Client FPS:** Consistent 60 FPS (interpolation)
- **React re-renders:** 40-60% reduction (throttling)
- **Reconnection time:** Sub-second recovery (proper connection management)

### Code Quality Improvements
- Type-safe message handling (Zod validation)
- No memory leaks (proper cleanup)
- Configurable deployment (environment variables)
- Better error handling (try-catch, user feedback)
- Comprehensive testing (unit + integration)

### User Experience Improvements
- Smoother robot motion (60 FPS interpolation)
- Clear connection status (visual indicator + latency)
- Faster response to commands (lower latency)
- Reliable reconnection (exponential backoff)
- Better debugging (performance metrics, validation errors)

## Success Criteria

1. Build passes without TypeScript errors
2. All tests pass without warnings
3. Connection cleanup verified (no memory leaks)
4. Latency < 50ms measured via timestamp
5. Client renders at 60 FPS consistently
6. foxglove_bridge successfully replaces rosbridge
7. Compression reduces JSON bandwidth by 60%+
8. Documentation complete and accurate

## Risk Mitigation

### Risk: foxglove_bridge incompatibility
- Mitigation: Run both bridges during transition, easy rollback
- Test: Verify all existing functionality before deprecating rosbridge

### Risk: Interpolation accuracy issues
- Mitigation: Use simple linear interpolation, validate against ground truth
- Test: Compare interpolated vs actual positions, ensure error < 5%

### Risk: Performance regression
- Mitigation: Measure before/after metrics, A/B test if needed
- Test: Load test with multiple clients, monitor CPU/memory usage

### Risk: Breaking changes to existing code
- Mitigation: Gradual refactoring, maintain backward compatibility
- Test: Comprehensive integration tests, manual QA

## Notes

- Current build fails due to missing @types/roslib and test errors - fix first
- Existing reconnection logic is good - enhance with pooling and cleanup
- rosbridge still viable for development - migrate to foxglove for production
- Client-side optimizations (interpolation, throttling) work with any bridge
- Message validation adds safety but requires schema maintenance
- Environment variables essential for deployment flexibility
- Performance monitoring helps identify regressions early

## References

- [foxglove_bridge Documentation](https://docs.foxglove.dev/docs/visualization/ros-foxglove-bridge)
- [foxglove_bridge GitHub](https://github.com/foxglove/ros-foxglove-bridge)
- [roslibjs Documentation](http://robotwebtools.org/jsdoc/roslibjs/current/)
- [WebSocket Compression RFC 7692](https://datatracker.ietf.org/doc/html/rfc7692)
- [High Performance Browser Networking - WebSockets](https://hpbn.co/websocket/)
- [React act() Testing Utility](https://reactjs.org/link/wrap-tests-with-act)
- [Zod Schema Validation](https://zod.dev/)
