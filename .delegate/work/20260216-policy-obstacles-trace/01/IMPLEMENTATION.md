# Loop 01 Implementation: Add frontend UI toggle and ROS topic for policy inference control

## Task 1: Add policyEnabled state to appStore

Completed: 2026-02-16

### Files Modified

- `web_frontend/src/store/appStore.ts`: Added policyEnabled boolean state and setPolicyEnabled setter to AppState interface and store implementation

### Changes Made

Added new state properties to track policy inference status:
- `policyEnabled: boolean` in the AppState interface (after simTime)
- `setPolicyEnabled: (enabled: boolean) => void` setter in the interface
- Initial state `policyEnabled: false` in the store creation
- Setter implementation `setPolicyEnabled: (enabled) => set({ policyEnabled: enabled })`

### Verification

- [x] TypeScript compiles without errors
- [x] Store state accessible via useAppStore hook

---

## Task 2: Add tests for policyEnabled state

Completed: 2026-02-16

### Files Modified

- `web_frontend/src/store/appStore.test.ts`: Added policyEnabled to beforeEach reset and new policy describe block with tests

### Changes Made

- Added `policyEnabled: false` to the beforeEach state reset
- Added new `describe('policy')` test block with:
  - `it('sets policy enabled state')` - verifies setPolicyEnabled(true) works
  - `it('can toggle policy enabled state')` - verifies toggling on and off

### Verification

- [x] `npm test -- --run appStore.test.ts`: All 12 tests pass

---

## Task 3: Add policy toggle to ControlPanel

Completed: 2026-02-16

### Files Modified

- `web_frontend/src/components/panels/ControlPanel.tsx`: Added policyEnabled state, publisher, handler, and toggle UI

### Changes Made

1. Added state selectors:
   ```typescript
   const policyEnabled = useAppStore((s) => s.policyEnabled)
   const setPolicyEnabled = useAppStore((s) => s.setPolicyEnabled)
   ```

2. Added ROS publisher:
   ```typescript
   const publishPolicyEnable = useRosPublisher<{ data: boolean }>('/inference/enable', 'std_msgs/msg/Bool')
   ```

3. Added handler function:
   ```typescript
   const handlePolicyToggle = () => {
     const newState = !policyEnabled
     setPolicyEnabled(newState)
     publishPolicyEnable({ data: newState })
   }
   ```

4. Added toggle switch UI after the demo section:
   - Toggle switch with blue/gray color states
   - "Policy Inference" label
   - "Policy running..." status text when enabled

### Verification

- [x] TypeScript compiles without errors
- [x] Toggle renders in ControlPanel
- [x] Toggle publishes Bool to `/inference/enable`

---

## Task 4: Add tests for policy toggle in ControlPanel

Completed: 2026-02-16

### Files Modified

- `web_frontend/src/components/ControlPanel.test.tsx`: Updated mock and added policy toggle tests

### Changes Made

1. Added separate mock for policy enable publisher:
   ```typescript
   const mockPublishPolicyEnable = vi.fn()
   ```

2. Updated useRosPublisher mock to return topic-specific functions:
   ```typescript
   useRosPublisher: (topic: string) => {
     if (topic === '/inference/enable') return mockPublishPolicyEnable
     return mockPublishJson
   }
   ```

3. Added `policyEnabled: false` to beforeEach state reset

4. Added new `describe('policy toggle')` block with tests:
   - `it('renders policy toggle')` - verifies "Policy Inference" label renders
   - `it('publishes to /inference/enable when toggled on')` - verifies publish with `{ data: true }`
   - `it('publishes to /inference/enable when toggled off')` - verifies publish with `{ data: false }`
   - `it('updates store state when toggled')` - verifies policyEnabled state changes
   - `it('shows status text when policy is enabled')` - verifies "Policy running..." text

### Verification

- [x] `npm test -- --run ControlPanel.test.tsx`: All 11 tests pass
- [x] `npm test -- --run`: All 135 frontend tests pass

---

## Summary

All 4 tasks completed successfully. The frontend now has:
1. `policyEnabled` state in the Zustand store
2. Unit tests for the new state
3. Toggle switch UI in ControlPanel that publishes Bool messages to `/inference/enable`
4. Unit tests for the toggle functionality

The inference node already subscribes to `/inference/enable` and uses an `enabled_` flag to gate inference, so the frontend toggle should now control policy inference when the full system is running.
