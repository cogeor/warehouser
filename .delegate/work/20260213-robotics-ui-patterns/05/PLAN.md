# Loop 05: Fix Canvas.test.tsx Type Errors

## Objective
Fix TypeScript errors in Canvas.test.tsx without changing test logic.

## Task

**Task 1: Fix Canvas.test.tsx type errors and unused variables**

Files to modify:
- `web_frontend/src/components/Canvas.test.tsx`

Changes required:

1. **Remove unused variables**
   - Remove `createRefMock` function (defined but never used)
   - Remove `createContainerMock` function (defined but never used)

2. **Fix implicit `any` types**
   - Create `KonvaProps` interface for typed mock props
   - Use `ForwardedRef<unknown>` for ref parameters in forwardRef mocks
   - Replace `any` types in Stage, Layer, Group, Rect, Circle, Arrow, Line, Image mocks

3. **Fix Entity interface mismatches**
   - Wall entities: Replace `x2`, `y2` with `width`, `height` (per Entity interface)
   - Zone entities: Remove `size` property (zones use fixed radius in Canvas.tsx)

4. **Fix Array.fill type inference**
   - Add `as number[]` type assertion for `Array(360).fill(1.0)`

## Verification

- TypeScript compilation passes (no errors in Canvas.test.tsx)
- All 13 tests pass
