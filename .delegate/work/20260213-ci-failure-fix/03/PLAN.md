# PLAN: Create Dedicated Python CI Workflow

## Objective

Create a dedicated Python CI workflow file for the training module that:
1. Runs pytest for unit tests
2. Runs mypy for type checking
3. Uses uv for dependency management
4. Triggers only on changes to training/** paths

## Analysis

### Current State

The existing `ci.yml` already has Python jobs (`python-lint` and `python-test`) that:
- Use `astral-sh/setup-uv@v4` for uv installation
- Run on ubuntu-24.04
- Install Python 3.12 via uv
- Install dependencies with `uv pip install -e ".[dev]"`
- Run ruff check, ruff format, mypy, and pytest

### Gap

The current CI runs on all pushes/PRs to main, regardless of which files changed. A dedicated Python CI workflow would:
- Only trigger on training/** path changes
- Be more focused and potentially faster to iterate on
- Allow independent Python-specific CI configuration

## Tasks

### Task 1: Create python-ci.yaml workflow

**Files to create:**
- `.github/workflows/python-ci.yaml`

**Implementation:**
1. Create workflow with path-based triggers for `training/**`
2. Include two jobs: `lint` and `test`
3. Lint job runs: ruff check, ruff format check, mypy
4. Test job runs: pytest with coverage
5. Use concurrency settings to cancel in-progress runs
6. Use uv for all Python/dependency operations

**Verification:**
- Workflow YAML is syntactically valid
- Path triggers correctly specified
- All required steps present
