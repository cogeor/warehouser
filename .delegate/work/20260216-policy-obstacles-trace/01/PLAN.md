# Loop 01: Add frontend UI toggle and ROS topic for policy inference control

## Overview

Add a toggle switch in the frontend ControlPanel that allows users to enable/disable the policy inference. When toggled, the frontend publishes a Bool message to `/inference/enable` which the existing inference node already subscribes to (see `inference_node.cpp:23-25`). The inference node has an `enabled_` flag (defaults to `false`) that gates whether inference runs.

## Tasks

### Task 1: Add policyEnabled state to appStore

**Goal:** Add state and setter for tracking whether policy inference is enabled in the frontend.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.ts` |

**Steps:**
1. Add `policyEnabled: boolean` to the `AppState` interface (after `simRunning`)
2. Add `setPolicyEnabled: (enabled: boolean) => void` to the interface
3. Add initial state `policyEnabled: false` in the store creation
4. Add setter `setPolicyEnabled: (enabled) => set({ policyEnabled: enabled })`

**Verify:** Existing test file structure shows pattern; run `npm test` in web_frontend

---

### Task 2: Add tests for policyEnabled state

**Goal:** Add unit tests for the new policyEnabled state following existing test patterns.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\store\appStore.test.ts` |

**Steps:**
1. Add `policyEnabled: false` to the `beforeEach` reset state (around line 18)
2. Add new `describe('policy')` block after the `selection` tests
3. Add test `it('sets policy enabled state')` that calls `setPolicyEnabled(true)` and verifies

**Verify:** `npm test -- appStore.test.ts`

---

### Task 3: Add policy toggle to ControlPanel

**Goal:** Add a toggle switch UI element in ControlPanel for enabling/disabling policy inference.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\components\panels\ControlPanel.tsx` |

**Steps:**
1. Import `useAppStore` selectors for `policyEnabled` and `setPolicyEnabled` (lines 16-19 area)
2. Create publisher hook for Bool message: `const publishPolicyEnable = useRosPublisher<{ data: boolean }>('/inference/enable', 'std_msgs/msg/Bool')`
3. Add handler function `handlePolicyToggle` that:
   - Toggles `policyEnabled` in store
   - Publishes new state to `/inference/enable`
4. Add toggle UI after the demo section (before closing `</>` around line 179):
   ```tsx
   {/* Policy Control */}
   <div className="border-t border-gray-700 pt-3 mt-3">
     <div className="flex items-center justify-between">
       <span className="text-sm text-gray-300">Policy Inference</span>
       <button
         onClick={handlePolicyToggle}
         className={`relative inline-flex h-6 w-11 items-center rounded-full transition-colors ${
           policyEnabled ? 'bg-blue-600' : 'bg-gray-600'
         }`}
       >
         <span
           className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform ${
             policyEnabled ? 'translate-x-6' : 'translate-x-1'
           }`}
         />
       </button>
     </div>
     {policyEnabled && (
       <p className="text-sm text-blue-400 text-center mt-2">
         Policy running...
       </p>
     )}
   </div>
   ```

**Verify:** Visual inspection in browser; `npm test`

---

### Task 4: Add tests for policy toggle in ControlPanel

**Goal:** Add unit tests for the policy toggle functionality.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `C:\Users\costa\src\warehouser\web_frontend\src\components\ControlPanel.test.tsx` |

**Steps:**
1. Add `policyEnabled: false` to the `beforeEach` state reset
2. Add new test: `it('renders policy toggle')`
3. Add new test: `it('publishes to /inference/enable when toggled')` that:
   - Clicks the policy toggle button
   - Verifies `mockPublishJson` was called with `{ data: true }`
4. Update mock to track topic-specific publishers if needed

**Verify:** `npm test -- ControlPanel.test.ts`

---

### Task 5: End-to-end verification

**Goal:** Verify the complete flow from frontend toggle to inference node.

**Steps:**
1. Build and run: `cd ros_ws && colcon build && ros2 launch warehouser_bringup simulation.launch.py`
2. Open frontend at `http://localhost:5173`
3. Toggle policy switch ON
4. Verify in ROS logs: `[inference]: Inference enabled`
5. Toggle policy switch OFF
6. Verify in ROS logs: `[inference]: Inference disabled`

**Verify:** ROS node logs show enable/disable messages

---

## Acceptance Criteria

- [ ] `policyEnabled` state exists in appStore with getter and setter
- [ ] Unit tests pass for appStore policyEnabled state
- [ ] Toggle switch renders in ControlPanel below demo section
- [ ] Toggle publishes Bool to `/inference/enable` on click
- [ ] Unit tests pass for ControlPanel toggle
- [ ] `npm test` passes with all new tests
- [ ] Frontend toggle triggers inference node log messages
