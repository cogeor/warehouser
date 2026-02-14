# Search: Safety and Collision Avoidance for Warehouse Robots

Created: 2026-02-12T23:56:34Z

## Query

Multiple focused searches executed:
1. "ISO 3691-4 warehouse robot safety standard autonomous mobile robots 2026"
2. "collision avoidance algorithms VFH DWA ORCA warehouse robots 2025"
3. "safe reinforcement learning constraint satisfaction shielding warehouse robots 2025"
4. "emergency stop e-stop recovery safety-rated sensors warehouse AMR robots 2025"
5. "multi-robot traffic management deadlock avoidance right-of-way warehouse 2025"

## Findings

### 1. Safety Standards and Certification

**ISO 3691-4:2023 - The Primary Standard for Warehouse Robots**

ISO 3691-4:2023 is the international standard that specifies safety requirements for "driverless industrial trucks" including AGVs, AMRs, AGCs, and autonomous forklifts. The standard has 6 major sections, 5 annexes, and spans 82 pages.

Key requirements:
- **Braking systems** must activate on power interruption or loss of control
- **Overspeed detection** triggers stop when speed exceeds manufacturer's rated speed
- **Personnel detection systems** must enable stopping within specified operating range
- **Automatic charging** connections >60 VDC or 25 VAC must prevent shock hazards
- **Speed monitoring** to ensure stability across all operating conditions

**Stakeholder Responsibilities:**
The standard defines shared responsibilities across:
- OEM Manufacturers (design-level safety)
- Integrators (deployment-level safety)
- End Users (operational-level safety)

**Regional Standards:**
- U.S. buyers increasingly demand ISO 3691-4 alignment due to stricter provisions
- ANSI/ITSDF B56.5 previously applied to AGVs but ISO 3691-4 now dominates
- New U.S. standards (ANSI/A3 R15.06-2025) align with ISO 3691-4 principles

**Key Citations:**
- [ISO 3691-4: The Global Standard for Mobile Robot Safety](https://jlcrobotics.com/iso-3691-4/)
- [Mobile Robot Safety Standards: ISO 3691-4 and ANSI/RIA R15.08](https://www.saphira.ai/blog/mobile-robot-safety-standards-understanding-iso-3691-4-(driverless-industrial-trucks)-and-r15-08-(industrial-mobile-robots)-implementation)
- [ISO 3691-4:2023 Official Standard](https://www.iso.org/standard/83545.html)
- [3Laws: What ISO 3691-4 Means for Your Products](https://3laws.io/iso-3691-4-what-it-means-for-your-products/)

---

### 2. Collision Avoidance Algorithms

**Dynamic Window Approach (DWA)**

DWA is a real-time local path planning algorithm that:
- Generates feasible dynamic paths considering robot dynamics
- Samples velocity space based on current velocity and acceleration constraints
- Evaluates trajectories using cost functions (goal heading, clearance, velocity)
- Provides fast, reactive obstacle avoidance

**Limitations:** Fixed cost function weights can lead to suboptimal paths; may struggle in complex multi-robot scenarios.

**Vector Field Histogram (VFH)**

VFH creates a polar histogram representation of obstacles:
- Divides space into angular sectors
- Calculates occupancy probabilities for each sector
- Selects steering direction with lowest obstacle density
- **p-VFH (2025):** Probabilistic variant improves exploration efficiency using probability distributions

**Optimal Reciprocal Collision Avoidance (ORCA)**

ORCA is a decentralized multi-agent algorithm:
- Each agent makes decisions based only on neighbor states (no central controller)
- Translates velocity obstacles into half-planes constraining velocity space
- Uses linear programming to find optimal velocity in convex space
- Provides smoother trajectories and stronger collision guarantees than RVO

**Hybrid Approaches (2025 Research)**

Recent papers demonstrate superior performance with combined approaches:

1. **ORCA-DWA Hybrid (Jan 2025):**
   - Combines ORCA's optimal speed selection with DWA's fast planning
   - Solves ORCA's difficulty in determining preferred speed
   - Maintains optimal trajectory while avoiding obstacles
   - Citation: [Mobile robot path planning based on ORCA and improved DWA](https://www.tandfonline.com/doi/full/10.1080/00207179.2025.2454905?af=R)

2. **Multi-UAV DWA-ORCA Integration (Apr 2025):**
   - DWA pre-screening layer compresses candidate velocity space
   - Conditional ORCA triggering when inter-robot distance < 2m
   - Results: 27.9% reduction in path length, 100% obstacle avoidance success
   - Citation: [Multi-UAV autonomous obstacle avoidance with DWA and ORCA](https://www.nature.com/articles/s41598-025-99111-8)

3. **ORCA-FLC (Fuzzy Logic Control, Aug 2025):**
   - Uses fuzzy logic controllers to handle uncertainty
   - Better adaptability to dynamic obstacles than fixed-weight approaches
   - Outperforms vanilla ORCA when agent velocity exceeds threshold
   - Citation: [Improved Obstacle Avoidance with ORCA-FLC](https://arxiv.org/abs/2508.06722)

**Warehouse-Specific Considerations (Apr 2025):**

Research on human-robot collaborative warehouses emphasizes:
- Framework accounting for static obstacles (cones, toolboxes) and dynamic (humans, robots)
- Fuzzy module for continuous speed adjustment when approaching obstacles
- Safe distance maintenance and speed reduction near humans
- Citation: [Obstacle Avoidance in Warehouse Environments](https://www.researchgate.net/publication/390633086_Obstacle_Avoidance_Technique_for_Mobile_Robots_at_Autonomous_Human-Robot_Collaborative_Warehouse_Environments)

**Algorithm Comparison Summary:**

| Algorithm | Strengths | Weaknesses | Best Use Case |
|-----------|-----------|------------|---------------|
| DWA | Fast, real-time, considers dynamics | Fixed weights, local minima | Single robot reactive navigation |
| VFH | Good for cluttered spaces | No multi-robot coordination | Unknown/complex environments |
| ORCA | Decentralized, smooth multi-agent | Needs preferred velocity input | Multi-robot coordination |
| ORCA-DWA | Best of both, 100% success rate | More complex implementation | Multi-robot warehouses |

---

### 3. Safe Reinforcement Learning

**Safety Challenges in RL Deployment**

Ensuring constraint satisfaction during RL controller deployment remains a key challenge for safety-critical systems. Traditional RL can violate safety constraints during both training and deployment.

**Two Primary Approaches:**

1. **Risk Shielding:** Identifies dangerous actions and excludes them from viable action set
2. **Safety Decision-Making:** Embeds safety directly into policy optimization

**Shielding Mechanisms**

Shielding examines all possible actions in current state and eliminates those violating safety protocols, establishing a safe action subset.

**Key Methods:**

1. **Safety Shields/Filters:**
   - Monitor agent's chosen action and override if unsafe
   - Override with safe default action or closest safe action
   - Dalal et al. (2018): Quadratic program (QP) in continuous spaces to minimally adjust actions
   - Guarantees zero constraint violations during training

2. **Adaptive Robust Model Predictive Shielding (2025):**
   - Verifies proposed actions through predictive models
   - Replaces unsafe actions with backup policy
   - Uses approximate robust nonlinear MPC as backup trained offline
   - Retains safety under uncertainty while enabling real-time applicability
   - Citation: [Safe RL via Adaptive Robust MPC Shielding](https://www.sciencedirect.com/science/article/pii/S0098135425005241)

3. **Probabilistic Shielding (AAAI 2025):**
   - State-augmentation of MDP for safety dynamics
   - Strict formal guarantees for probabilistic avoidance properties
   - Agent stays safe at training AND test time
   - Citation: [Probabilistic Shielding for Safe RL](https://ojs.aaai.org/index.php/AAAI/article/view/33767)

**Warehouse-Specific Safe RL**

**Human-Robot Collaboration Research:**
- Mobile robots share workspace with human workers (unpredictable positions)
- Visual input from LiDAR/RGB camera for dynamic adjustments
- Velocity command adjustments to maintain safe distance and reduced speed
- Citation: [Safe RL for Human-Robot Collaboration in Warehouses](https://kth.diva-portal.org/smash/record.jsf?pid=diva2:1713407&dswid=-7999)

**Multi-Robot Safe RL (Jan 2025):**
- Uniformly ultimate boundedness constraints for multi-robot systems
- Applications in logistics distribution, intelligent transportation, smart warehousing
- Real-time control requirements
- Citation: [Multi-robot hierarchical safe RL](https://www.nature.com/articles/s41598-025-89285-6)

**Trade-offs:**

**Shielding Advantages:**
- Hard safety guarantees (no violations in theory)
- Formal verification possible
- Can retrofit existing policies

**Shielding Disadvantages:**
- Requires additional knowledge (dynamics model, safe set, or supervisor)
- May introduce performance bias (overly conservative behavior)
- Computational overhead for real-time verification

**Recommendations for Warehouser:**
- Implement shielding layer as safety backstop for RL policies
- Use MPC-based backup controller with collision prediction
- Define formal safe set based on ISO 3691-4 requirements (speed limits, stopping distances)
- Train with constraint-aware reward shaping to minimize shield interventions

---

### 4. Emergency Stop and Recovery Systems

**E-Stop Challenges in Autonomous Systems**

Traditional emergency stops require manual worker restart, reducing usability and causing gridlock in multi-robot systems where one stopped robot blocks others.

**Modern E-Stop Approaches (2025):**

**Dynamic Safety Features:**
- Enable automated restarting after safety-rated stop
- Instead of full stop, robots slow down and navigate around obstacles
- Example: Box falling from shelf triggers slowdown and re-routing, not full stop
- "Keeping robots moving is especially important in multi-system environments" - Andrew Singletary, CEO 3Laws Robotics
- Citation: [Latest in Autonomous Mobile Robots Safety](https://www.automate.org/robotics/industry-insights/autonomous-mobile-robot-safety-updates-new-features)

**Wireless E-Stop Systems:**

**FORT Robotics Vehicle Safety Controller (VSC):**
- VSC installed on each mobile robot
- "Master" VSC on entry doors to caged areas
- Automatically sends wireless safety-rated e-stop signal when door opens
- Designed to ISO 13849, Cat3, PLd safety standards
- Fleet-wide safety commands to multiple robots simultaneously
- Integration with fire alarms, door switches, sensors, buttons
- Citation: [Wireless E-Stop Technology](https://www.mmh.com/article/wireless_e_stop_technology_improves_safety_around_warehouse_robots)
- Citation: [Case Study: Wireless E-Stop for AMRs](https://www.automate.org/robotics/case-studies/case-study-wireless-e-stopping-improves-safety-around-warehouse-amrs)

**Safety-Rated Sensor Technologies:**

**Primary Sensors:**
1. **LiDAR Scanners:**
   - Laser beams measure distance to objects
   - Create 2D/3D maps for obstacle detection
   - Primary navigation and safety sensor

2. **Depth-Sensing 3D Cameras:**
   - Detect obstacles below LiDAR plane
   - Avoid floor drop-offs
   - Complementary to LiDAR coverage

3. **Safety-Edge Bumpers:**
   - Physical contact triggers E-Stop mode
   - Built-in self-diagnostics run when tripped
   - Last-resort collision detection

4. **Emergency Stop Buttons:**
   - Cut power from robot motors when pressed
   - Required on all units per ISO 3691-4
   - KUKA AMRs: 4 E-Stop pushbuttons for operator access

**SIL-Rated Communication:**
- Functionally safe, SIL-rated systems for reliable wireless command transmission
- Built-in security ensures commands reliably sent, received, executed
- Critical for fleet-wide coordinated safety responses

**Fire Safety Considerations:**
- Integration of fire alarm systems with fleet E-Stop
- Automated evacuation procedures
- Citation: [Fire Safety for Warehouse Robots](https://www.fortrobotics.com/news/fire-safety-for-warehouse-amrs)

---

### 5. Multi-Robot Traffic Management and Deadlock Prevention

**Deadlock Scenarios in Warehouses**

**Common Deadlock Types:**
1. **Heading-On Deadlock:** Two AMRs approach narrow aisle from opposite directions
2. **Loop Deadlock:** Circular dependency where robots block each other's paths
3. **Intersection Deadlock:** Multiple robots arrive at intersection simultaneously

**Recent Research Solutions (2025):**

**1. LivePoint - Fully Decentralized Approach:**
- Ensures minimally invasive deadlock avoidance
- Dynamically adjusts agents' speeds based on symmetric interaction metric
- Validated in doorways and intersections
- Results: Zero collisions/deadlocks, 100% success rate
- Citation: [LivePoint: Fully Decentralized Multi-Robot Control](https://arxiv.org/html/2503.13098)

**2. Discrete-Time Control Barrier Functions:**
- Addresses multi-robot navigation in constrained environments
- Handles narrow doors, hallways, corridor intersections
- Safe and deadlock-free for decentralized systems
- Citation: [Deadlock-free Multi-Robot Navigation](https://link.springer.com/article/10.1007/s10514-025-10194-8)

**3. Nonstop Areas Approach:**
- Defines critical areas in warehouse as nonstop zones
- Prohibits robots from stopping in these areas
- Based on dynamics of vehicle intersections
- Citation: [Multi-Robot Scheduling with Nonstop Areas](https://ieeexplore.ieee.org/document/10905455/)

**4. Hierarchical Traffic Management:**

Three-layer control architecture:
- **Top Layer (Topological):** Models traffic among different areas
- **Middle Layer:** Path planner computes traffic-sensitive paths
- **Bottom Layer (Roadmap):** Defines final routes, coordinates AGVs over time
- Uses time-expanded graphs for deadlock prevention
- Citation: [Hierarchical Traffic Management of Multi-AGV Systems](https://www.researchgate.net/publication/371019271_Hierarchical_Traffic_Management_of_Multi-AGV_Systems_With_Deadlock_Prevention_Applied_to_Industrial_Environments)

**5. Multi-Agent RL for Deadlock Handling:**
- Dissertation addresses narrow warehouse aisles with unidirectional traffic
- Strategy: Prevent AMRs from facing head-on by construction
- Trade-off: Reduces flexibility as only one aisle accessible from each side
- Citation: [Multi-Agent RL for Deadlock Handling](https://opendata.uni-halle.de/bitstream/1981185920/123455/1/M%C3%BCller_Marcel_Dissertation_2025.pdf)

**Path Planning with Traffic Rules:**

**Improved A* with Reservation Tables:**
- Incorporates traffic rules into planning
- Reservation tables prevent conflicts
- Efficient multi-robot path planning in 2D warehouse logistics
- Citation: [Path Planning Approaches in Multi-Robot Systems](https://onlinelibrary.wiley.com/doi/10.1002/eng2.13035)

**Dynamic Resource Reservation (IDRR):**
- Time-efficient task completion
- Deadlock-free movements of multiple AGVs
- Applied in manufacturing systems

**Social Mini-Games and Right-of-Way:**

Research insight: "Humans are adept at avoiding collisions and deadlocks without deviating too much from their preferred walking speed or trajectory; for instance, when two individuals go through a doorway together, one person modulates their velocity just enough to enable the other to pass through first."

Challenge: Robots often collide or deadlock in "social mini-games" due to lack of implicit coordination that humans use naturally.

**Solution Strategies:**
- Symmetric interaction metrics (LivePoint)
- Priority-based right-of-way systems
- Velocity modulation near conflict zones
- Predictive conflict detection and preemptive yielding

---

## Cloned

No repositories cloned. All information gathered from academic papers and technical documentation.

---

## Proposal: Safety Architecture for Warehouser

Based on comprehensive research, I propose a layered safety architecture for Warehouser:

### Layer 1: Standards Compliance
**Target: ISO 3691-4:2023 Alignment**
- Implement overspeed detection and automatic braking
- Define personnel detection system parameters (detection range, stopping distance)
- Document OEM/Integrator/End-User responsibility boundaries
- Ensure braking system activates on power interruption

### Layer 2: Collision Avoidance
**Recommended Algorithm: ORCA-DWA Hybrid**

Rationale:
- Proven 100% obstacle avoidance success rate in recent research
- 27.9% path length reduction vs traditional approaches
- Handles both single-robot reactive needs (DWA) and multi-robot coordination (ORCA)
- Conditional triggering reduces computational overhead

**Implementation:**
1. DWA pre-screening layer filters candidate velocities
2. ORCA activation when inter-robot distance < 2.0m threshold
3. Fuzzy logic layer for human proximity (variable speed reduction)
4. Safety velocity limits from ISO 3691-4 as hard constraints

### Layer 3: Safe RL Deployment
**Recommended: Adaptive Robust MPC Shielding**

Components:
1. **Shield Layer:** Verifies RL policy outputs before execution
2. **Backup Controller:** MPC trained offline with robust multi-stage data
3. **Formal Safe Set:** Derived from ISO 3691-4 stopping distances and speed limits
4. **Constraint Rewards:** Minimize shield interventions during training

**Training Protocol:**
- Use shielding during both training and deployment
- Log shield intervention frequency as safety metric
- Gradually reduce constraint penalties as policy learns safe behaviors
- Require <5% shield intervention rate before deployment

### Layer 4: Emergency Systems
**E-Stop Architecture:**

1. **Local E-Stop:** Physical buttons on each robot (ISO 3691-4 requirement)
2. **Fleet-Wide E-Stop:** Wireless safety-rated system (ISO 13849 Cat3, PLd)
3. **Smart Recovery:** Automated restart after safety-rated stop (not full E-Stop)
4. **Fire Integration:** Fleet E-Stop triggered by building fire alarm system

**Sensor Redundancy:**
1. **Primary:** Safety-rated LiDAR for navigation and obstacle detection
2. **Secondary:** Depth-sensing 3D cameras for low obstacles and drop-offs
3. **Tertiary:** Safety-edge bumpers for contact detection
4. **Monitoring:** Cross-validate sensor readings, trigger safe stop on disagreement

### Layer 5: Multi-Robot Coordination
**Deadlock Prevention Strategy:**

1. **Nonstop Zones:** Mark intersections and narrow passages
2. **Reservation System:** Time-expanded graph with traffic rules
3. **Right-of-Way Protocol:** Distance-based priority (closer robot has right-of-way)
4. **Velocity Modulation:** LivePoint-style symmetric interaction metric
5. **Deadlock Detection:** Timeout-based detection with automated resolution (one robot reverses)

**Traffic Management:**
- Implement 3-layer hierarchical architecture
- Use improved A* with reservation tables for global planning
- Apply ORCA-DWA hybrid for local planning with multi-robot awareness
- Define unidirectional aisles where space permits

### Integration with Existing Warehouser Components

**SafetyController Enhancement:**
```
Input: RL policy action, sensor data, robot state
Process:
  1. Check formal safe set constraints
  2. Verify multi-robot coordination (ORCA)
  3. Apply MPC shielding if unsafe
  4. Output validated action or backup action
```

**Reward Function Augmentation:**
```
reward = task_reward
         - collision_penalty (large negative)
         - speed_violation_penalty
         - personal_space_penalty (human proximity)
         + exploration_reward
         - shield_intervention_penalty (small negative)
```

**Observation Space Addition:**
```
Add to existing observations:
- Nearest robot distances and velocities (for ORCA)
- Nonstop zone indicator (binary)
- Time-to-collision estimate (from MPC predictor)
- Shield intervention flag (for learning)
```

### Testing Requirements

**Simulation Tests:**
1. Single-robot obstacle avoidance (static and dynamic)
2. Multi-robot intersection crossing (2, 4, 8 robots)
3. Narrow corridor passing (head-on scenarios)
4. Human proximity response (variable speeds)
5. E-Stop recovery and restart
6. Sensor failure scenarios (LiDAR dropout)
7. Deadlock resolution (timeout-based)

**Safety Metrics:**
1. Collision rate: 0 per 1000 episodes (hard requirement)
2. Deadlock rate: <0.1% of multi-robot scenarios
3. Shield intervention rate: <5% after training convergence
4. E-Stop trigger appropriateness: 100% (no false positives)
5. Speed limit compliance: 100%
6. Stopping distance compliance: 100% (ISO 3691-4)

**Sim-to-Real Safety Margin:**
- Use 1.5x safety factor on all distance thresholds
- Reduce max velocity by 20% for real deployment
- Increase sensor fusion redundancy (all 3 sensor types)
- Implement graduated deployment (single robot → multi-robot)

---

## Key Takeaways

1. **ISO 3691-4:2023 is the gold standard** - align all safety requirements to this specification
2. **ORCA-DWA hybrid outperforms individual algorithms** - 100% success rate in recent research
3. **Shielding is essential for safe RL** - use MPC-based backup with formal guarantees
4. **Modern E-Stop systems enable recovery** - don't halt entire fleet for single incident
5. **Deadlock prevention requires multiple strategies** - nonstop zones + reservations + right-of-way
6. **Sensor redundancy is non-negotiable** - LiDAR + cameras + bumpers with cross-validation
7. **Multi-robot coordination is a solved problem** - use time-expanded graphs with traffic rules

### Next Steps for Implementation

1. Formalize safe set based on ISO 3691-4 parameters
2. Implement ORCA-DWA hybrid in SafetyController
3. Develop MPC shielding layer with collision prediction
4. Design nonstop zones and reservation system for warehouse layout
5. Add constraint penalties to reward function
6. Create comprehensive safety test suite
7. Document OEM/Integrator/User responsibilities per ISO 3691-4

---

## Sources

### Safety Standards
- [ISO 3691-4: The Global Standard for Mobile Robot Safety](https://jlcrobotics.com/iso-3691-4/)
- [Mobile Robot Safety Standards: ISO 3691-4 and ANSI/RIA R15.08](https://www.saphira.ai/blog/mobile-robot-safety-standards-understanding-iso-3691-4-(driverless-industrial-trucks)-and-r15-08-(industrial-mobile-robots)-implementation)
- [ISO 3691-4:2023 Official Standard](https://www.iso.org/standard/83545.html)
- [3Laws: What ISO 3691-4 Means for Your Products](https://3laws.io/iso-3691-4-what-it-means-for-your-products/)
- [AGV Network: ISO 3691-4 Responsibility Framework](https://www.agvnetwork.com/automated-guided-vehicles-technology/standard-3691-4)
- [ANSI Blog: ISO 3691-4:2023 for Driverless Industrial Trucks](https://blog.ansi.org/ansi/iso-3691-4-2023-driverless-industrial-trucks/)

### Collision Avoidance Algorithms
- [Mobile robot path planning based on ORCA and improved DWA](https://www.tandfonline.com/doi/full/10.1080/00207179.2025.2454905?af=R)
- [Multi-UAV autonomous obstacle avoidance with DWA and ORCA](https://www.nature.com/articles/s41598-025-99111-8)
- [Improved Obstacle Avoidance with ORCA-FLC](https://arxiv.org/abs/2508.06722)
- [Collision-based probabilistic obstacle avoidance algorithm](https://www.tandfonline.com/doi/pdf/10.1080/01691864.2025.2530515)
- [Obstacle Avoidance for AMRs Based on Mapping Method](https://arxiv.org/pdf/2109.06773)
- [Obstacle Avoidance in Warehouse Environments](https://www.researchgate.net/publication/390633086_Obstacle_Avoidance_Technique_for_Mobile_Robots_at_Autonomous_Human-Robot_Collaborative_Warehouse_Environments)

### Safe Reinforcement Learning
- [Multi-robot hierarchical safe RL](https://www.nature.com/articles/s41598-025-89285-6)
- [Safe RL via Adaptive Robust MPC Shielding](https://www.sciencedirect.com/science/article/pii/S0098135425005241)
- [Survey of Safe RL and Constrained MDPs](https://arxiv.org/html/2505.17342v1)
- [Human-Risk-Aware Safe Path Planning with RL](https://www.mdpi.com/1424-8220/25/23/7211)
- [Safe RL for Human-Robot Collaboration in Warehouses](https://kth.diva-portal.org/smash/record.jsf?pid=diva2:1713407&dswid=-7999)
- [Probabilistic Shielding for Safe RL](https://ojs.aaai.org/index.php/AAAI/article/view/33767)
- [Safe RL Baselines Repository](https://github.com/chauncygu/Safe-Reinforcement-Learning-Baselines)

### Emergency Stop and Recovery
- [Wireless E-Stop Technology](https://www.mmh.com/article/wireless_e_stop_technology_improves_safety_around_warehouse_robots)
- [Case Study: Wireless E-Stop for AMRs](https://www.automate.org/robotics/case-studies/case-study-wireless-e-stopping-improves-safety-around-warehouse-amrs)
- [Latest in Autonomous Mobile Robots Safety](https://www.automate.org/robotics/industry-insights/autonomous-mobile-robot-safety-updates-new-features)
- [KUKA AMR Safety Features](https://www.kuka.com/en-us/products/amr-autonomous-mobile-robotics)
- [Fire Safety for Warehouse Robots](https://www.fortrobotics.com/news/fire-safety-for-warehouse-amrs)
- [Why Include Wireless E-Stop in AMR Design](https://www.fortrobotics.com/news/autonomous-mobile-robot-design)
- [Humans and Robots in the Warehouse: The Safety Dance](https://locusrobotics.com/blog/humans-and-robots-in-the-warehouse-the-safety-dance)

### Multi-Robot Traffic Management
- [Autonomous mobile robot travel with deadlock and collision prevention](https://www.tandfonline.com/doi/full/10.1080/13675567.2022.2138290)
- [Multi-Robot Scheduling with Nonstop Areas](https://ieeexplore.ieee.org/document/10905455/)
- [LivePoint: Fully Decentralized Multi-Robot Control](https://arxiv.org/html/2503.13098)
- [Deadlock-free Multi-Robot Navigation](https://link.springer.com/article/10.1007/s10514-025-10194-8)
- [Path Planning Approaches in Multi-Robot Systems](https://onlinelibrary.wiley.com/doi/10.1002/eng2.13035)
- [Hierarchical Traffic Management of Multi-AGV Systems](https://www.researchgate.net/publication/371019271_Hierarchical_Traffic_Management_of_Multi-AGV_Systems_With_Deadlock_Prevention_Applied_to_Industrial_Environments)
- [Multi-Agent RL for Deadlock Handling](https://opendata.uni-halle.de/bitstream/1981185920/123455/1/M%C3%BCller_Marcel_Dissertation_2025.pdf)
- [Deadlock prevention and multi-agent path finding for massive fleet AGV](https://www.sciencedirect.com/science/article/abs/pii/S156849462400499X)
