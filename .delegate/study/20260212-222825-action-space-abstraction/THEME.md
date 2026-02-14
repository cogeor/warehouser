# Cycle 11 Theme: Action Space Abstraction

## Focus
Research action space design patterns for robotics RL, focusing on continuous vs discrete actions and abstraction levels.

## Context
- Warehouser uses continuous actions: [linear_vel, angular_vel, pick, place]
- Pick/place are discrete but encoded as continuous [-1, 1]
- Need to understand action space design for effective learning and sim-to-real transfer

## Objectives
1. Research continuous vs discrete action spaces
2. Study action abstraction levels (low-level vs skills)
3. Identify action smoothing and rate limiting patterns
4. Document action space normalization strategies
5. Find patterns for hybrid action spaces
