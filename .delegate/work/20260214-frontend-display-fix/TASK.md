# Frontend Display Fix

## Problem
The frontend at `http://localhost:3001` connects to the ROS bridge but displays nothing - blank canvas with no entities, no lidar, no task status.

## Root Cause Analysis
Two separate ROS connection systems existed:

1. **Old system** (`ros/connection.ts`):
   - Uses `initRosConnection()`
   - Subscribes to topics and updates Zustand `appStore`
   - Never called anywhere in the app

2. **New system** (`ros/RosConnection.ts` + `hooks/useRosConnection.tsx`):
   - Used by `RosConnectionProvider` in `App.tsx`
   - Creates connection but does NOT subscribe to topics
   - Topic hooks (`useWorldState`, etc.) update local React state only

The Canvas component uses `useAppStore` (Zustand) to get entities/lidar data:
```tsx
const entities = useAppStore((s) => s.entities)  // Always empty!
```

But the new hooks never update the store - they keep data in their own local state.

## Solution
Created `RosDataBridge.tsx` - a component that:
1. Uses new hooks (`useWorldState`, `useLidarDebug`, `useTaskStatus`) to receive ROS data
2. Updates the Zustand `appStore` via `useEffect` whenever data changes
3. Renders nothing (purely side-effect component)

Added to `App.tsx`:
```tsx
<RosConnectionProvider>
  <RosDataBridge />  // New bridge component
  <div className="min-h-screen p-4">
    ...
  </div>
</RosConnectionProvider>
```

## Files Changed
- `src/components/RosDataBridge.tsx` - New file (bridge component)
- `src/App.tsx` - Import and render RosDataBridge
- `src/components/canvas/*.test.tsx` - Fixed unused React import warnings

## Verification
- Build passes: `npm run build`
- Tests pass: 127/127 tests
- Docker logs show subscriptions being created
- Frontend displays connection status and canvas
