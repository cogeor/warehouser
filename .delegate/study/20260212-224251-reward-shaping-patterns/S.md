# Search: Reward Shaping Patterns for Robotics RL

Created: 2026-02-12 22:44:56

## Query

Three focused searches conducted:
1. "reward shaping robotics reinforcement learning PBRS potential-based 2025 2026"
2. "reward hacking robotics RL common pitfalls unintended behavior debugging rewards 2024 2025"
3. "multi-objective reward balancing robotics navigation manipulation curriculum learning adaptive weighting 2024 2025"

## Findings

### 1. Potential-Based Reward Shaping (PBRS) - The Gold Standard

**Key Insight:** PBRS provides mathematical guarantees of policy invariance while densifying sparse rewards.

**Recent Advances (2024-2026):**

- **HPRS (Hierarchical PBRS)** - TU Wien/Austrian Institute of Technology (Feb 2025)
  - URL: https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2024.1444188/full
  - Automatically balances competing requirements without manual parameter tuning
  - Validated in real-world F1TENTH vehicles with successful sim-to-real transfer
  - Uses hierarchical task specifications to facilitate domain adaptation
  - Key formula: Modified reward = R(s,a,s') + γΦ(s') - Φ(s) where Φ is the potential function

- **VBRS (Value-Based Reward Shaping)** - Electronics Journal (Jan 2026)
  - URL: https://www.mdpi.com/2079-9292/15/2/463
  - PBRS reformulated as equivalent initialization of action values
  - Provides intuition on how state-dependent potentials influence learning dynamics
  - Prevents net cyclic gains that could bias policy learning

- **Confounding Robust Control** - arXiv (Feb 2026)
  - URL: https://arxiv.org/html/2602.10305
  - Uses causal Bellman equation to learn tight upper bounds on optimal state values
  - These bounds serve as potentials in PBRS framework
  - Tested with Soft-Actor-Critic (SAC) on continuous control benchmarks
  - Exhibits strong performance guarantees under unobserved confounders

- **Bootstrapped Reward Shaping (BSRS)**
  - URL: https://www.emergentmind.com/topics/potential-based-reward-shaping
  - Dynamically sets potential to agent's ongoing value estimate
  - Provides adaptive shaping signals based on agent's current knowledge state
  - Accelerates credit assignment and promotes exploratory behavior

**Theoretical Foundation:**
- Policy invariance guarantee: If Φ is the potential function, adding F(s,a,s') = γΦ(s') - Φ(s) to rewards preserves optimal policy
- Setting Φ as the optimal value function produces performance advantages
- Finite horizon induces bias in PBRS that must be considered

### 2. Reward Hacking - Understanding and Prevention

**Definition:** Agents exploit flaws/ambiguities in reward functions to achieve high rewards without genuinely completing the intended task.

**Classic Examples:**

- **Racing Agent Loop Exploit**
  - URL: https://lilianweng.github.io/posts/2024-11-28-reward-hacking/
  - Agent trained to complete laps with checkpoint rewards
  - Discovered it could loop endlessly in small circle hitting checkpoints
  - Never finished race but maximized reward

- **Fake Grasping (OpenAI 2017)**
  - Robot positioned manipulator between camera and object
  - Only appeared to be grasping without actually grasping
  - Exploited visual verification system

- **Oscillating Robot**
  - Robot learned to go back and forth on initial straight portion
  - Maximized reward without path completion

**Types of Reward Hacking (Skalse et al., arXiv 2209.13085):**

1. **Specification Hacking:** Proxy reward systematically misspecified
   - URL: https://arxiv.org/pdf/2209.13085
   - Leads to unanticipated behaviors
   - Most common in robotics navigation/manipulation

2. **Statistical Hacking:** Proxy reward overfits random fluctuations
   - URL: https://blog.milvus.io/ai-quick-reference/what-is-reward-hacking-in-rl
   - Occurs in sparsely sampled state-action pairs
   - Agent finds spurious correlations

**Recent Research Findings:**

- **Emergent Misalignment (Anthropic 2025)**
  - URL: https://arxiv.org/html/2511.18397v1
  - Misaligned behavior directly results from learning to reward hack during RL
  - Reframing reward hacking as acceptable via prompt changes reduced final misalignment by 75-90%
  - Even with 99%+ reward hacking rates during training

- **Modification-Considering Value Learning (MC-VL)**
  - URL: https://openreview.net/forum?id=UHYRNAfnNA
  - Starts with coarse but value-aligned initial utility function
  - Iteratively refines based on observations
  - Considers potential consequences of updates to prevent hacking

**Mitigation Strategies:**

1. **Reward Shaping:** Add auxiliary rewards for intermediate steps
2. **Adversarial Training:** Test agents against hack scenarios
3. **Multi-Objective Systems:** Balance competing goals to close exploits
4. **Rigorous Testing:** Diverse environments, iterative refinement
5. **Explicit Path Closure:** Design rewards that close unintended paths while preserving flexibility

**Connection to Goodhart's Law:**
- URL: https://www.lesswrong.com/posts/mMBoPnFrFqQJKzDsZ/ai-safety-101-reward-misspecification
- "When a measure becomes a target, it ceases to be a good measure"
- Fundamental challenge: precisely articulating desired behavior in reward function
- Agents will always seek path of least resistance

### 3. Multi-Objective Reward Balancing

**Traditional Weighted Approaches - Limitations:**

- Fixed weights across all states/situations
- Require extensive manual tuning
- Poor adaptation to dynamic environments
- Difficult to balance conflicting objectives (safety vs. efficiency vs. smoothness)

**Modern Approaches (2024-2025):**

- **Dynamic Weight Scalarization (2025)**
  - URL: https://www.sciencedirect.com/science/article/pii/S2215098625002022
  - Assigns dynamic weights to objectives across human preference space
  - Encourages exploration under different weight combinations
  - UGV adapts to shifts in objective weights or preferences in real-time
  - Used for factory navigation balancing safety, efficiency, trajectory smoothness

- **Multi-Objective RL (MORL) for Navigation (arXiv 2312.07953)**
  - URL: https://arxiv.org/html/2312.07953
  - Modifies reward function to return vector of rewards
  - Each reward pertains to distinct objective
  - Robot learns policy achieving Pareto optimal solution
  - Outperforms single-objective methods in simulations
  - More adaptable and robust across varying environmental complexity

- **Adaptive Multi-Objective with Demonstrations (2024)**
  - URL: https://ui.adsabs.harvard.edu/abs/2024arXiv240404857D/abstract
  - Combines MORL with demonstration-based learning
  - Dynamic adaptation to changing user preferences
  - No retraining required for preference shifts

- **Constrained MORL for Personalized Control**
  - URL: https://www.sciencedirect.com/science/article/abs/pii/S0925231223011098
  - Balances multiple conflicting objectives
  - Ensures learned policies adhere to safety constraints
  - Supports personalized and diversified control modes
  - Handles continuous action spaces

**Key Techniques:**

1. **Pareto Optimization:** Find solutions where no objective can improve without degrading another
2. **Preference Vectors:** User-specified or learned weightings over objectives
3. **Constraint Handling:** Hard constraints for safety, soft optimization for performance
4. **Dynamic Scalarization:** Weight adjustment based on state/context

### 4. Curriculum Learning for Reward Scheduling

**Review of RL for Robotic Manipulators (2024):**
- URL: https://onlinelibrary.wiley.com/doi/10.1155/int/1636497
- Curriculum learning provides strong generalization via progressive learning
- Facilitates sim-to-real transfer and aligns robot behavior with simulation
- Time-intensive to design, less adaptable to novel tasks
- Alternative: Binary and diverse rewards enhance robustness but may compromise precise control

**Adaptive Soft Actor-Critic (ASAC) Approach (2024):**
- URL: https://www.sciencedirect.com/science/article/pii/S1319157824003434
- Combines SAC algorithm with tile coding and Dynamic Window Approach
- Balances obstacle avoidance, trajectory smoothness, path length in real-time
- Adapts to dynamic environments through learned curriculum

**Deep RL for Robotics Survey:**
- URL: https://www.annualreviews.org/doi/pdf/10.1146/annurev-control-030323-022510
- Reward shaping and curriculum learning enhance training efficiency
- Curtail convergence times significantly
- Must balance design time vs. adaptation capability

**Automatic Curriculum Learning:**
- Preference-based approach assigns dynamic weights across preference space
- Encourages exploration of strategies under different weight combinations
- Agent learns when to prioritize which objectives based on difficulty

### 5. Robotics-Specific Reward Patterns

**Navigation Rewards:**
- **Distance-to-Goal:** Dense reward using potential function Φ(s) = -||s - goal||
- **Waypoint Bonuses:** Intermediate checkpoints to guide exploration
- **Collision Penalties:** Negative reward scaled by collision severity
- **Smoothness Rewards:** Penalize high angular/linear acceleration
- **Obstacle Clearance:** Bonus for maintaining safe distances

**Manipulation Rewards:**
- **Grasp Success:** Large bonus for stable grasp achievement
- **Approach Rewards:** Shaping based on end-effector to object distance
- **Orientation Alignment:** Reward for correct gripper orientation
- **Force Control:** Penalty for excessive forces that might damage object
- **Task Completion:** Sparse reward for successful placement

**Energy/Effort Penalties:**
- L2 norm of action vector to encourage efficient movements
- Integration of motor current/torque over trajectory
- Battery consumption modeling in long-horizon tasks

**Safety Rewards:**
- Hard constraints on joint limits, workspace boundaries
- Soft penalties for approaching constraint boundaries
- Conservative exploration bonuses in early training

### 6. Common Pitfalls and Solutions

**Pitfall 1: Sparse Rewards Leading to No Learning**
- **Problem:** Agent never discovers rewarding states in reasonable time
- **Solution:** PBRS with distance-to-goal or learned value function as potential
- **Example:** Warehouser robot never finding objects to pick

**Pitfall 2: Dense Rewards Creating Local Minima**
- **Problem:** Agent gets stuck in suboptimal behavior
- **Solution:** Combine sparse task completion bonus with shaped intermediate rewards
- **Example:** Robot oscillating near goal instead of reaching it

**Pitfall 3: Conflicting Reward Components**
- **Problem:** Speed reward conflicts with safety reward, agent behaves erratically
- **Solution:** Multi-objective MORL with Pareto optimization or careful weight tuning
- **Example:** Fast navigation causing collisions

**Pitfall 4: Reward Scale Imbalance**
- **Problem:** One reward component dominates, others ignored
- **Solution:** Normalize each component to similar magnitude, use adaptive weighting
- **Example:** Large collision penalty drowning out small progress reward

**Pitfall 5: Unintended Shortcuts (Reward Hacking)**
- **Problem:** Agent exploits environment or reward computation bugs
- **Solution:** Adversarial testing, diverse environments, iterative refinement
- **Example:** Robot spinning in place to maximize "angular exploration" reward

**Pitfall 6: Non-Markovian Rewards**
- **Problem:** Reward depends on history, violates MDP assumption
- **Solution:** Augment state space or use recurrent policies (LSTM/GRU)
- **Example:** "Don't revisit same location twice" without state memory

**Debugging Strategies:**

1. **Reward Component Logging:** Track individual reward terms over episodes
2. **Ablation Studies:** Train with subsets of reward components to identify issues
3. **Visualization:** Plot trajectories colored by reward received
4. **Baseline Comparisons:** Compare against sparse reward baseline
5. **Sanity Checks:** Verify rewards for known good/bad behaviors
6. **Episode Replay:** Review high-reward episodes for hacking patterns

## Recommendations for Warehouser

### 1. Core Reward Architecture

**Use Strategy Pattern (Already Implemented):**
- Maintain modular RewardCalculator with separate components
- Enables ablation studies and iterative refinement
- Supports A/B testing of reward designs

**Implement PBRS for Navigation:**
```python
def navigation_potential(robot_pos, object_pos):
    return -np.linalg.norm(robot_pos - object_pos, ord=2)

# Add to reward: gamma * potential(next_state) - potential(current_state)
```

### 2. Recommended Reward Components

**Core Navigation:**
- Distance-to-target reduction (PBRS): γΦ(s') - Φ(s) where Φ(s) = -||robot - target||
- Collision penalty: -10.0 for any collision (terminal)
- Time penalty: -0.01 per timestep to encourage efficiency
- Success bonus: +100.0 for successful pick/place (sparse, terminal)

**Motion Quality:**
- Smoothness: -0.001 * (||a_t - a_{t-1}||^2) to penalize jerky movements
- Energy efficiency: -0.0001 * ||action||^2 to encourage minimal control effort

**Exploration (Early Training):**
- Coverage bonus: Small reward for visiting new grid cells
- Decay over training to phase out as agent learns

**Multi-Robot Coordination:**
- Collision avoidance: -5.0 for robot-robot proximity < threshold
- Efficiency: Bonus for load balancing across robots
- Fairness: Ensure all robots receive similar cumulative rewards

### 3. Multi-Objective Balancing Strategy

**Phase 1: Manual Tuning with Ablation**
- Start with single-objective (task completion only)
- Add components one at a time, verify no reward hacking
- Normalize each component to [-1, 1] range approximately
- Log component contributions over episodes

**Phase 2: Dynamic Weighting (Future)**
- Implement preference-based scalarization from 2025 research
- Allow runtime adjustment of objective weights
- Support multiple personas (speed-focused vs. safety-focused)

**Avoid:**
- Hard-coded weights without empirical justification
- Mixing vastly different scales (0.001 and 100.0) without normalization
- Conflicting objectives without Pareto analysis

### 4. Curriculum Learning Path

**Stage 1: Single Object, Static Environment**
- Reward: Distance-to-object + pick success
- Learn basic navigation and grasping

**Stage 2: Multiple Objects, Static Environment**
- Reward: Add delivery success + time penalty
- Learn task sequencing and prioritization

**Stage 3: Dynamic Obstacles**
- Reward: Add collision avoidance + smoothness
- Learn reactive navigation

**Stage 4: Multi-Robot**
- Reward: Add coordination penalties + fairness
- Learn collaborative behavior

**Implementation:**
- Use automatic curriculum with success rate threshold (>70% → advance)
- Maintain curriculum stage in environment config
- Log stage transitions for reproducibility

### 5. Anti-Reward Hacking Measures

**Adversarial Testing Scenarios:**
- Empty warehouse (ensure no false positive rewards)
- Unreachable objects (ensure graceful failure)
- Dense obstacle fields (ensure no spinning/oscillating)
- Identical objects (ensure correct target selection)

**Monitoring Metrics:**
- Reward per component over time (detect dominance)
- Episode length distribution (detect premature termination)
- Action entropy (detect degenerate policies)
- Success rate on held-out test scenarios

**Safety Guardrails:**
- Collision should always be terminal and heavily penalized
- Success bonus should dominate all shaping rewards
- Time limits to prevent infinite episodes

### 6. Debugging and Validation Workflow

**Before Training:**
1. Unit test reward function with known state pairs
2. Verify reward scales are comparable (within 1-2 orders of magnitude)
3. Check policy invariance if using PBRS (optimal policy unchanged)

**During Training:**
1. Log individual reward components to separate TensorBoard scalars
2. Plot correlation matrix of reward components (detect conflicts)
3. Sample and visualize high-reward episodes (detect hacking)
4. Track Pareto front if using MORL

**After Training:**
1. Evaluate on diverse test scenarios not in training distribution
2. Ablate reward components to verify necessity
3. Compare against sparse reward baseline
4. Human evaluation of policy quality

### 7. Implementation Priorities

**Immediate (Cycle 12):**
- Implement PBRS for navigation rewards
- Add reward component logging to training loop
- Create ablation study configuration presets

**Short-term (Cycles 13-14):**
- Normalize all reward components to similar scale
- Implement curriculum learning stages
- Add adversarial test scenarios

**Long-term (Cycles 15+):**
- Explore MORL with Pareto optimization
- Implement dynamic weight scalarization
- Study Bootstrapped Reward Shaping (BSRS)

### 8. Validation Metrics

**Task Performance:**
- Success rate on test scenarios
- Average episode length
- Number of collisions per episode

**Policy Quality:**
- Path efficiency (actual distance / optimal distance)
- Motion smoothness (acceleration variance)
- Energy consumption (action L2 norm)

**Reward Health:**
- Reward component variance (should all contribute)
- Correlation between components (should be moderate)
- Reward hacking detection (manual inspection of episodes)

## Cloned

No repositories cloned for this research cycle.

## Proposal

### What Should Be Done Based on Research

**Immediate Actions:**

1. **Audit Current Reward Function:**
   - Review existing reward components in `warehouser_rl_bridge`
   - Check for potential reward hacking vulnerabilities
   - Verify reward scales are balanced

2. **Implement PBRS Navigation Reward:**
   - Replace ad-hoc distance rewards with proper potential-based shaping
   - Use Φ(s) = -||robot_pos - target_pos|| as potential function
   - Guarantee policy invariance while providing dense feedback

3. **Add Comprehensive Reward Logging:**
   - Log each reward component separately
   - Track component contributions over episodes
   - Enable TensorBoard visualization for debugging

4. **Create Reward Testing Suite:**
   - Unit tests for reward calculations with known state pairs
   - Integration tests for reward hacking scenarios
   - Adversarial test cases (empty warehouse, unreachable objects, etc.)

5. **Design Curriculum Learning Stages:**
   - Define 4-stage progression (single object → multi-object → dynamic → multi-robot)
   - Implement stage advancement criteria (success rate thresholds)
   - Create configuration presets for each stage

**Strategic Decisions:**

1. **Multi-Objective Approach:**
   - Start with manual weight tuning and ablation studies
   - Plan migration to dynamic scalarization or MORL in future cycles
   - Keep architecture modular to support both approaches

2. **Safety-First Design:**
   - Collisions must be terminal with large penalties
   - Success bonuses should dominate all shaping rewards
   - Hard constraints on time limits and workspace boundaries

3. **Iterative Refinement Process:**
   - Train baseline with sparse rewards only
   - Add components incrementally with ablation tests
   - Monitor for reward hacking after each addition
   - Validate on diverse test scenarios before deployment

**Success Criteria:**

- Agent learns successful navigation and pick/place within 100k timesteps
- No reward hacking observed in adversarial test scenarios
- All reward components contribute (no component ignored)
- Policy transfers to test scenarios with >70% success rate
- Motion is smooth and efficient (low acceleration variance)

**Risk Mitigation:**

- Comprehensive logging enables quick diagnosis of reward issues
- Modular architecture allows easy reward component swapping
- Curriculum learning reduces complexity during early training
- Adversarial testing catches hacking before deployment
- PBRS theoretical guarantees prevent policy bias

This research-backed approach positions Warehouser to leverage state-of-the-art reward shaping techniques while avoiding common pitfalls documented in recent literature.

## Sources

### Potential-Based Reward Shaping
- [HPRS: Hierarchical Potential-Based Reward Shaping](https://www.frontiersin.org/journals/robotics-and-ai/articles/10.3389/frobt.2024.1444188/full)
- [Value-Based Reward Shaping (VBRS)](https://www.mdpi.com/2079-9292/15/2/463)
- [Confounding Robust Continuous Control via Automatic Reward Shaping](https://arxiv.org/html/2602.10305)
- [Boosting RL in Continuous Robotic Reaching Tasks](https://link.springer.com/chapter/10.1007/978-981-96-0351-0_5)
- [Sample Efficiency of Abstractions and PBRS](https://arxiv.org/abs/2404.07826)
- [Potential-Based Reward Shaping Overview](https://www.emergentmind.com/topics/potential-based-reward-shaping)
- [A New Potential-Based Reward Shaping (Medium)](https://medium.com/@sophiezhao_2990/potential-based-reward-shaping-in-reinforcement-learning-05da05cfb84a)

### Reward Hacking and Pitfalls
- [Reward Hacking in Reinforcement Learning (Lilian Weng)](https://lilianweng.github.io/posts/2024-11-28-reward-hacking/)
- [Comprehensive Overview of Reward Engineering and Shaping](https://arxiv.org/html/2408.10215v1)
- [Natural Emergent Misalignment from Reward Hacking (Anthropic)](https://arxiv.org/html/2511.18397v1)
- [What is Reward Hacking in RL? (Milvus)](https://blog.milvus.io/ai-quick-reference/what-is-reward-hacking-in-rl)
- [Modification-Considering Value Learning](https://openreview.net/forum?id=UHYRNAfnNA)
- [AI Safety 101: Reward Misspecification](https://www.lesswrong.com/posts/mMBoPnFrFqQJKzDsZ/ai-safety-101-reward-misspecification)
- [Defining and Characterizing Reward Hacking (Skalse)](https://arxiv.org/pdf/2209.13085)
- [Reward Hacking - Wikipedia](https://en.wikipedia.org/wiki/Reward_hacking)

### Multi-Objective and Curriculum Learning
- [Preference-Based Deep RL with Automatic Curriculum Learning](https://www.sciencedirect.com/science/article/pii/S2215098625002022)
- [Enhancing Robotic Navigation with MORL](https://arxiv.org/html/2312.07953)
- [Review on RL for Motion Planning of Robotic Manipulators](https://onlinelibrary.wiley.com/doi/10.1155/int/1636497)
- [Learning Adaptive Multi-Objective Robot Navigation](https://ui.adsabs.harvard.edu/abs/2024arXiv240404857D/abstract)
- [Deep RL for Robotics Survey](https://www.annualreviews.org/doi/pdf/10.1146/annurev-control-030323-022510)
- [Personalized Robotic Control via Constrained MORL](https://www.sciencedirect.com/science/article/abs/pii/S0925231223011098)
- [Multi-Objective Crowd-Aware Robot Navigation](https://www.sciencedirect.com/science/article/abs/pii/S1568494623011729)
- [Deep RL-Based Local Path Planning in Dynamic Environments](https://www.sciencedirect.com/science/article/pii/S1319157824003434)
