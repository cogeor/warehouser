# Implementation Log - Loop 03: Trajectory Trace Feature

## Task 1: Extend appStore with trajectory state

Completed: 2026-02-16T10:00:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.ts`: Added trajectory-related types and state
  - Added `TrajectoryPoint` interface with `x`, `y`, `timestamp` fields
  - Added `MAX_TRAJECTORY_POINTS = 1000` constant
  - Added `traceEnabled: boolean` state field (default: false)
  - Added `setTraceEnabled: (enabled: boolean) => void` action
  - Added `trajectoryHistory: TrajectoryPoint[]` state field (default: [])
  - Added `addTrajectoryPoint: (x: number, y: number) => void` action with circular buffer behavior
  - Added `clearTrajectory: () => void` action

### Verification

- [x] TrajectoryPoint interface exported
- [x] MAX_TRAJECTORY_POINTS constant exported
- [x] State fields added to AppState interface
- [x] Actions implemented with circular buffer logic

### Notes

The `addTrajectoryPoint` action automatically adds a timestamp using `Date.now()` and implements circular buffer behavior by slicing off the oldest points when the limit is exceeded.

---

## Task 2: Create OptionsPanel component

Completed: 2026-02-16T10:05:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\components\panels\OptionsPanel.tsx`: Created new panel component
  - Defined `optionsPanelConfig: PanelConfig` with id 'options', title 'Options'
  - Created `OptionsPanel` functional component accepting `PanelProps`
  - Added checkbox toggle for `traceEnabled` with label "Show trajectory"
  - Added "Clear" button that calls `clearTrajectory()`, disabled when history is empty
  - Added point count display when trace is enabled

### Verification

- [x] Component renders correctly
- [x] Checkbox toggles traceEnabled state
- [x] Clear button disabled when no points
- [x] Point count displays correctly

### Notes

Follows the existing panel pattern from StatusPanel.tsx and ControlPanel.tsx. Uses consistent gray-on-white styling to match the sidebar layout.

---

## Task 3: Add OptionsPanel to layout

Completed: 2026-02-16T10:10:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\layouts\LayoutSplit.tsx`:
  - Imported `OptionsPanel` from `../components/panels/OptionsPanel`
  - Added "Options" section between "Drive Robot" and "Status" sections
  - Used h3 header with uppercase tracking-wide style matching other sections
  - Rendered OptionsPanel inside the section

### Verification

- [x] Options section visible in sidebar
- [x] Checkbox and Clear button functional
- [x] Styling consistent with other sidebar sections

### Notes

None.

---

## Task 4: Create CanvasTrajectory component

Completed: 2026-02-16T10:15:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\components\canvas\CanvasTrajectory.tsx`: Created canvas component
  - Defined `CanvasTrajectoryProps` interface with points, scale, canvasSize, color, strokeWidth, opacity
  - Implemented coordinate transformation using `CoordinateTransform` class
  - Renders Konva `Line` component with flattened points array
  - Returns null if fewer than 2 points
  - Applied rounded line caps and joins for smooth appearance

### Verification

- [x] Component renders polyline correctly
- [x] Coordinate transformation applied (Y-flip for canvas)
- [x] Default styling: blue (#3b82f6), strokeWidth 2, opacity 0.6

### Notes

Uses the existing CoordinateTransform utility for consistent world-to-canvas coordinate conversion.

---

## Task 5: Export CanvasTrajectory from canvas index

Completed: 2026-02-16T10:17:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\components\canvas\index.ts`:
  - Added `export { CanvasTrajectory } from './CanvasTrajectory'`

### Verification

- [x] Component importable from canvas/index

### Notes

None.

---

## Task 6: Integrate CanvasTrajectory into Canvas

Completed: 2026-02-16T10:20:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\components\Canvas.tsx`:
  - Imported `CanvasTrajectory` from `./canvas/index`
  - Added store selectors for `traceEnabled` and `trajectoryHistory` (with shallow comparison)
  - Rendered CanvasTrajectory conditionally when `traceEnabled && trajectoryHistory.length >= 2`
  - Positioned trajectory layer between zones and objects (so objects render on top)

### Verification

- [x] Trajectory renders when enabled
- [x] Trajectory hidden when disabled
- [x] Z-ordering correct (trajectory behind objects)

### Notes

None.

---

## Task 7: Add trajectory recording logic in RosDataBridge

Completed: 2026-02-16T10:25:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\components\RosDataBridge.tsx`:
  - Added `TRAJECTORY_THROTTLE_MS = 100` constant (10 points/second max)
  - Added `lastTrajectoryTimestampRef` to track last recording time
  - Added trajectory recording logic in worldState effect
  - Records selected robot position when trace enabled and throttle interval passed

- `C:\Users\costa\src\warehouser\web_frontend\src\layouts\LayoutSplit.tsx`:
  - Added `clearTrajectory` to useSimControls hook
  - Called `clearTrajectory()` in reset button handler

### Verification

- [x] Points recorded when trace enabled
- [x] Throttling works (max 10 Hz)
- [x] Trajectory cleared on simulation reset

### Notes

Trajectory recording is throttled to 100ms intervals to prevent excessive memory usage while maintaining smooth visualization.

---

## Task 8: Add unit tests for trajectory features

Completed: 2026-02-16T10:35:00Z

### Changes

- `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.test.ts`:
  - Added trajectory fields to beforeEach reset
  - Added 'trajectory trace' describe block with 5 tests:
    - setTraceEnabled toggles state correctly
    - addTrajectoryPoint appends points with timestamps
    - addTrajectoryPoint accumulates multiple points
    - clearTrajectory empties the history
    - enforces max points limit with circular buffer behavior

- `C:\Users\costa\src\warehouser\web_frontend\src\components\panels\OptionsPanel.test.tsx`: Created new test file
  - Tests for checkbox reflecting state
  - Tests for clicking checkbox calling setTraceEnabled
  - Tests for clear button disabled/enabled states
  - Tests for clear button calling clearTrajectory
  - Tests for point count display
  - Tests for isCollapsed behavior

- `C:\Users\costa\src\warehouser\web_frontend\src\components\canvas\CanvasTrajectory.test.tsx`: Created new test file
  - Tests for returning null with < 2 points
  - Tests for rendering Line with correct points array
  - Tests for coordinate transformation
  - Tests for default and custom styling

### Verification

- [x] All 158 tests pass
- [x] appStore tests cover trajectory state
- [x] OptionsPanel tests cover UI interactions
- [x] CanvasTrajectory tests cover rendering logic

### Notes

All tests pass. Some pre-existing warnings about act() in Canvas.test.tsx are unrelated to this implementation.

---

## Summary

All 8 tasks completed successfully. The trajectory trace feature is fully implemented with:
- State management in Zustand store with circular buffer
- Options panel with checkbox toggle and clear button
- Canvas component rendering the trajectory as a blue polyline
- Recording logic with 100ms throttling
- Trajectory cleared on simulation reset
- Comprehensive unit test coverage (17 new tests)
