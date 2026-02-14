# TASK: Fix CI Pipeline Failures - Missing ROS2 Dependencies

Created: 2026-02-12 17:30:00
Build: SKIPPED (local environment issue - missing pydantic)
Tests: N/A (local test failed on dependency import)

## Summary

The CI pipeline is failing because the GitHub Actions workflow and Dockerfile are missing critical ROS2 package dependencies that were added in recent commits. Specifically, `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` were added to the `warehouser_observations` package on February 2, 2026 (commits 9bec276 and 434ed99), but the CI configuration files were not updated accordingly.

## Root Cause

On February 2, 2026, two commits introduced new ROS2 message dependencies:

1. **Commit 9bec276**: Added OdometrySimulator publishing `nav_msgs/Odometry` messages
2. **Commit 434ed99**: Added LidarSimulator publishing `sensor_msgs/LaserScan` messages

These dependencies were correctly added to `warehouser_observations/package.xml` and `CMakeLists.txt`, but the CI infrastructure was not updated:

- `.github/workflows/ci.yml` lacks `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs`
- `Dockerfile` lacks the same two dependencies

**Impact**: Both `ros2-build-test` and `docker-build` jobs fail at CMake configuration with:
```
CMake Error: By not providing "Findnav_msgs.cmake" in CMAKE_MODULE_PATH...
```

## Context

### Sources

**[I] Introspection Findings:**
- Identified exact missing dependencies: `nav_msgs` and `sensor_msgs`
- Located the commits that introduced these dependencies (9bec276, 434ed99)
- Found matching issues in both CI workflow and Dockerfile
- Discovered secondary issues: missing web frontend testing in CI, tracked cache files in git

**[S] Search Findings:**
- Best practice: Use `rosdep install` to automatically resolve dependencies from package.xml
- Recommended: Migrate to `action-ros-ci` for automatic dependency management
- Pattern: Path-based workflow triggers for multi-language projects
- Standard: Separate workflows for ROS2, Python, and TypeScript components

**[T] Template Findings:**
- Found production-ready templates for `action-ros-ci` workflows
- Identified modern uv-based Python CI patterns
- Learned about colcon mixins for coverage and optimization
- Discovered reusable workflow patterns from ros-controls

## Objective

Fix the immediate CI failures by adding missing ROS2 dependencies, then implement modern CI best practices to prevent similar issues in the future.

## Implementation Plan

### Phase 1: Immediate Fix (Required - Unblocks CI)

**Priority: CRITICAL - Blocks all PR merges**

- [ ] Add `ros-jazzy-nav-msgs` to `.github/workflows/ci.yml` dependency list
- [ ] Add `ros-jazzy-sensor-msgs` to `.github/workflows/ci.yml` dependency list
- [ ] Add `ros-jazzy-nav-msgs` to `Dockerfile` dependency list
- [ ] Add `ros-jazzy-sensor-msgs` to `Dockerfile` dependency list
- [ ] Verify CI passes after changes

**Estimated Time**: 5 minutes
**Risk**: None - simple addition of known dependencies

### Phase 2: Modernize CI Infrastructure (Recommended)

**Priority: HIGH - Prevents future dependency sync issues**

- [ ] Create `.github/workflows/ros2-ci.yaml` using `action-ros-ci`
- [ ] Create `.github/workflows/python-ci.yaml` for training tests
- [ ] Create `.github/workflows/typescript-ci.yaml` for web frontend tests
- [ ] Update `.github/workflows/ci.yml` to call the three new workflows
- [ ] Add path-based triggers to prevent unnecessary CI runs
- [ ] Test all three workflows independently

**Estimated Time**: 2 hours
**Risk**: Medium - requires validation across all three stacks

### Phase 3: Repository Hygiene (Optional but Recommended)

**Priority: MEDIUM - Improves repository cleanliness**

- [ ] Remove tracked cache files: `git rm --cached training/.coverage`
- [ ] Remove tracked `__pycache__` directories
- [ ] Update `.gitignore` to be more explicit about coverage files
- [ ] Commit `.gitignore` changes separately

**Estimated Time**: 15 minutes
**Risk**: Low - only affects repository cleanliness

## Interface Definitions

### Workflow File Structure (Phase 2)

```
.github/workflows/
├── ci.yml                    # Orchestrator (calls all sub-workflows)
├── ros2-ci.yaml              # ROS2 build and test
├── python-ci.yaml            # Python training tests
└── typescript-ci.yaml        # TypeScript frontend tests
```

### ROS2 Dependency Management

All ROS2 dependencies must be declared in three places:
1. `package.xml` (source of truth)
2. `.github/workflows/ci.yml` or use rosdep
3. `Dockerfile` (for production builds)

**Recommended**: Use `rosdep install --from-paths src --ignore-src -r -y` to automatically sync from package.xml.

## Files to Modify

| File | Change |
|------|--------|
| `.github/workflows/ci.yml:26-37` | Add `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` to apt install list |
| `Dockerfile:11-16` | Add `ros-jazzy-nav-msgs` and `ros-jazzy-sensor-msgs` to apt install list |
| `.gitignore` | Add explicit patterns: `.coverage`, `.coverage.*`, `__pycache__/` |

## Files to Create (Phase 2)

| File | Purpose |
|------|---------|
| `.github/workflows/ros2-ci.yaml` | Modern ROS2 CI using action-ros-ci with automatic rosdep |
| `.github/workflows/python-ci.yaml` | Python training tests with uv, mypy, pytest |
| `.github/workflows/typescript-ci.yaml` | TypeScript frontend tests with npm, vitest |

## Verification

### Phase 1 Verification

- [ ] Push changes to a branch
- [ ] Open pull request
- [ ] Verify `ros2-build-test` job passes (colcon build succeeds)
- [ ] Verify `docker-build` job passes (Docker image builds)
- [ ] Verify `python-test` and `python-lint` still pass
- [ ] Merge to main once all checks green

### Phase 2 Verification

- [ ] Test ROS2 workflow: `gh workflow run ros2-ci.yaml`
- [ ] Test Python workflow: `gh workflow run python-ci.yaml`
- [ ] Test TypeScript workflow: `gh workflow run typescript-ci.yaml`
- [ ] Verify path triggers work (only affected workflows run)
- [ ] Confirm rosdep installs dependencies automatically

### Phase 3 Verification

- [ ] Run `git status` and verify no tracked cache files
- [ ] Create new coverage files and verify they're gitignored
- [ ] Run tests and verify new `__pycache__` is gitignored

## Architecture Notes

### Modular CI Design

The recommended Phase 2 approach follows modern multi-language CI patterns:

1. **Isolation**: Each language stack (ROS2/C++, Python, TypeScript) has independent workflows
2. **Efficiency**: Path-based triggers prevent unnecessary CI runs
3. **Parallelism**: All three workflows run simultaneously on PR
4. **Maintainability**: Each workflow uses language-specific best practices
5. **Debugging**: Failed workflow doesn't block unrelated stacks

### Dependency Management Strategy

**Current State** (manual sync):
```
package.xml → manually copy → ci.yml
                           └→ Dockerfile
```

**Recommended State** (automatic sync):
```
package.xml → rosdep install → automatically fetches all dependencies
```

This eliminates the need to manually maintain dependency lists in three places.

### Risk Assessment

**Phase 1 (Immediate Fix)**:
- **Risk**: None - Adding known dependencies
- **Downside**: Maintains technical debt (manual dependency sync)
- **Benefit**: Unblocks CI immediately

**Phase 2 (Modernization)**:
- **Risk**: Medium - New workflow patterns to validate
- **Downside**: Initial time investment
- **Benefit**: Prevents future dependency sync issues, adds web frontend testing, follows ROS2 best practices

**Phase 3 (Hygiene)**:
- **Risk**: Low - Only affects repository cleanliness
- **Downside**: None
- **Benefit**: Cleaner git history, prevents cache file conflicts

## Success Criteria

### Minimum Success (Phase 1 Complete)

- [ ] CI pipeline passes on main branch
- [ ] ROS2 packages build successfully in CI
- [ ] Docker image builds successfully
- [ ] All existing tests still pass

### Full Success (All Phases Complete)

- [ ] Modern CI using action-ros-ci with automatic dependency resolution
- [ ] Web frontend tests running in CI
- [ ] No tracked cache files in repository
- [ ] Path-based triggers working correctly
- [ ] All three language stacks tested independently

## Timeline Estimate

- **Phase 1**: 5 minutes (can be done immediately)
- **Phase 2**: 2 hours (one work session)
- **Phase 3**: 15 minutes (quick cleanup)
- **Total**: ~2.5 hours for complete solution

## Next Steps

1. Start with Phase 1 to unblock CI immediately
2. Once CI is green, proceed with Phase 2 modernization
3. Complete Phase 3 as final cleanup
4. Document new CI patterns in CLAUDE.md for future reference
