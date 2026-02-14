# Implementation: Refactor Main CI Workflow

## Task 1: Refactor ci.yml

Completed: 2026-02-13T12:00:00Z

### Changes

- `.github/workflows/ci.yml`: Refactored to remove duplicate jobs and add path-based triggers

### Details

**Removed jobs (now in dedicated workflows):**
- `ros2-build-test` - moved to `ros2-ci.yaml`
- `python-lint` - moved to `python-ci.yaml`
- `python-test` - moved to `python-ci.yaml`

**Kept jobs:**
- `docker-build` - remains in ci.yml (not covered by other workflows)
- `ci-success` - simplified to only depend on docker-build

**New triggers:**
- Path-based triggers for Docker-related files:
  - `Dockerfile`
  - `docker-compose*.yml`
  - `.dockerignore`
  - `.github/workflows/ci.yml`
- Added `workflow_dispatch` for manual triggering

### Verification

- [x] YAML syntax validation: All workflow files pass YAML validation
- [x] No duplicate job names: Each workflow has unique job names
- [x] Path triggers configured: Docker files trigger ci.yml, domain-specific files trigger respective workflows

### Notes

The refactoring follows the "let dedicated workflows run independently" approach rather than using workflow_call. This is simpler and more maintainable because:

1. Each workflow has its own concurrency group
2. Path-based triggers are handled at the workflow level
3. No need for complex orchestration
4. Each workflow's ci-success job can be used for branch protection

**Workflow coverage after refactor:**

| Workflow | Triggers on | Jobs |
|----------|-------------|------|
| ci.yml | Dockerfile, docker-compose*.yml, .dockerignore | docker-build |
| ros2-ci.yaml | ros_ws/** | build-and-test |
| python-ci.yaml | training/** | lint, test |
| typescript-ci.yaml | web_frontend/** | lint, build, test |

---
