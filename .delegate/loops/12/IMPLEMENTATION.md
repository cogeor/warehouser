## Task 12: Create useEntityAnimation hook for Konva refs

Completed: 2026-02-13T14:40:00Z

### Changes

- `web_frontend/src/hooks/useEntityAnimation.ts`: Created new file with two hooks:
  - `useEntityAnimation<T>`: Animates a single Konva node to target position
  - `useMultipleEntityAnimations<T>`: Manages animations for multiple nodes by ID

### Implementation Details

1. **AnimationTarget interface**: Defines x, y, and optional rotation properties
2. **AnimationOptions interface**: Supports custom duration and easing function
3. **useEntityAnimation**:
   - Takes target position and optional animation options
   - Returns a React ref to attach to Konva node
   - Skips animation on first render (positions immediately)
   - Uses Konva.Tween for smooth animations on subsequent renders
   - Properly cleans up tweens on unmount/update
4. **useMultipleEntityAnimations**:
   - Takes Map<string, AnimationTarget> for managing multiple entities
   - Returns getRef callback and refs Map
   - Tracks first-render state per entity ID
   - Cleans up removed entities automatically

### Verification

- [x] TypeScript compiles without errors: `npx tsc --noEmit` passed
- [x] Uses CANVAS_CONFIG.ANIMATION_DURATION (100ms) as default
- [x] Uses Konva.Easings.EaseOut as default easing
- [x] Imports CANVAS_CONFIG from '../config'
- [x] Hook signatures match requirements

### Notes

- Easing type uses the actual Konva function signature `(t: number, b: number, c: number, d: number) => number` rather than `typeof Konva.Easings.EaseOut` to avoid type inference issues
- Return type uses `React.RefObject<T | null>` for compatibility with React 18+ ref patterns

---
