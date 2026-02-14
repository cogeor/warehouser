# Search: Observation Space Design for Robotics RL

Created: 2026-02-12T22:13:01Z

## Query

"observation space design robotics reinforcement learning best practices ego-centric sensor encoding 2024 2025"

## Findings

### 1. Observation Space Design Fundamentals

**Key Insight**: Designing observation spaces for RL requires careful design of a state-space that provides the needed information and is a reliable model of the true environment representing its features and constraints. The state space is the input for the RL agent, so the policy should be designed accordingly.

**Sources**:
- [Deep Reinforcement Learning for Zero-Shot Coverage Path Planning With Mobile Robots](https://www.ieee-jas.net/en/article/doi/10.1109/JAS.2024.125064) - Unified framework with observation space design that accommodates different map sizes, action masking for safety, and size-invariant value functions
- [Introduction to Reinforcement Learning – A Robotics Perspective](https://lamarr-institute.org/blog/reinforcement-learning-and-robotics/) - Overview of observation spaces in robotics (continuous actions vs discrete)

### 2. Ego-Centric vs World-Centric Observations

**Key Insight**: Modern robotics RL emphasizes ego-centric observations for end-to-end navigation. Models process first-person images, integrate observations into latent memory, and allow online usage in novel environments without preconstructed maps. The objective is to perceive the environment from an egocentric perspective and continually learn in dynamic, unstructured settings.

**Trade-offs**:
- **Ego-centric**: Better for sim-to-real transfer, more realistic (only sensor-available data), natural for embodied intelligence
- **World-centric**: Easier to learn initially, requires privileged information (ground truth positions)

**Sources**:
- [An Overview of Robot Embodied Intelligence Based on Multimodal Models](https://onlinelibrary.wiley.com/doi/10.1155/int/5124400) - Embodied intelligence and egocentric perception
- [Reinforcement learning for end-to-end UAV slung-load navigation and obstacle avoidance](https://www.nature.com/articles/s41598-025-18220-6) - End-to-end navigation with ego-centric inputs

### 3. State Representation Spectrum

**Key Insight**: A spectrum of state representations exists with varying structural priors:
- **Unstructured**: Pixels, latent embeddings
- **Structured**: Particles, keypoints, object-centric representations

Increasing structure introduces stronger priors and abstraction, enabling better generalization but requiring more design consideration. Interpretability is crucial - for representations like pixels, particles, and keypoints, visualizing predicted trajectories is natural, making it simple to diagnose failure cases.

**Warehouser Context**: V1 (positions) is highly structured, V2 (lidar) is semi-structured, V3 (multi-robot) adds relational structure.

**Sources**:
- [Frontiers | A survey on autonomous environmental monitoring approaches](https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2024.1336612/full) - State representation design considerations

### 4. Sensor Fusion and Multi-Modal Observations

**Key Insight**: Sensor fusion integrates data from multiple sensors to provide comprehensive and reliable environmental perception. The observation space should combine:
- **LiDAR readings**: Environmental geometry
- **Goal direction**: Task context
- **Kinematic states**: Robot velocity, orientation

This yields a comprehensive representation of both the environment and the agent itself. Sensor fusion maximizes the strengths of different sensors while mitigating their weaknesses.

**Best Practice**: Use domain-invariant observations (elevation maps, relative goal position/orientation) rather than raw sensory data for better sim-to-real transfer.

**Sources**:
- [Deep Reinforcement Learning for Sim-to-Real Robot Navigation with a Minimal Sensor Suite](https://www.mdpi.com/2076-3417/15/19/10719) - Observation space with LiDAR, goal direction, and kinematic states
- [Adaptive Navigation in Collaborative Robots: A Reinforcement Learning and Sensor Fusion Approach](https://www.mdpi.com/2571-5577/8/1/9) - Sensor fusion techniques
- [Hybrid Mode Sensor Fusion for Accurate Robot Positioning](https://pmc.ncbi.nlm.nih.gov/articles/PMC12115087/) - Multi-sensor integration

### 5. LiDAR Observation Encoding

**Observation Not Found in Search**: While sensor fusion approaches were documented, specific LiDAR encoding strategies (raw rays, discretized bins, image-like representations) were not detailed in the 2024-2025 research reviewed.

**Warehouser Context**: V2 observation uses discretized lidar bins - should verify this aligns with common practices or if raw lidar rays are preferred.

### 6. Normalization Techniques

**Key Insight**: Input normalization is essential to successful training of RL agents. Stable-Baselines3 provides `VecNormalize` wrapper that computes running average and standard deviation of input features.

**Implementation Details**:
```python
# Normalization formula
np.clip((obs - obs_rms.mean) / np.sqrt(obs_rms.var + epsilon), -clip_obs, clip_obs)
```

**Requirements**:
- Only supports `gym.spaces.Box` observation spaces
- For `Dict` observation spaces, explicitly pass `norm_obs_keys` parameter to specify which keys to normalize
- Required for environments with non-image inputs (e.g., PyBullet environments)

**Best Practices**:
- Always normalize input to the agent when applying RL to custom problems
- Look at common preprocessing done on other environments (frame-stack for Atari, normalization for continuous control)
- Images are scaled by default, but other input types are not

**Sources**:
- [Vectorized Environments — Stable Baselines3 Documentation](https://stable-baselines3.readthedocs.io/en/master/guide/vec_envs.html) - VecNormalize wrapper
- [stable_baselines3.common.vec_env.vec_normalize](https://stable-baselines3.readthedocs.io/en/master/_modules/stable_baselines3/common/vec_env/vec_normalize.html) - Implementation details
- [Reinforcement Learning Tips and Tricks — Stable Baselines3](https://stable-baselines3.readthedocs.io/en/v0.11.1/guide/rl_tips.html) - Normalization best practices

### 7. Frame Stacking and Temporal Information

**Key Insight**: Vectorized environments are required when using wrappers for frame-stacking or normalization. VecEnvWrapper can be used to stack multiple frames, monitor the environment, normalize observations, etc.

**Warehouser Context**: Currently not using frame stacking - may need to add temporal dimension to capture velocity information or handle partial observability.

**Sources**:
- [Vectorized Environments — Stable Baselines3](https://stable-baselines3.readthedocs.io/en/master/guide/vec_envs.html) - Frame stacking wrappers

### 8. Partially Observable Markov Decision Process (POMDP)

**Key Insight**: Humanoid-Gym framework employs a reinforcement learning model designed for both simulated and real-world settings, transitioning from full observability in simulations. This necessitates operating within a Partially Observable Markov Decision Process (POMDP).

**Implication**: Real-world deployment often requires handling partial observability. Training should account for this by:
- Using sensor-realistic observations during training
- Incorporating history (frame stacking, recurrent policies)
- Avoiding privileged information

**Sources**:
- [Humanoid-Gym: Reinforcement Learning for Humanoid Robot with Zero-Shot Sim2Real Transfer](https://arxiv.org/html/2404.05695v1) - POMDP framework for sim-to-real

### 9. Privileged Information and Sim-to-Real Transfer

**Key Insight**: Studies identify five key elements for robust RL-based control policies for real-world zero-shot deployment across input space design, reward design, system identification, and training techniques.

**Privileged Information Patterns**:
- Time vectors used as privileged information during training
- Domain randomization enhances policy robustness by introducing variations in simulated environments
- System identification (SysID) techniques estimate physical parameters (mass, inertia, actuator response) using real sensor feedback

**Sim-to-Real Strategies**:
1. **Domain Randomization**: Introduce variations in simulation to generalize to real-world scenarios
2. **Domain Adaptation**: Align feature distributions between simulated and real domains
3. **Sensor Fusion**: Provide comprehensive and reliable environmental perception
4. **Domain-Invariant Observations**: Use elevation maps, relative goal position/orientation instead of raw sensory data

**Critical Mistake**: V1 observation using ground truth positions is unrealistic for real deployment - V2 (lidar-based) is correct direction.

**Sources**:
- [What Matters in Learning A Zero-Shot Sim-to-Real RL](https://nicsefc.ee.tsinghua.edu.cn//nics_file/pdf/2bcdb470-f63c-49fd-8d1f-69be970ce82d.pdf) - Five key elements for sim-to-real
- [Real-world humanoid locomotion with reinforcement learning](https://www.science.org/doi/10.1126/scirobotics.adi9579) - Domain randomization and system identification
- [SIM2REAL: How to Reduce the Reality Gap in Robotics](https://www.reinforcementlearningpath.com/sim2real) - Comprehensive sim-to-real techniques

### 10. Goal Representation

**Key Insight**: Goal representation in observation space typically includes:
- Robot's **distance to goal location**
- Robot's **orientation relative to goal** (heading)
- Elevation map (for navigation)

These are domain-invariant and do not include raw sensory data, making them suitable for sim-to-real transfer.

**Sources**:
- [Deep Reinforcement Learning for Sim-to-Real Robot Navigation](https://www.mdpi.com/2076-3417/15/19/10719) - Goal representation pattern

### 11. Emerging Research Directions (2024-2025)

**Vision-Language-Action (VLA) Models**: Datasets like ALFRED, RLBench, and CALVIN introduce longer-horizon tasks with richer sensory streams combining RGB, depth, proprioception, and natural language instructions.

**Benchmarks**:
- BOSS: Benchmark for Observation Space Shift in Long-Horizon Tasks (2025)
- Point Cloud Matters: Rethinking the Impact of Different Observation Spaces on Robot Learning (2024)

**Phys2Real**: Fuses VLM-inferred priors over physical parameters with online ensemble of adaptation models using Bayesian fusion for policy conditioning.

**Sources**:
- [A curated list of embodied AI research](https://github.com/jonyzhang2023/awesome-embodied-vla-va-vln) - VLA models and datasets
- [CoRL 2025 papers](https://github.com/smallfryy/corl-2025-papers) - 200+ robotics papers
- [The duality of generative AI and reinforcement learning in robotics](https://www.sciencedirect.com/science/article/pii/S1566253525010656) - Emerging trends
- [Multimodal fusion with vision-language-action models for robotic manipulation](https://www.sciencedirect.com/science/article/pii/S1566253525011248) - VLA applications

## Cloned

None - no reference repositories cloned during this search.

## Proposal: Recommendations for Warehouser

Based on this research, here are actionable recommendations for Warehouser's observation space design:

### 1. Observation Space Architecture

**Current State**:
- V1: Ground truth positions (privileged information - bad for sim-to-real)
- V2: Lidar-based observations (good direction)
- V3: Multi-robot observations

**Recommendations**:
- **Phase out V1** for any sim-to-real aspirations
- **Focus on V2/V3** as the primary observation space
- Ensure all observations are **sensor-realistic** (no privileged information)

### 2. Sensor Fusion Design

**Implement comprehensive observation space**:
```python
observation = {
    'lidar': lidar_readings,           # Environmental geometry
    'goal_distance': float,            # Scalar distance to goal
    'goal_direction': float,           # Angle to goal (ego-centric)
    'robot_velocity': [vx, vy, omega], # Kinematic state
    'robot_orientation': theta,        # Robot heading (if needed)
}
```

All coordinates should be **ego-centric** (relative to robot frame).

### 3. Normalization Strategy

**Implement VecNormalize**:
```python
from stable_baselines3.common.vec_env import VecNormalize

env = VecNormalize(
    env,
    norm_obs=True,           # Normalize observations
    norm_reward=True,        # Normalize rewards
    clip_obs=10.0,          # Clip normalized obs to [-10, 10]
    clip_reward=10.0,       # Clip normalized reward
    norm_obs_keys=['lidar', 'robot_velocity']  # For Dict spaces
)
```

**Critical**: Save normalization statistics with the model for deployment.

### 4. Temporal Information

**Consider adding temporal dimension**:
- **Frame stacking** (4 frames) to capture velocity implicitly
- Or explicitly include velocity in observation space (preferred for interpretability)
- For POMDP handling, consider **recurrent policies** (LSTM/GRU) if needed

### 5. LiDAR Encoding (Research Gap)

**Current approach**: Discretized lidar bins
**Recommendation**: Validate this choice or experiment with:
- Raw lidar rays (if computational budget allows)
- Image-like representations (if using CNN policies)
- Point cloud representations (emerging approach)

### 6. Domain Randomization

**Implement in V2/V3 observations**:
- Add sensor noise to lidar readings
- Randomize lidar ray dropout (simulate occlusions)
- Randomize goal position representations
- Add noise to velocity estimates

This is critical for sim-to-real transfer robustness.

### 7. Goal Representation

**Current approach**: Unclear from codebase review
**Recommendation**: Use **domain-invariant** goal representation:
- Relative distance (scalar)
- Relative angle (scalar, ego-centric)
- Avoid absolute coordinates

### 8. Observation Space Testing

**Validation checklist**:
- [ ] All observations are sensor-realistic (no ground truth positions)
- [ ] Observations are ego-centric (robot frame)
- [ ] Normalization is applied (VecNormalize)
- [ ] Domain randomization is enabled for sensors
- [ ] Observation space is documented in code
- [ ] Normalization statistics are saved with model

### 9. Future Enhancements

**Long-term considerations**:
- Explore **object-centric representations** for task objects
- Consider **vision-language goals** for natural language task specification
- Investigate **point cloud observations** as alternative to discretized lidar
- Add **elevation maps** if multi-level warehouses are planned

### 10. Documentation Needs

**Create documentation for**:
- Observation space design rationale
- Coordinate frame conventions (ego-centric vs world)
- Normalization strategy and statistics
- Sensor noise models and domain randomization parameters

---

## Summary

The research strongly validates Warehouser's move from V1 (ground truth positions) to V2 (lidar-based observations). Key priorities:

1. **Normalization**: Implement VecNormalize immediately
2. **Ego-centric observations**: Ensure all observations are in robot frame
3. **Domain randomization**: Add sensor noise for sim-to-real robustness
4. **Goal representation**: Use relative distance/angle (domain-invariant)
5. **Temporal information**: Consider frame stacking or velocity observations
6. **Documentation**: Document coordinate frames and design rationale

The observation space is the foundation for sim-to-real transfer. Getting this right now will save significant effort later.
