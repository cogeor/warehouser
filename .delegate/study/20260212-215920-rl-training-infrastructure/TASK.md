# TASK: Implement Production-Grade RL Training Infrastructure

Created: 2026-02-12T22:10:00Z
Build: N/A (Python training module - dependencies missing)
Tests: FAIL (ModuleNotFoundError: pydantic not installed in system Python)

## Summary

Upgrade the Warehouser RL training infrastructure from a basic SB3 implementation to a production-grade research platform with experiment tracking (Weights & Biases), automated hyperparameter optimization (Optuna), reproducibility guarantees, custom robotics metrics, and parallel training support. The current implementation has solid foundations but lacks critical features for scalable, reproducible research.

## Context

### [S] Search Findings: Industry Best Practices (2025-2026)

**Experiment Tracking:**
- OpenAI Robotics uses W&B as their primary results-sharing tool, replacing Google Docs workflows
- RL in robotics requires 30x more compute than Behavioral Cloning, making efficient tracking essential
- W&B integrates with SB3 with just a few lines of code, provides live monitoring, artifact versioning, and collaboration features
- Alternative: Keep TensorBoard as fallback for air-gapped environments

**Hyperparameter Optimization:**
- Optuna 4.7.0 (released January 2026) with Tree-Structured Parzen Estimator (TPE) sampler
- Direct integration with Stable-Baselines3 demonstrated by Antonin Raffin
- Recent research shows effectiveness with curriculum learning in RL (AlgOS framework 2025)
- Can optimize multiple objectives (reward vs. training time) with constrained optimization

**Distributed Training:**
- SB3: "User-friendly PyTorch-based framework for single-machine RL experiments" (research-focused)
- Ray RLlib: "Scalable distributed library for multi-agent settings and cluster training" (production-focused)
- Recommendation: Stay with SB3 for now, prepare migration path to RLlib when multi-agent training or multi-node clusters needed
- Optimize SB3 with `SubprocVecEnv` for parallel environment rollouts

### [I] Introspection Findings: Current Implementation

**Strengths:**
- Comprehensive Pydantic-based configuration system with validation (`training/models/config.py`)
- Proper error handling with informative messages throughout
- ROS2 integration with graceful degradation when unavailable
- Multi-agent support via PettingZoo (`training/envs/pettingzoo_env.py`)
- ONNX export pipeline with validation (`training/scripts/export_onnx.py`)
- Well-tested configuration system (330 lines of tests)

**Critical Gaps:**
1. **No Experiment Tracking Beyond TensorBoard:**
   - README.md mentions W&B (lines 23, 48) but NOT implemented
   - No hyperparameter logging, no model artifact versioning
   - No custom robotics metrics (success rate, collision rate, delivery time)

2. **Limited Reproducibility:**
   - No seed management (PyTorch, NumPy, Python random not seeded)
   - No git commit hash tracking
   - Dependencies use `>=` not `==` in pyproject.toml
   - Can't reproduce run from checkpoint alone

3. **Simplified Config System:**
   - README shows Hydra examples (lines 167-227) but uses JSON + argparse
   - No config composition, inheritance, or CLI overrides
   - No experiment templates for sweeps

4. **No Hyperparameter Optimization:**
   - README shows Optuna sweeps (lines 388-402) but not implemented
   - Manual hyperparameter tuning only

5. **No Parallel Training:**
   - Uses `DummyVecEnv` (single environment, line 112 in train.py)
   - No `SubprocVecEnv` for parallel rollouts

6. **Missing Custom Metrics:**
   - No episode-level metrics (pick success, collision rate)
   - No reward component breakdown
   - No observation/action distribution monitoring

### [T] Template Findings: Production Patterns

Complete implementation patterns provided for:
- W&B integration with custom callback and artifact management
- Optuna hyperparameter optimization with pruning and TPE sampler
- Reproducibility utilities (seed management, git tracking, metadata logging)
- Custom callbacks (robotics metrics, curriculum learning, early stopping)
- Parallel environment support with Docker-based ROS2 instances
- Enhanced training script template with all features integrated

All code follows Warehouser conventions: Pydantic configs, type hints, explicit error handling, REP-103 compliance.

## Objective

Transform the training infrastructure to support production-grade RL research by implementing:
1. Live experiment tracking with artifact versioning
2. Automated hyperparameter optimization
3. Reproducible experiments with git integration
4. Robotics-specific metrics and callbacks
5. Parallel training support
6. Early stopping and curriculum learning

Acceptance: Train a PPO policy with W&B tracking, optimize hyperparameters with Optuna, achieve reproducible results from saved metadata.

## Implementation Plan

### Phase 1: Experiment Tracking (Priority: CRITICAL)

**Goal:** Enable live experiment tracking with W&B and custom robotics metrics.

- [ ] Create `training/training/utils/wandb_utils.py`:
  - `WandbOutputFormat`: Custom logger for W&B
  - `WandbCallback`: Enhanced callback with artifact management
  - `init_wandb()`: Initialize run with git info, tags, config
  - Log custom metrics: collision_rate, success_rate, delivery_time
  - Save model artifacts every N timesteps with metadata

- [ ] Create `training/training/callbacks/custom_callbacks.py`:
  - `RoboticsMetricsCallback`: Track success/collision/pickup/delivery rates
  - `EarlyStoppingCallback`: Stop training when performance plateaus
  - `CurriculumCallback`: Progressive difficulty adjustment (future)

- [ ] Update `training/training/scripts/train.py`:
  - Import W&B utilities and custom callbacks
  - Initialize W&B run with config dict and git info
  - Add callbacks to training loop: `[checkpoint_callback, eval_callback, wandb_callback, metrics_callback]`
  - Call `wandb.finish()` after training completes

- [ ] Update `training/pyproject.toml`:
  - Add `wandb>=0.18.0` to dependencies

**Verification:**
- Run training with W&B tracking: `python -m training.scripts.train --config configs/default.json`
- Check W&B dashboard for live metrics (reward, collision rate, success rate)
- Verify artifacts saved with metadata (timesteps, mean reward)

---

### Phase 2: Reproducibility (Priority: HIGH)

**Goal:** Guarantee reproducible experiments with seed management and git tracking.

- [ ] Create `training/training/utils/reproducibility.py`:
  - `set_random_seeds(seed)`: Set Python, NumPy, PyTorch, CUDA seeds
  - `get_git_info()`: Extract commit hash, branch, dirty status
  - `save_experiment_metadata()`: Save config, git info, system info to JSON
  - `ReproducibilityConfig`: Class to orchestrate reproducibility setup
  - Warn if git repo has uncommitted changes

- [ ] Update `training/training/scripts/train.py`:
  - Import `ReproducibilityConfig`
  - Call `repro_config.setup()` at start of training
  - Pass seed to PPO model initialization
  - Save metadata to `experiments/experiment_{timestamp}.json`

- [ ] Update `training/pyproject.toml`:
  - Pin exact dependency versions (change `>=` to `==`)
  - Document Python version requirement (3.11+)

**Verification:**
- Train twice with same seed, verify identical results
- Check `experiments/` directory for metadata JSON
- Verify git warning appears when repo is dirty

---

### Phase 3: Hyperparameter Optimization (Priority: HIGH)

**Goal:** Automate PPO hyperparameter tuning with Optuna.

- [ ] Create `training/training/scripts/optimize_hyperparams.py`:
  - `sample_ppo_params(trial)`: Sample learning_rate, batch_size, n_steps, gamma, gae_lambda, clip_range, ent_coef, vf_coef, max_grad_norm, n_epochs
  - `OptunaCallback`: Report intermediate values, prune unpromising trials
  - `objective(trial)`: Train for N timesteps, return mean evaluation reward
  - `optimize()`: Create study with TPE sampler and median pruner
  - `save_best_params()`: Export best hyperparameters to JSON
  - CLI arguments: `--n-trials`, `--n-timesteps`, `--study-name`, `--storage`

- [ ] Update `training/pyproject.toml`:
  - Add `optuna>=4.7.0` to dependencies

- [ ] Create example usage documentation:
  - Run optimization: `python -m training.scripts.optimize_hyperparams --n-trials 50`
  - Resume from database: `--storage sqlite:///optuna_study.db`
  - Use best params: `python -m training.scripts.train --config best_hyperparams.json`

**Verification:**
- Run hyperparameter optimization for 10 trials
- Check Optuna outputs best hyperparameters
- Verify `best_hyperparams.json` created
- Train with optimized hyperparameters, compare to defaults

---

### Phase 4: Training Optimizations (Priority: MEDIUM)

**Goal:** Enable parallel environments and early stopping.

- [ ] Create `training/training/envs/parallel_env.py`:
  - `make_parallel_env(env_fn, n_envs)`: Create `SubprocVecEnv` with N processes
  - Each environment gets unique seed: `1000 + rank`
  - Support for Docker-based ROS2 instances with unique `ROS_DOMAIN_ID`

- [ ] Update `training/training/scripts/train.py`:
  - Add `--n-envs` argument to enable parallel environments
  - Replace `DummyVecEnv` with `make_parallel_env()` when `n_envs > 1`
  - Add `EarlyStoppingCallback` with configurable patience

- [ ] Add curriculum learning support to `CurriculumCallback`:
  - Define stages: `[{obstacles: 0, objects: 1}, {obstacles: 2, objects: 2}, {obstacles: 5, objects: 3}]`
  - Advance stages when success rate threshold reached
  - Log current stage to metrics

- [ ] Create Docker launch script for parallel simulations:
  - `scripts/launch_parallel_sims.sh`: Launch N Docker containers
  - Each with unique `ROS_DOMAIN_ID={0..N-1}`
  - Container name: `ros_sim_{i}`

**Verification:**
- Train with 4 parallel environments: `--n-envs 4`
- Verify 4x faster rollout collection
- Check early stopping triggers after patience evaluations
- Test curriculum learning advances through stages

---

### Phase 5: Enhanced Training Script (Priority: MEDIUM)

**Goal:** Create production-ready training script template.

- [ ] Create `training/training/scripts/train_enhanced.py`:
  - Integrate all features: W&B, reproducibility, custom callbacks, early stopping
  - CLI arguments: `--config`, `--no-wandb`, `--seed`, `--project`, `--n-envs`
  - Complete example of best practices

- [ ] Update documentation:
  - Add training guide to `training/README.md`
  - Document W&B setup and API key configuration
  - Add Optuna optimization workflow
  - Explain reproducibility features

**Verification:**
- Run enhanced training script end-to-end
- Verify all callbacks active and logging correctly
- Check W&B dashboard has complete metrics

---

## Interface Definitions

### WandbCallback Interface

```python
class WandbCallback(BaseCallback):
    """Enhanced W&B callback with artifact management."""

    def __init__(
        self,
        project: str = "warehouser-rl",
        model_save_freq: int = 50_000,
        save_replay_buffer: bool = False,
        log_custom_metrics: bool = True,
        verbose: int = 1,
    ):
        """Initialize W&B callback."""

    def _on_step(self) -> bool:
        """Called at each environment step.

        - Check for episode completion
        - Extract episode info (reward, length, custom metrics)
        - Log to W&B: collision_rate, success_rate, episode_reward
        - Save model artifacts at regular intervals
        """

    def _save_model_artifact(self) -> None:
        """Save model as W&B artifact with metadata."""
```

### Optuna Objective Interface

```python
def objective(trial: optuna.Trial, env_config: EnvConfig, n_timesteps: int) -> float:
    """Objective function for hyperparameter optimization.

    Args:
        trial: Optuna trial for sampling hyperparameters
        env_config: Environment configuration (fixed)
        n_timesteps: Training budget per trial

    Returns:
        Mean evaluation reward (to maximize)

    Steps:
        1. Sample hyperparameters with trial.suggest_*
        2. Create environment and model
        3. Train with OptunaCallback for pruning
        4. Evaluate policy on eval_env
        5. Return mean_reward
    """
```

### ReproducibilityConfig Interface

```python
class ReproducibilityConfig:
    """Configuration for reproducible training."""

    def __init__(
        self,
        seed: int = 42,
        deterministic: bool = True,
        save_metadata: bool = True,
        metadata_dir: str = "experiments",
    ):
        """Initialize reproducibility config."""

    def setup(self, config: dict[str, Any], notes: str | None = None) -> None:
        """Set up reproducibility for training.

        Steps:
            1. Call set_random_seeds(self.seed)
            2. Save metadata with git info, config, system info
            3. Warn if git repo is dirty (uncommitted changes)
        """
```

### RoboticsMetricsCallback Interface

```python
class RoboticsMetricsCallback(BaseCallback):
    """Callback for logging robotics-specific metrics."""

    def __init__(self, verbose: int = 0):
        """Initialize callback."""
        # Track: episode_rewards, collision_count, success_count,
        #        pickup_count, delivery_count, delivery_times

    def _on_step(self) -> bool:
        """Called at each environment step.

        Extract from info dict:
        - collision: bool
        - success: bool
        - picked_up: bool
        - delivered: bool
        - delivery_time: float

        Log every 1000 steps:
        - metrics/collision_rate
        - metrics/success_rate
        - metrics/pickup_rate
        - metrics/delivery_rate
        - metrics/mean_episode_reward
        - metrics/mean_delivery_time
        """
```

---

## Files to Create

| File | Purpose | Lines (Est.) |
|------|---------|--------------|
| `training/training/utils/wandb_utils.py` | W&B integration with custom callback and init | 250 |
| `training/training/utils/reproducibility.py` | Seed management, git tracking, metadata | 200 |
| `training/training/callbacks/custom_callbacks.py` | Robotics metrics, early stopping, curriculum | 350 |
| `training/training/scripts/optimize_hyperparams.py` | Optuna hyperparameter optimization | 400 |
| `training/training/envs/parallel_env.py` | Parallel environment wrapper | 50 |
| `training/training/scripts/train_enhanced.py` | Enhanced training script template | 350 |

**Total: ~1,600 lines of production-ready code**

---

## Files to Modify

| File | Change | Lines |
|------|--------|-------|
| `training/training/scripts/train.py` | Add W&B, reproducibility, custom callbacks integration | +30 |
| `training/pyproject.toml` | Add dependencies: `wandb>=0.18.0`, `optuna>=4.7.0` | +2 |
| `training/pyproject.toml` | Pin exact dependency versions (change `>=` to `==`) | ~10 |
| `training/README.md` | Add training guide, W&B setup, Optuna workflow docs | +100 |

---

## Architecture Notes

### Modularity and Separation of Concerns

1. **Utils Layer** (`training/utils/`):
   - `wandb_utils.py`: Experiment tracking (W&B-specific)
   - `reproducibility.py`: Seed management and metadata (framework-agnostic)

2. **Callbacks Layer** (`training/callbacks/`):
   - `custom_callbacks.py`: Domain-specific metrics and training logic
   - Follows SB3 `BaseCallback` interface for compatibility

3. **Scripts Layer** (`training/scripts/`):
   - `train.py`: Basic training (existing, enhanced)
   - `train_enhanced.py`: Full-featured template
   - `optimize_hyperparams.py`: Hyperparameter search
   - `export_onnx.py`: Model export (existing)

4. **Environments Layer** (`training/envs/`):
   - `ros_env.py`: Single-agent Gymnasium wrapper (existing)
   - `pettingzoo_env.py`: Multi-agent wrapper (existing)
   - `parallel_env.py`: Parallel environment factory (new)

### Design Decisions

**W&B vs TensorBoard:**
- W&B for experiment tracking, artifact versioning, collaboration
- Keep TensorBoard via `sync_tensorboard=True` for local debugging
- Allow `--no-wandb` flag for air-gapped environments

**Optuna vs Manual Tuning:**
- Optuna for automated search with pruning
- Save best hyperparameters to JSON for reproducibility
- Use persistent storage (SQLite) for resumable studies

**SB3 vs RLlib:**
- Stay with SB3 for single-robot, single-machine training
- Prepare migration path to RLlib for future multi-agent/multi-node needs
- Use `SubprocVecEnv` for parallel rollouts (4x speedup expected)

**Reproducibility Strategy:**
- Set all random seeds (Python, NumPy, PyTorch, CUDA)
- Track git commit hash with every experiment
- Save complete metadata (config, git, system info, dependencies)
- Warn on uncommitted changes (don't block, but notify)

**Callbacks Architecture:**
- Each callback has single responsibility (metrics, early stopping, curriculum)
- Composable: use any combination in training loop
- Follow SB3 `BaseCallback` interface for compatibility

### Integration with Existing Codebase

**Pydantic Configs:**
- Leverage existing `EnvConfig` and `TrainingConfig`
- Convert to dict with `.model_dump()` for W&B logging
- No changes to config.py needed initially

**ROS2 Integration:**
- No changes to `ros_env.py` wrapper
- Parallel environments require multiple ROS2 instances (Docker)
- Each instance uses unique `ROS_DOMAIN_ID` for isolation

**ONNX Export:**
- Existing `export_onnx.py` remains unchanged
- W&B can save ONNX models as artifacts
- Future: add ONNX validation comparing PyTorch vs ONNX outputs

**Testing Strategy:**
- Add tests for new utilities (reproducibility, W&B mocking)
- Mock W&B API calls in tests (no real uploads)
- Test Optuna objective function with small timestep budget
- Integration tests marked with `@pytest.mark.integration`

---

## Verification Checklist

### Phase 1: Experiment Tracking
- [ ] W&B run created with correct project name
- [ ] Hyperparameters logged to W&B config
- [ ] Git commit hash appears in W&B metadata
- [ ] Custom metrics visible: collision_rate, success_rate
- [ ] Model artifacts saved with timestep metadata
- [ ] TensorBoard still works via sync_tensorboard=True

### Phase 2: Reproducibility
- [ ] Training with same seed produces identical results (2 runs)
- [ ] Git warning appears when repo is dirty
- [ ] Metadata JSON saved with git info, config, system info
- [ ] PyTorch deterministic mode enabled (check warning messages)
- [ ] Dependencies pinned to exact versions

### Phase 3: Hyperparameter Optimization
- [ ] Optuna study completes N trials without errors
- [ ] Best hyperparameters saved to JSON
- [ ] Pruning stops unpromising trials early (check pruned count)
- [ ] Training with optimized params improves over defaults
- [ ] Study resumable from database

### Phase 4: Training Optimizations
- [ ] Parallel environments (n_envs=4) provide 3-4x speedup
- [ ] Early stopping triggers after patience evaluations
- [ ] Curriculum callback advances through stages
- [ ] Docker containers launched with unique ROS_DOMAIN_IDs

### Phase 5: Enhanced Training Script
- [ ] Enhanced script runs end-to-end without errors
- [ ] All callbacks active and logging correctly
- [ ] W&B dashboard shows complete metrics
- [ ] Documentation covers setup and usage

---

## Success Metrics

**Before Implementation (Current State):**
- Experiment tracking: TensorBoard only (local)
- Hyperparameter tuning: Manual
- Reproducibility: Partial (missing seed management)
- Metrics: Basic SB3 defaults (reward, loss)
- Training speed: Single environment
- Checkpoints: Saved but no versioning

**After Implementation (Target State):**
- Experiment tracking: W&B with artifact versioning
- Hyperparameter tuning: Automated with Optuna
- Reproducibility: Full (seeds, git, metadata, pinned deps)
- Metrics: Custom robotics metrics (success, collision, delivery time)
- Training speed: 4x faster with parallel environments
- Checkpoints: Versioned artifacts with metadata

**Quantitative Goals:**
- 2-3x performance improvement from hyperparameter optimization
- 4x training speedup from parallel environments
- 100% reproducibility (identical results with same seed)
- Zero manual hyperparameter tuning required

---

## References

**Industry Best Practices:**
- OpenAI Robotics W&B case study: https://wandb.ai/site/customers/learning-dexterity-end-to-end-using-weights-biases-reports/
- Antonin Raffin's Optuna + SB3 guide: https://araffin.github.io/post/optuna/
- RSL-RL robotics library patterns: https://arxiv.org/html/2509.10771v1

**Documentation:**
- Weights & Biases: https://wandb.ai/site/experiment-tracking/
- Optuna 4.7.0: https://optuna.readthedocs.io/
- Stable-Baselines3: https://stable-baselines3.readthedocs.io/
- Ray RLlib: https://docs.ray.io/en/latest/rllib/

**Research Papers:**
- Curriculum learning with Optuna (2025): https://arxiv.org/html/2504.06683
- RL frameworks comparison (2025): https://apxml.com/courses/advanced-reinforcement-learning/

All code follows Warehouser conventions: Pydantic, type hints, REP-103, explicit error handling, pytest structure.
