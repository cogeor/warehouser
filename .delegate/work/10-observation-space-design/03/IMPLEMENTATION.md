## Task 03: Create evaluate.py script with frozen normalization

Completed: 2026-02-13T15:45:00

### Changes

- `training/training/scripts/evaluate.py`: Created new evaluation script that:
  - Accepts CLI args: `--model` (path to model.zip), `--episodes` (count), `--obs-dim` (observation dimension)
  - Loads VecNormalize stats from `{model_path}_vecnormalize.pkl` if available
  - Sets `env.training = False` to freeze running statistics during evaluation
  - Runs evaluation episodes with `deterministic=True` for action selection
  - Reports mean reward, std reward, success rate, mean episode length, std episode length
  - Follows project conventions: Pydantic-style dataclass for results, typed functions, informative error messages

### Verification

- [x] Syntax check: `python -m py_compile` passed
- [x] Type check: `mypy --ignore-missing-imports` passed with no errors
- [x] Unit tests: All 108 tests pass, no regressions

### Notes

- The script expects VecNormalize stats at `{model_stem}_vecnormalize.pkl` alongside the model file
- If VecNormalize stats are not found, the script logs a warning and runs evaluation without normalization
- Uses `Any` type annotation for `obs` variable to handle VecEnv's polymorphic return types
- The `EvaluationResult` dataclass provides a clean interface for results with a formatted `__str__` method

---
