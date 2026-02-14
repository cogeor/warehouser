# Search: Action Space Design Patterns for Robotics RL

Created: 2026-02-12T22:30:00Z

## Query

Primary query: "robotics reinforcement learning action space design continuous discrete hybrid 2025"

Follow-up queries:
- "action space normalization scaling robotics RL sim-to-real action smoothing 2024 2025"
- "hierarchical reinforcement learning action abstraction levels skills primitives options 2024 2025"

## Findings

### 1. Hybrid Action Spaces

**Key Insight:** Discrete-continuous hybrid action spaces are natural in robotics (joint rotation is continuous, gripper clamping is discrete), but most RL methods only handle homogeneous action spaces.

**URL:** https://arxiv.org/html/2512.24651v1
- HMP-DRL (Dec 2025) uses "fine-grained discrete action space that still enables smooth motion" for collision avoidance
- Discrete actions can provide smooth control if designed correctly

**URL:** https://www.sciencedirect.com/science/article/abs/pii/S0925231223003028
- Naive approach: discretize continuous or continualize discrete (loses structure, scalability issues)
- Better approach: Learn compact latent representation (HyAR framework) that preserves hybrid structure
- Discretization loses fine-grained control advantage and creates vast operation spaces

**URL:** https://openreview.net/forum?id=64trBbOhdGU
- HyAR proposes "compact and decodable latent representation space" instead of conversion
- Maintains benefits of both discrete (strategic decisions) and continuous (fine control)

**URL:** https://arxiv.org/html/2510.26646v1
- Hybrid DQN-TD3 framework combines DQN for discrete subgoal selection with TD3 for continuous control
- Hierarchical integration: strategic discrete decision-making + precise continuous execution

### 2. Action Space Design for Sim-to-Real Transfer

**URL:** https://arxiv.org/html/2312.03673v1
- Comprehensive study of 13 control spaces across 250+ RL agents in sim and real-world
- **Key finding:** Joint velocity action spaces perform best for sim-to-real transfer
- Lowest tracking error (OTE), acceleration (ACC), and control variability (ECV)
- Delta action space scaling can control normalized tracking error
- Increasing stiffness can achieve similar control if task allows

**URL:** https://arxiv.org/html/2502.14457v1
- Transform Cartesian actions to joint velocities, then use joint impedance controller
- "Very good at handling non-smooth policy actions without introducing additional sim-to-real gap"
- Impedance control provides tolerance for suboptimal action predictions

### 3. Action Smoothing and Trajectory Processing

**URL:** https://arxiv.org/html/2312.03673v1
- Non-smooth RL action trajectories are a known problem
- Solutions: interpolators, cubic spline fitting
- Challenge: involves task-specific hyperparameters, no universal solution

**URL:** https://arxiv.org/html/2502.14457v1
- Motion-aware rewards encourage smooth motions while maintaining success rates
- Activated after policy completes main task (fine-tuning incentive)
- Prevents unnecessary motion or non-achievable target poses
- **Critical insight:** Learn manipulation as smooth continuous motion, not discrete waypoints

**URL:** https://arxiv.org/html/2511.04665v2
- Trajectory resampling and cubic spline interpolation for action normalization
- Addresses varying execution speeds across datasets
- Optical flow magnitude used to estimate execution speed

**URL:** https://arxiv.org/html/2512.01996
- FastSAC/FastTD3 achieves humanoid training in 15 minutes
- Minimalist reward functions and careful design choices
- Massively parallel simulation enables rapid iteration

### 4. Action Space Normalization Strategies

**URL:** https://apxml.com/courses/advanced-reinforcement-learning/chapter-8-rl-implementation-optimization/rl-space-representation
- Normalization crucial for stable policy learning and consistent performance
- Two schemes: mean-std (zero mean, unit variance) vs min-max (independent scaling)
- Observation normalization particularly helpful for off-policy RL

**URL:** https://arxiv.org/html/2312.03673v1
- Calculate difference between joint limits and default position
- Use as action bound for each joint
- Reduces need to tune action bounds during training

**URL:** https://arxiv.org/html/2506.04147
- SLAC uses simulation-pretrained latent action space
- Latent representations enable whole-body real-world RL
- Pre-training in simulation provides structured action manifold

### 5. Hierarchical RL and Action Abstraction Levels

**URL:** https://www.mdpi.com/2504-4990/4/1/9
- Options framework (Sutton et al.): fixed action sequences as macro actions
- Temporally extended runtime
- Skills: versatile behavioral primitives ("open door", "navigate to object")
- Seamless transition from fixed sequences to whole policies as options

**URL:** https://www.nature.com/articles/s41598-024-76719-w
- Hierarchical model-based RL (HMBRL) combines sample efficiency with abstraction
- Hierarchical world models simulate at various temporal abstraction levels
- Stack of agents with top-down goal communication
- Abstract actions should be lower dimensional than concatenated primitive sequences

**URL:** https://www.nature.com/articles/s41598-025-20653-y
- LLM-augmented HRL (2025) for long-horizon manipulation
- Action primitives as building blocks for diverse skills
- RL focuses on "what to do" instead of "how to do it"
- Speeds up learning by decomposing skill execution from skill selection

**URL:** http://staff.ustc.edu.cn/~zkan/Papers/Journals/%5B61%5D_2024CYB.pdf
- Primitive library: reach, grasp, push, release, atomic actions
- Parameterized action primitives provide versatility
- RL module selects primitive + parameters based on state and task features

**URL:** https://arxiv.org/pdf/2510.25634
- Continuous skill representation allows smooth interpolation between behaviors
- Advantages over discrete skills in manipulation and locomotion
- Temporal abstraction: control transferred to skill for K time steps

**URL:** https://thegradient.pub/the-promise-of-hierarchical-reinforcement-learning/
- Temporal/state abstractions increase sample efficiency
- Makes credit assignment less challenging
- Agent freed from reasoning about every individual step
- Humans use high-level skills, not fine-grained movements

### 6. Action Abstraction Methods

**URL:** https://www.nature.com/articles/s41598-024-76719-w
- K-means clustering of action sequence chunks
- Centroids updated via moving averages
- Makes higher-level actions discrete
- Avoids exploration challenges from high-dimensional continuous abstract action spaces

**URL:** https://rlj.cs.umass.edu/2025/papers/RLJ_RLC_2025_27.pdf
- Action mapping for continuous spaces
- Robotic arm end-effector pose tracking with obstacles
- Perfect feasibility model derivation possible

## Proposal: Recommendations for Warehouser

Based on this research, here are specific recommendations for Warehouser's action space design:

### 1. Action Space Architecture

**Current State:**
- Continuous actions: [linear_vel, angular_vel, pick, place]
- Pick/place are discrete but encoded as continuous [-1, 1]

**Recommendation:**
- **Keep hybrid approach** but refine the representation
- Consider threshold-based triggering for pick/place (e.g., >0.5 = activate)
- Alternative: Use multi-discrete output head for pick/place separate from continuous velocity head
- Implement action masking to prevent invalid pick/place actions

**Rationale:**
- Hybrid spaces are natural for warehouse robotics (navigation + manipulation)
- Separate heads preserve structure better than continualization
- Action masking improves sample efficiency by preventing impossible actions

### 2. Action Space Type: Joint Velocity Control

**Recommendation:**
- **Use joint velocity action space** (linear_vel, angular_vel) as currently designed
- Normalize to [-1, 1] then scale to robot's velocity limits
- Consider delta actions if fine position control needed

**Rationale:**
- Research shows joint velocity spaces have best sim-to-real transfer performance
- Lowest tracking error and control variability
- Current design already follows this best practice

### 3. Action Normalization

**Recommendation:**
- Normalize all continuous actions to [-1, 1] (already implemented)
- Use per-action bounds based on robot's physical limits
- Calculate action bounds as difference between joint limits and default position
- Apply symmetric bounds for velocity ([-max_vel, +max_vel])

**Example Implementation:**
```python
# In action processing
linear_vel = action[0] * MAX_LINEAR_VEL  # action[0] in [-1, 1]
angular_vel = action[1] * MAX_ANGULAR_VEL
pick_trigger = action[2] > 0.5  # threshold for discrete action
place_trigger = action[3] > 0.5
```

### 4. Action Smoothing and Rate Limiting

**Recommendation:**
- **Implement low-pass filter or exponential moving average** for velocity commands
- Add motion-aware reward terms (penalties for high acceleration/jerk)
- Use impedance control model if simulating realistic actuator dynamics

**Example:**
```python
# Exponential moving average smoothing
alpha = 0.3  # smoothing factor
smoothed_action = alpha * new_action + (1 - alpha) * prev_action
```

**Rationale:**
- RL policies produce non-smooth trajectories
- Smoothing prevents jerky motion and improves sim-to-real transfer
- Motion-aware rewards encourage naturally smooth behavior

### 5. Hierarchical Action Abstraction (Future Work)

**For Long-Horizon Tasks:**
- **Low-level (current):** Direct velocity commands + pick/place triggers
- **Mid-level (future):** Skills like "navigate_to_object", "pickup_object", "deliver_to_zone"
- **High-level (future):** Task planning with LLM-based goal decomposition

**Recommendation for Next Cycle:**
- Start with low-level actions (already implemented)
- Add skill primitives once base policy is stable
- Use options framework: each skill is a sub-policy with termination condition
- Continuous skill representation for smooth interpolation

**Primitive Library:**
- `navigate_to(x, y, theta)` - continuous parameters
- `pickup_object()` - executes until gripper closed or timeout
- `release_object()` - opens gripper
- `scan_area(radius)` - exploration primitive

### 6. Action Processing Pipeline

**Recommended Pipeline:**
```
Policy Output ([-1, 1])
  |
  v
Action Scaling (to physical limits)
  |
  v
Action Smoothing (EMA or low-pass filter)
  |
  v
Discrete Trigger Processing (threshold + action masking)
  |
  v
Safety Clipping (ensure bounds respected)
  |
  v
Send to Simulation/Robot
```

### 7. Exploration and Action Noise

**Recommendation:**
- PPO's stochastic policy provides exploration naturally
- For deterministic evaluation: add Ornstein-Uhlenbeck noise to actions
- Use action noise schedule: high early in training, decay over time
- Noise std proportional to action magnitude (relative noise)

**Example:**
```python
# During training
if exploration_mode:
    action += gaussian_noise(std=0.1 * (1 - training_progress))
```

### 8. Sim-to-Real Considerations

**Action Delay Modeling:**
- Add configurable action delay (e.g., 50-100ms)
- Simulate communication latency in training

**Motor Dynamics:**
- Model acceleration limits (not instantaneous velocity changes)
- Add actuator saturation and deadband zones

**Safety Bounds:**
- Hard-clip actions before sending to robot
- Implement velocity ramps for smooth acceleration
- Add emergency stop on unexpected behavior

**Action Noise:**
- Add Gaussian noise to executed actions in simulation
- Domain randomization: vary noise levels across episodes

### 9. Multi-Agent Extensions

**For Multi-Robot Warehousing:**
- Decentralized execution: each robot has independent action space
- Centralized training: shared parameters across robots
- Consider graph neural networks for robot-robot interactions
- Action coordination: add "wait" or "yield" discrete actions

## Summary

**Key Takeaways:**
1. Joint velocity action spaces are best for sim-to-real transfer
2. Hybrid discrete-continuous spaces should preserve structure (separate heads)
3. Action smoothing is critical for realistic motion
4. Normalize actions to [-1, 1], then scale to physical limits
5. Hierarchical abstractions enable long-horizon tasks
6. Motion-aware rewards encourage smooth, transferable behavior

**Immediate Actions for Warehouser:**
- Verify action normalization is symmetric and bounded
- Implement action smoothing (EMA or low-pass filter)
- Add motion-aware reward terms (acceleration/jerk penalties)
- Consider separate output heads for pick/place vs velocity
- Add action masking to prevent invalid discrete actions

**Future Enhancements:**
- Skill primitive library with temporal abstraction
- Hierarchical policy with options framework
- LLM-augmented high-level planning
- Multi-agent coordination primitives
