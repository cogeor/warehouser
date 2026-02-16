# TASK: Frontend Policy Execution, Obstacles, and Trajectory Trace

## Context
The robot simulation works and can be moved by keyboard. A deep-learning policy exists for goal-seeking behavior.

## Requirements

### 1. Frontend Policy Execution
- Add UI control to enable/disable running the inference policy from the frontend
- When enabled, robot should autonomously navigate toward objectives using the trained policy
- Must integrate with existing ROS inference node

### 2. Obstacles and Larger Scene
- Add obstacles to the simulation scene
- Make the simulation world larger
- Obstacles should be visible in frontend and affect robot navigation/lidar

### 3. Trajectory Trace Feature
- Add "Options" category in the frontend UI (under status section)
- Include "Trace" item with a tickable checkbox
- When enabled, store and display robot trajectory history
- Implement trajectory storage logic and rendering

### 4. Multi-Robot Architecture Review
- Review all code changes and existing codebase
- Produce thorough report on changes needed for multiple concurrent robots
- Follow ROS best practices for services, modularity, namespacing
- Cover: topics, services, TF frames, state management, frontend rendering
