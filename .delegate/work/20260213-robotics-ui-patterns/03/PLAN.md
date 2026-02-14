# Loop 03: Create centralized configuration module

## Objective

Create a centralized configuration module that consolidates all configuration constants used across the web frontend, with environment variable support for deployment flexibility.

## Files to Create

1. `web_frontend/src/config/index.ts` - Main configuration module
2. `web_frontend/.env.example` - Environment variable documentation

## Implementation Details

### 1. Configuration Module (`config/index.ts`)

#### ROS Connection Settings
- `ROS_WS_URL`: From `VITE_ROS_WS_URL` env var, default `ws://localhost:9090`
- Reconnect settings:
  - `maxAttempts`: 10
  - `baseDelay`: 1000ms
  - `maxDelay`: 30000ms
  - `factor`: 2 (exponential multiplier)
  - `jitter`: 0.1 (10% randomization)

#### Canvas Settings
- `WORLD_SIZE`: 10 (meters)
- `CANVAS_SIZE`: 600 (pixels)
- `ANIMATION_DURATION`: 100 (ms)
- `ROBOT_SIZE`: 0.6 (meters)
- `OBJECT_SIZE`: 0.4 (meters)

#### Demo Mode Settings
- `DEMO_INTERVAL`: 3000 (ms)
- `COLORS`: ['red', 'green', 'blue', 'yellow']

### 2. Type Safety

All configuration objects will be:
- Exported as `const` with `as const` assertion
- Frozen with `Object.freeze()` for runtime immutability
- Properly typed with explicit interfaces

### 3. Environment Variable Support

The module will read from `import.meta.env` (Vite convention) with fallbacks to default values.

## Dependencies

- None (pure TypeScript module)

## Verification

- [ ] TypeScript compiles without errors
- [ ] All configuration values match specification
- [ ] Objects are properly frozen
- [ ] Environment variable reading works correctly
