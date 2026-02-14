## Task 02: Integrate VecNormalize into training pipeline

Completed: 2026-02-13

### Changes

- `training/training/scripts/train.py`: Added VecNormalize wrapper integration
  - Imported VecNormalize from stable_baselines3.common.vec_env
  - Wrapped DummyVecEnv with VecNormalize using train_config settings (norm_obs, norm_reward, clip_obs, clip_reward)
  - Wrapped eval_env with VecNormalize (training=False) and synced stats from training env
  - Added save logic to persist VecNormalize stats to `{model_path}_vecnormalize.pkl`
  - Added load logic for resume case to restore VecNormalize stats if file exists

### Verification

- [x] Syntax check: passed (py_compile)
- [x] Type check: passed (mypy --ignore-missing-imports)
- [x] Unit tests: 36 passed (pytest tests/ -v -k train)

### Notes

- The TrainingConfig already had VecNormalize settings (norm_obs, norm_reward, clip_obs, clip_reward) from a previous task
- Used proper type annotations to avoid mypy errors with VecNormalize reassignment
- VecNormalize stats are saved alongside the model with `_vecnormalize.pkl` suffix
- Resume case handles both `.zip` and non-`.zip` checkpoint paths
- If VecNormalize stats file is missing during resume, training continues with fresh stats and logs a warning

---
