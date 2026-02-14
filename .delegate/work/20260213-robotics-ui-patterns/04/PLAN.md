# Loop 04: Create roslib Type Declarations

## Objective

Fix TypeScript build error: "Could not find a declaration file for module 'roslib'"

The roslib package does not have official @types/roslib definitions. Create a TypeScript declaration file that provides type definitions for the roslib classes used in this project.

## Analysis

### Current Usage in connection.ts

The following ROSLIB types are used:

1. **ROSLIB.Ros** - ROS WebSocket connection
   - Constructor: `new ROSLIB.Ros({ url: string })`
   - Methods: `on(event, callback)`, `connect(url)`, `close()`
   - Used as type annotation: `let ros: ROSLIB.Ros | null`

2. **ROSLIB.Topic** - ROS topic publisher/subscriber
   - Constructor: `new ROSLIB.Topic({ ros, name, messageType })`
   - Methods: `subscribe(callback)`, `publish(message)`, `unsubscribe()`

3. **ROSLIB.Service** - ROS service client
   - Constructor: `new ROSLIB.Service({ ros, name, serviceType })`
   - Methods: `callService(request, callback)`

4. **ROSLIB.ServiceRequest** - Service request wrapper
   - Constructor: `new ROSLIB.ServiceRequest({})`

5. **ROSLIB.Message** - Message wrapper
   - Constructor: `new ROSLIB.Message({ data: ... })`

### Requirements

- Module declaration for `import ROSLIB from 'roslib'`
- Global namespace declaration for `ROSLIB.Ros` type annotations
- Generic type parameters for Topic, Service, Message classes

## Task

| # | Description | Files |
|---|-------------|-------|
| 1 | Create roslib.d.ts with type declarations | `web_frontend/src/types/roslib.d.ts` |

## Success Criteria

- [ ] TypeScript compiles without "Cannot find module 'roslib'" error
- [ ] TypeScript compiles without "Cannot find namespace 'ROSLIB'" error
- [ ] All existing tests pass
- [ ] Type declarations cover all roslib usage in connection.ts
