# Loop 03: Add trajectory trace feature with options panel

## Overview

This loop adds a trajectory tracing feature to the frontend that allows users to visualize the robot's path history. The feature includes:
- An "Options" panel in the sidebar with a trace toggle checkbox
- Trajectory history storage in the app store with throttled position recording
- A canvas component that renders the trajectory as a polyline
- A clear button to reset the trajectory history

## Tasks

### Task 1: Extend appStore with trajectory state

**Goal:** Add trajectory-related state fields and actions to the Zustand store.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.ts` |

**Steps:**
1. Define a `TrajectoryPoint` interface with `x`, `y`, and `timestamp` fields
2. Add `traceEnabled: boolean` state field (default: `false`)
3. Add `setTraceEnabled: (enabled: boolean) => void` action
4. Add `trajectoryHistory: TrajectoryPoint[]` state field (default: `[]`)
5. Add `addTrajectoryPoint: (x: number, y: number) => void` action that appends a point with current timestamp
6. Add `clearTrajectory: () => void` action that resets the history to empty array
7. Add `MAX_TRAJECTORY_POINTS = 1000` constant to limit memory usage (circular buffer behavior - remove oldest when full)

**Verify:** Run `npm test -- appStore.test.ts` to ensure existing tests pass, then add new tests.

---

### Task 2: Create OptionsPanel component

**Goal:** Create a new panel component for display options, following the existing panel pattern (StatusPanel, ControlPanel).

**Files:**
| Action | Path |
|--------|------|
| CREATE | `C:\Users\costa\src\warehouser\web_frontend\src\components\panels\OptionsPanel.tsx` |

**Steps:**
1. Create component following the pattern in `StatusPanel.tsx` and `ControlPanel.tsx`
2. Define `optionsPanelConfig: PanelConfig` with id: `'options'`, title: `'Options'`
3. Create `OptionsPanel` functional component accepting `PanelProps`
4. Add "Trajectory Trace" section with:
   - A checkbox toggle for `traceEnabled` using appStore
   - Text label "Show trajectory"
   - A "Clear" button that calls `clearTrajectory()` (disabled when history is empty)
5. Show trajectory point count when trace is enabled (e.g., "123 points")
6. Use consistent styling matching other panels (gray-800 background, rounded-lg, etc.)

**Verify:** Component renders without errors, checkbox toggles state correctly.

---

### Task 3: Add OptionsPanel to layout

**Goal:** Integrate the OptionsPanel into the sidebar layout.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\layouts\LayoutSplit.tsx` |

**Steps:**
1. Import `OptionsPanel` from `../components/panels/OptionsPanel`
2. Add an "Options" section in the sidebar between "Drive Robot" and "Status" sections
3. Follow the existing section pattern with `h3` header using uppercase tracking-wide style
4. Render the OptionsPanel content (trace toggle and clear button)

**Verify:** Options section appears in sidebar with working trace toggle.

---

### Task 4: Create CanvasTrajectory component

**Goal:** Create a canvas component that renders the robot's trajectory as a polyline.

**Files:**
| Action | Path |
|--------|------|
| CREATE | `C:\Users\costa\src\warehouser\web_frontend\src\components\canvas\CanvasTrajectory.tsx` |

**Steps:**
1. Create component following the pattern in `CanvasLidar.tsx`
2. Define `CanvasTrajectoryProps` interface with:
   - `points: TrajectoryPoint[]`
   - `scale: number`
   - `canvasSize: number`
   - `color?: string` (default: `'#3b82f6'` - blue)
   - `strokeWidth?: number` (default: `2`)
   - `opacity?: number` (default: `0.6`)
3. Use `CoordinateTransform` from `../../utils/transforms` to convert world coordinates to canvas
4. Render a Konva `Line` component with `points` array flattened to `[x1, y1, x2, y2, ...]`
5. Return `null` if points array has fewer than 2 points
6. Apply stroke styling with rounded line caps and joins for smooth appearance

**Verify:** Component renders correctly with test data.

---

### Task 5: Export CanvasTrajectory from canvas index

**Goal:** Add the new component to the canvas barrel export.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\components\canvas\index.ts` |

**Steps:**
1. Add `export { CanvasTrajectory } from './CanvasTrajectory'`

**Verify:** Import works from `'./canvas/index'`.

---

### Task 6: Integrate CanvasTrajectory into Canvas

**Goal:** Render the trajectory in the main Canvas component when trace is enabled.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\components\Canvas.tsx` |

**Steps:**
1. Import `CanvasTrajectory` from `'./canvas/index'`
2. Add store selectors for `traceEnabled` and `trajectoryHistory`
3. Render `CanvasTrajectory` component conditionally when `traceEnabled && trajectoryHistory.length >= 2`
4. Position the trajectory layer between zones and objects (so objects render on top)
5. Pass `trajectoryHistory`, `scale`, and `canvasSize` props

**Verify:** Trajectory line appears on canvas when trace is enabled and has points.

---

### Task 7: Add trajectory recording logic

**Goal:** Record robot positions to trajectory history when trace is enabled, with throttling.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\components\RosDataBridge.tsx` |

**Steps:**
1. Import trajectory-related selectors from appStore
2. Add a `useRef` to track last recorded timestamp for throttling
3. Define `TRAJECTORY_THROTTLE_MS = 100` constant (record at most 10 points/second)
4. In the effect that updates entities, check if trace is enabled
5. When trace is enabled and enough time has passed since last recording:
   - Get the selected robot position from entities
   - Call `addTrajectoryPoint(robot.x, robot.y)`
   - Update the last timestamp ref
6. Reset trajectory when simulation is reset (clear on `/sim/reset` call)

**Verify:** Enable trace, move robot, verify points accumulate in store. Check throttling works.

---

### Task 8: Add unit tests for trajectory features

**Goal:** Add comprehensive tests for the new trajectory functionality.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.test.ts` |
| CREATE | `C:\Users\costa\src\warehouser\web_frontend\src\components\panels\OptionsPanel.test.tsx` |
| CREATE | `C:\Users\costa\src\warehouser\web_frontend\src\components\canvas\CanvasTrajectory.test.tsx` |

**Steps:**
1. In `appStore.test.ts`:
   - Test `setTraceEnabled` toggles state correctly
   - Test `addTrajectoryPoint` appends points with timestamps
   - Test `clearTrajectory` empties the history
   - Test max points limit (circular buffer behavior)

2. In `OptionsPanel.test.tsx`:
   - Test checkbox reflects `traceEnabled` state
   - Test clicking checkbox calls `setTraceEnabled`
   - Test clear button is disabled when history is empty
   - Test clear button calls `clearTrajectory` when clicked
   - Test point count display

3. In `CanvasTrajectory.test.tsx`:
   - Test returns null with fewer than 2 points
   - Test renders Line with correct points array
   - Test coordinate transformation is applied correctly

**Verify:** Run `npm test` and ensure all new tests pass.

---

## Acceptance Criteria

- [ ] Trace toggle checkbox appears in sidebar Options section
- [ ] Toggling trace on/off updates appStore state
- [ ] Robot positions are recorded when trace is enabled (throttled to 100ms)
- [ ] Trajectory renders as a blue polyline on the canvas
- [ ] Clear button removes all trajectory points
- [ ] Clear button is disabled when no points exist
- [ ] Point count is displayed when trace is enabled
- [ ] Maximum 1000 points stored (oldest removed when limit reached)
- [ ] All new unit tests pass
- [ ] Existing tests continue to pass
