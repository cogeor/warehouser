# Loop 32: Add robot selector to StatusPanel

## Task 1: Add robot selector dropdown to StatusPanel

Completed: 2026-02-13

### Changes

- `web_frontend/src/components/panels/StatusPanel.tsx`:
  - Updated import to include `selectRobots` and `selectSelectedRobot` selectors from appStore
  - Replaced `entities` state with `robots` (using `selectRobots` selector) and `selectedRobot` (using `selectSelectedRobot` selector)
  - Added `setSelectedRobotId` action from store
  - Added robot selector dropdown UI that displays only when there are multiple robots (robots.length > 1)
  - Changed status display to use `selectedRobot` instead of finding the first robot entity

### Verification

- [x] TypeScript compilation: `npx tsc --noEmit` - passes (only pre-existing test file warnings)
- [x] File only modifies StatusPanel.tsx as required
- [x] Imports added from appStore: `selectRobots`, `selectSelectedRobot`
- [x] Robot selector shows only when robots.length > 1
- [x] Uses selectedRobot for status display instead of finding first robot

### Notes

The TypeScript compilation shows 4 warnings about unused React imports in test files (CanvasFloor.test.tsx, CanvasLidar.test.tsx, CanvasWalls.test.tsx, CanvasZones.test.tsx). These are pre-existing issues unrelated to this implementation.

---
