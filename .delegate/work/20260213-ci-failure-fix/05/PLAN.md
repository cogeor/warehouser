# Plan: Refactor Main CI Workflow

## Objective

Refactor `.github/workflows/ci.yml` to remove duplicate jobs now covered by dedicated workflows, keeping only the Docker build job and adding path-based triggers.

## Current State

- **ci.yml**: Contains ros2-build-test, python-lint, python-test, docker-build, and ci-success jobs
- **ros2-ci.yaml**: Standalone workflow with path triggers for `ros_ws/**`
- **python-ci.yaml**: Standalone workflow with path triggers for `training/**`
- **typescript-ci.yaml**: Standalone workflow with path triggers for `web_frontend/**`

## Approach

Since the dedicated workflows already run independently with their own triggers, the simplest refactor is:

1. Remove duplicate jobs from ci.yml (ros2-build-test, python-lint, python-test)
2. Keep docker-build job (not covered elsewhere)
3. Add path-based triggers for Docker-related files
4. Simplify ci-success to only check docker-build
5. Add workflow_dispatch for manual triggering

## Tasks

### Task 1: Refactor ci.yml

**Files to modify:**
- `.github/workflows/ci.yml`

**Changes:**
1. Update triggers to include path filters for Docker and workflow files
2. Remove ros2-build-test job (moved to ros2-ci.yaml)
3. Remove python-lint job (moved to python-ci.yaml)
4. Remove python-test job (moved to python-ci.yaml)
5. Keep docker-build job
6. Update ci-success job to only depend on docker-build
7. Add workflow_dispatch for manual runs

## Expected Outcome

- ci.yml only handles Docker builds
- Dedicated workflows handle their respective domains
- Path-based triggers reduce unnecessary CI runs
- Branch protection can use individual workflow success jobs

## Verification

1. YAML syntax validation
2. Confirm no duplicate job names across workflows
3. Verify path triggers are correctly configured
