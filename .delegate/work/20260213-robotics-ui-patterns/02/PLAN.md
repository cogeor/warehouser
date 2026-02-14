# Loop 02: CoordinateTransform Class

## Objective

Create a reusable CoordinateTransform class that handles conversion between ROS world coordinates and canvas pixel coordinates, extracting the transformation logic from Canvas.tsx into a testable utility.

## Scope

### Files to Create
- `web_frontend/src/utils/transforms.ts` - CoordinateTransform class implementation
- `web_frontend/src/utils/transforms.test.ts` - Comprehensive unit tests

### Coordinate Systems

**ROS Coordinates (REP 103):**
- X: forward (positive)
- Y: left (positive)
- Z: up (positive)
- Theta: counter-clockwise from X-axis (radians)

**Canvas Coordinates:**
- X: right (positive)
- Y: down (positive)
- Origin: top-left corner
- Rotation: clockwise (degrees)

## Tasks

### Task 1: Create CoordinateTransform Class
- Implement constructor with worldSize and canvasSize parameters
- Add input validation for positive values
- Calculate and store scale factor

### Task 2: Implement Coordinate Conversion Methods
- `worldToCanvas(x, y)` - World meters to canvas pixels
- `canvasToWorld(cx, cy)` - Canvas pixels to world meters
- Handle Y-axis flip between coordinate systems

### Task 3: Implement Angle Conversion Methods
- `worldThetaToCanvasRotation(theta)` - Radians CCW to degrees CW
- `canvasRotationToWorldTheta(rotation)` - Degrees CW to radians CCW
- Account for sprite orientation offset (-90 degrees)

### Task 4: Implement Point Transformation Methods
- `transformPoint(point, pose)` - Apply SE(2) transformation
- `inverseTransformPoint(worldPoint, pose)` - Inverse transformation
- Support rotation + translation

### Task 5: Implement Utility Methods
- `distance(p1, p2)` - Euclidean distance
- `normalizeAngle(angle)` - Normalize to [-PI, PI]

### Task 6: Create Comprehensive Tests
- Constructor validation tests
- Round-trip conversion tests
- Edge case tests (boundaries, large/small values)
- Custom scale ratio tests

## Constants

Based on existing Canvas.tsx:
```typescript
WORLD_SIZE = 10   // meters
CANVAS_SIZE = 600 // pixels
SCALE = 60        // pixels per meter
```

## Dependencies

- None (pure TypeScript utility)
- Vitest for testing

## Verification

- All unit tests pass
- TypeScript compilation succeeds
- Existing Canvas.tsx tests remain passing
