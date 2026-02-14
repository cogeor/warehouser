# Search

Created: 2026-02-12T22:00:00Z

## Query

"Weights and Biases reinforcement learning experiment tracking robotics 2025"
"Stable-Baselines3 distributed training parallel environments Ray RLlib 2025"
"Optuna hyperparameter optimization reinforcement learning robotics curriculum learning 2025"

## Findings

### 1. Experiment Tracking: Weights & Biases for RL in Robotics

**Key Insights:**

- **W&B Adoption in Robotics**: OpenAI's Robotics team uses W&B Reports as their primary means of sharing results. The ability to mix real data from experiments with context and commentary replaced their previous workflow of creating Google Docs and manually linking to experimental data.

- **RL Compute Requirements**: For robotics work, Reinforcement Learning to learn an end-to-end policy requires approximately 30x more compute than Behavioral Cloning when both use a pretrained vision sub-model.

- **RSL-RL Integration**: The RSL-RL robotics library supports both Weights & Biases and Neptune for experiment logging. These are excellent choices for training on compute clusters, allowing experiments to be monitored live online. However, they require user accounts and involve uploading data to cloud platforms.

- **W&B Core Capabilities**: Tracks every model, metric, and hyperparameter with just a few lines of code. Provides full visibility into AI workflows for reproducing results, debugging model performance, and optimizing faster in a single dashboard.

- **RLHF Integration**: W&B provides experiment tracking for RLHF (Reinforcement Learning from Human Feedback) with its Weave toolkit for LLM-specific analysis. Logging helps monitor training, visualize reward improvement, and log sample outputs for qualitative checking.

**Sources:**
- https://wandb.ai/wandb_fc/genai-research/reports/Observability-tools-for-reinforcement-learning--VmlldzoxNDE3MzExMw
- https://wandb.ai/site/customers/learning-dexterity-end-to-end-using-weights-biases-reports/
- https://wandb.ai/site/experiment-tracking/
- https://arxiv.org/html/2509.10771v1

### 2. Distributed Training: Stable-Baselines3 vs Ray RLlib

**Key Insights:**

- **SB3 Positioning**: Stable-Baselines3 is a "user-friendly and reliable PyTorch-based framework for single-machine reinforcement learning experiments." It is research-oriented, offering flexibility for custom algorithms and experiments.

- **RLlib Positioning**: Ray RLlib is "a scalable, distributed reinforcement learning library that supports a wide array of algorithms and multi-agent settings." Built on the Ray framework, it supports multi-agent setups, hyperparameter optimization, and large-scale parallel training across clusters.

- **2025 Enterprise Recommendations**: RLlib, TensorFlow Agents, and ReAgent are suited for large-scale, production-grade RL deployments. RLlib excels in distributed training with features like fault tolerance and Kubernetes integration.

- **Integration Patterns**: pyRDDLGym-rl provides wrappers for both Stable Baselines 3 (>= 2.2.1) and RLlib (ray[rllib] >= 2.9.2) to work with the same environments. Some projects offer compatibility with both gym.Env and RLlib's MultiAgentEnv.

- **Migration Consideration**: Main difference when transitioning from SB3 to RLlib: with SB3, environments registered with gym can be found via `gym.make()`. In Ray, you must call the class that creates the environment directly.

- **Production Use**: Companies like Amazon and Anthem use RLlib for real-world applications. While it has a steeper learning curve than SB3, its flexibility and performance make it a top choice for enterprise-level RL projects.

**Sources:**
- https://discuss.ray.io/t/issues-reproducing-stable-baselines3-ppo-performance-with-rllib/3028
- https://apxml.com/courses/advanced-reinforcement-learning/chapter-8-rl-implementation-optimization/rl-frameworks-libraries
- https://pyrddlgym.readthedocs.io/en/latest/sb.html
- https://www.devopsschool.com/blog/top-10-reinforcement-learning-tools-in-2025-features-pros-cons-comparison/
- https://discuss.ray.io/t/from-stable-baselines3-to-ray-rl/6342

### 3. Hyperparameter Optimization: Optuna for RL

**Key Insights:**

- **Optuna Core Design**: Automatic hyperparameter optimization framework with imperative, define-by-run style user API. Particularly designed for machine learning and integrates well with Stable-Baselines3.

- **RL Sensitivity**: In reinforcement learning, hyperparameters affect the policy and reward approximations. With better-tuned results, hyperparameter optimization can drastically improve performance.

- **Curriculum Learning with Optuna (2025)**: Recent research addresses Probabilistic Curriculum Learning (PCL) with Optuna. PCL is a strategy designed to improve RL performance by structuring the agent's learning process. Using AlgOS framework integrated with Optuna's Tree-Structured Parzen Estimator (TPE), researchers refined hyperparameter search spaces to enhance optimization efficiency.

- **Robotics Applications**: Optuna is used for real-time applications in autonomous systems for robotics, supporting decision making in dynamic environments. Also exploited for self-driving cars to optimize models for safe navigation in complex environments.

- **Latest Updates (2025-2026)**:
  - Optuna 4.7.0 released January 19, 2026
  - AutoSampler with full support for multi-objective and constrained optimization (October 2025)
  - Gaussian Process-Based Sampler (GPSampler) for constrained multi-objective optimization (September 2025)
  - Optuna 5.0 roadmap published May 2025

**Sources:**
- https://araffin.github.io/post/optuna/
- https://optuna.org/
- https://www.datacamp.com/tutorial/optuna
- https://arxiv.org/html/2504.06683
- https://optuna.readthedocs.io/

## Cloned

None. No reference repositories cloned for this research cycle.

## Proposal

### Immediate Recommendations for Warehouser

Based on this research, here are the recommended infrastructure improvements for the Warehouser RL training pipeline:

#### 1. Experiment Tracking (High Priority)

**Recommendation**: Integrate Weights & Biases for experiment tracking.

**Rationale**:
- Proven in robotics applications (OpenAI Robotics team uses it as primary tool)
- Lightweight integration with Stable-Baselines3 (just a few lines of code)
- Superior collaboration features compared to local TensorBoard
- Live monitoring capabilities essential for long-running training jobs
- Better reproducibility through automatic hyperparameter and metric logging

**Implementation**:
- Add W&B callback to SB3 PPO training loop
- Log episode rewards, policy losses, value losses, entropy
- Track hyperparameters (learning rate, batch size, gamma, etc.)
- Log custom metrics (success rate, collision rate, delivery time)
- Save policy checkpoints with W&B artifacts

**Alternative**: Keep TensorBoard as fallback for air-gapped environments.

#### 2. Hyperparameter Optimization (High Priority)

**Recommendation**: Implement Optuna for automated hyperparameter tuning.

**Rationale**:
- Direct integration with Stable-Baselines3 (shown in Antonin Raffin's blog)
- TPE (Tree-Structured Parzen Estimator) sampler works well for RL
- Can optimize multiple objectives (e.g., reward vs. training time)
- Recent 2025 research shows effectiveness with curriculum learning strategies
- Latest Optuna 4.7.0 supports constrained multi-objective optimization

**Implementation**:
- Create Optuna study for PPO hyperparameters
- Define objective function that trains for N timesteps and returns mean reward
- Optimize: learning_rate, batch_size, n_steps, gamma, gae_lambda, clip_range
- Use pruning to stop unpromising trials early
- Save best hyperparameters to YAML config file

**Key Hyperparameters to Tune**:
```python
learning_rate: [1e-5, 1e-3]
batch_size: [32, 64, 128, 256]
n_steps: [512, 1024, 2048]
gamma: [0.95, 0.99, 0.999]
gae_lambda: [0.9, 0.95, 0.99]
clip_range: [0.1, 0.2, 0.3]
```

#### 3. Distributed Training (Medium Priority)

**Recommendation**: Stay with Stable-Baselines3 for now, but prepare for Ray RLlib migration path.

**Rationale**:
- SB3 is sufficient for single-machine, single-robot training
- Warehouser already supports multi-robot observations (V3_MultiRobot)
- Migration to RLlib makes sense when:
  - Multi-agent training becomes priority (multiple robots cooperating/competing)
  - Training requires multi-node clusters
  - Production deployment needs fault tolerance

**Current SB3 Optimization**:
- Use `VecEnv` with `SubprocVecEnv` for parallel environment rollouts
- Leverage existing ROS2 multi-robot support for data collection
- GPU utilization for policy/value network updates

**Future RLlib Migration Path**:
- RLlib supports multi-agent scenarios natively
- Better for production deployments (fault tolerance, Kubernetes)
- Compatible with pyRDDLGym-rl patterns for environment wrapping

#### 4. Training Pipeline Best Practices

**Reproducibility**:
- Set all random seeds (Python, NumPy, PyTorch, environment)
- Version control all configs (YAML-based hyperparameter files)
- Log git commit hash with each experiment
- Save environment kwargs with checkpoints

**Checkpoint Management**:
- Save checkpoints every N timesteps (e.g., every 100k steps)
- Keep top-K checkpoints by evaluation performance
- Use W&B artifacts for checkpoint versioning
- Implement auto-cleanup of old checkpoints

**Early Stopping and Evaluation**:
- Periodic evaluation on separate test scenarios
- Stop training when evaluation reward plateaus (patience-based)
- Use curriculum learning: start simple, gradually increase difficulty
- Monitor success rate, not just cumulative reward

**Curriculum Learning Pattern**:
```python
# Phase 1: Navigation only (no pick/place)
# Phase 2: Single object delivery
# Phase 3: Multiple objects with obstacles
# Phase 4: Full warehouse scenario with traffic
```

#### 5. Model Management

**ONNX Export Pipeline**:
- Current implementation is good (export_onnx.py exists)
- Add ONNX validation: compare SB3 vs ONNX outputs on test cases
- Version ONNX models with W&B artifacts
- Include preprocessing/postprocessing in ONNX graph

**Deployment Workflow**:
```
1. Train with SB3 → save .zip checkpoint
2. Export to ONNX → validate outputs match
3. Upload to W&B artifacts → tag with version
4. Deploy to ROS2 inference node
5. A/B test: compare ONNX policy vs previous version
```

**A/B Testing in Simulation**:
- Run multiple policy versions in parallel robots
- Compare success rates, collision rates, efficiency
- Use statistical significance testing before deployment

#### 6. Robotics-Specific Patterns

**Training with ROS2 Simulation**:
- Current service-based architecture (RLStep/RLReset) is good
- Add parallel environment support: launch N ROS2 simulation instances
- Use Docker containers for isolated simulation environments
- Shared memory for faster observation transfer (future optimization)

**Sim-to-Real Considerations**:
- Domain randomization: add sensor noise models (already implemented)
- Action noise: add motor uncertainty
- Observation noise: lidar noise, position uncertainty
- Curriculum: train with increasing realism

**Parallel Environment Pattern**:
```python
# Launch N ROS2 simulation containers
# Each with unique ROS_DOMAIN_ID
# SubprocVecEnv connects to each via services
# Aggregate rollouts for batch updates
```

### Implementation Priorities

**Phase 1 (Immediate)**:
1. Integrate W&B for experiment tracking
2. Implement Optuna hyperparameter optimization
3. Add reproducibility features (seed management, config versioning)

**Phase 2 (Short-term)**:
1. Improve checkpoint management
2. Add evaluation callback with early stopping
3. Implement curriculum learning stages

**Phase 3 (Medium-term)**:
1. Parallel environment support with Docker
2. Enhanced ONNX validation pipeline
3. A/B testing framework in simulation

**Phase 4 (Long-term)**:
1. Evaluate Ray RLlib migration for multi-agent scenarios
2. Multi-node distributed training setup
3. Advanced sim-to-real transfer techniques

### Code Integration Examples

**W&B Integration**:
```python
import wandb
from wandb.integration.sb3 import WandbCallback

wandb.init(
    project="warehouser-rl",
    config={"learning_rate": 3e-4, "batch_size": 64},
    sync_tensorboard=True,
)

model = PPO("MultiInputPolicy", env, verbose=1, tensorboard_log=f"runs/{run.id}")
model.learn(
    total_timesteps=1_000_000,
    callback=WandbCallback(model_save_path=f"models/{run.id}", verbose=2),
)
```

**Optuna Integration**:
```python
import optuna
from stable_baselines3 import PPO

def objective(trial):
    lr = trial.suggest_float("learning_rate", 1e-5, 1e-3, log=True)
    batch_size = trial.suggest_categorical("batch_size", [32, 64, 128, 256])

    model = PPO("MultiInputPolicy", env, learning_rate=lr, batch_size=batch_size)
    model.learn(total_timesteps=100_000)

    mean_reward, _ = evaluate_policy(model, eval_env, n_eval_episodes=10)
    return mean_reward

study = optuna.create_study(direction="maximize")
study.optimize(objective, n_trials=50)
```

### References

This research drew from current 2025-2026 best practices in:
- Production RL deployments (OpenAI Robotics, Amazon, Anthem)
- Academic research (curriculum learning, hyperparameter optimization)
- Open-source frameworks (SB3, Ray RLlib, Optuna, W&B)
- Robotics-specific RL libraries (RSL-RL, pyRDDLGym-rl)

All recommendations align with Warehouser's existing architecture (ROS2, SB3, ONNX pipeline) while providing clear upgrade paths for scaling.
