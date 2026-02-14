# TASK: Fix All CI Issues and Add Pre-commit Hooks

Created: 2026-02-14
Status: In Progress

## Summary

Fix all CI pipeline failures and add pre-commit hooks to catch issues before pushing. Ensure all tests pass, lint checks succeed, and the CI pipeline reliably passes.

## Current CI Failures

### TypeScript CI
- **Build job**: Failing - missing ESLint config file
- **Lint & Type Check job**: Failing - ESLint can't find configuration
- **Unit Tests job**: Likely failing due to upstream failures

### Python CI
- **Lint & Type Check job**: Failing - 39 mypy type errors, ruff import/format errors
- **Unit Tests job**: Tests pass locally but CI may fail due to lint gate

### ROS2 CI
- **Build & Test (Jazzy)**: May have dependency or build issues

## Root Causes Identified

### 1. TypeScript - Missing ESLint Config
```
ESLint couldn't find a configuration file.
```
`package.json` has ESLint dependencies but no `.eslintrc.cjs` or `.eslintrc.json`.

### 2. Python - mypy Type Errors (39 errors)
- Missing type parameters for `ActionWrapper` generics in wrappers
- `comparison-overlap` errors in test assertions (ObservationVersion enum)
- `attr-defined` errors accessing `.low`/`.high` on `Space[Any]`
- Unused `type: ignore` comments in `pettingzoo_env.py`

### 3. Python - ruff Lint Errors
- Import blocks un-sorted in `tests/test_action_wrappers.py`, `tests/test_scripts.py`
- Lines too long (>100 chars) in `tests/test_config.py:152,210`

## Requirements

1. All CI jobs must pass (TypeScript, Python, ROS2)
2. Pre-commit hooks must catch lint/type issues before push
3. Local development workflow must match CI
4. Tests must pass both locally and in CI

## Deliverables

1. `.eslintrc.cjs` - ESLint configuration for TypeScript
2. Fix all mypy type errors in Python code
3. Fix all ruff lint errors in Python code
4. `.pre-commit-config.yaml` - Pre-commit hooks configuration
5. Verify all tests pass locally
6. Verify CI passes (via local simulation or push)

## Files to Modify

| File | Change |
|------|--------|
| `web_frontend/.eslintrc.cjs` | Create ESLint config |
| `training/training/wrappers/*.py` | Fix mypy type errors |
| `training/training/envs/pettingzoo_env.py` | Fix unused type: ignore |
| `training/tests/test_config.py` | Fix line length, comparison errors |
| `training/tests/test_action_wrappers.py` | Fix import sorting, type errors |
| `training/tests/test_scripts.py` | Fix import sorting |
| `training/tests/test_pettingzoo_env.py` | Fix attr-defined errors |
| `.pre-commit-config.yaml` | Create pre-commit config |

## Success Criteria

- [ ] `npm run lint` passes in web_frontend
- [ ] `npm run build` passes in web_frontend
- [ ] `npm test` passes in web_frontend
- [ ] `npx tsc --noEmit` passes in web_frontend
- [ ] `uv run ruff check .` passes in training
- [ ] `uv run ruff format --check .` passes in training
- [ ] `uv run mypy training tests --ignore-missing-imports` passes in training
- [ ] `uv run pytest tests/ -v --ignore=tests/integration` passes in training
- [ ] `pre-commit run --all-files` passes
- [ ] All CI workflows pass on push
