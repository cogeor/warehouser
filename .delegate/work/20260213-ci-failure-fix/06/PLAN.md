# Plan: Loop 06 - Clean up tracked cache files and update .gitignore

## Objective

Update `.gitignore` to properly ignore coverage and cache files that are currently being tracked by git.

## Analysis

Current `.gitignore` has these relevant patterns:
- `*pycache*` - wildcard pattern (line 5)
- `__pycache__/` - directory pattern (line 6)
- `*.coverage` - matches `foo.coverage` but NOT `.coverage` (line 15)
- `.coverage.*` - matches `.coverage.something` (line 18)
- `*.pyc` - Python bytecode (line 20)

**Problem:** The pattern `*.coverage` does NOT match the literal file `.coverage` because the `*` wildcard does not match leading dots. This is why `training/.coverage` is still being tracked.

## Tasks

### Task 1: Add explicit `.coverage` pattern

**Files to modify:**
- `C:\Users\costa\src\warehouser\.gitignore`

**Changes:**
- Add `.coverage` as an explicit pattern in the Coverage section
- This will ignore the root `.coverage` file that pytest-cov generates

## Verification

1. Confirm `.gitignore` syntax is correct
2. Verify the pattern `.coverage` is present
3. Note: Actual file removal from git tracking will be handled separately (not part of this task)

## Constraints

- DO NOT run `git rm` commands
- Only update `.gitignore`
