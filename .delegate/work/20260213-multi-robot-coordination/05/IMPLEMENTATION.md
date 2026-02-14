# Loop 05: Pass robot_count to simulation reset service

## Task 1: Create SimReset.srv and update simulation reset

Completed: 2026-02-13T15:30:00Z

### Changes

- `ros_ws/src/warehouser_msgs/srv/SimReset.srv`: Created new service definition with robot_count request parameter and actual_robot_count/success/message response fields

- `ros_ws/src/warehouser_msgs/CMakeLists.txt`: Added SimReset.srv to rosidl_generate_interfaces

- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp`: Added resetWithRobotCount(size_t) method declaration

- `ros_ws/src/warehouser_simulation/src/world_manager.cpp`: Implemented resetWithRobotCount() to clear existing robots and spawn N robots in grid pattern

- `ros_ws/src/warehouser_simulation/include/warehouser_simulation/simulation_node.hpp`: Changed reset service type from std_srvs::Trigger to warehouser_msgs::SimReset

- `ros_ws/src/warehouser_simulation/src/simulation_node.cpp`: Updated handleReset() to accept robot_count and call resetWithRobotCount()

- `ros_ws/src/warehouser_rl_bridge/include/warehouser_rl_bridge/rl_bridge_node.hpp`: Changed reset_client_ type to SimReset service client

- `ros_ws/src/warehouser_rl_bridge/src/rl_bridge_node.cpp`: Updated resetSimulation() to use SimReset service and pass robot_count

### Verification

- [ ] Build: colcon build (requires ROS2 environment)
- [ ] Test: colcon test (requires ROS2 environment)

### Notes

Bug 3 fix: The simulation now accepts robot_count during reset via the new SimReset service. The WorldManager::resetWithRobotCount() method clears existing robots and spawns the requested number in a grid pattern with 2.0m spacing, clamped to world bounds. Robot IDs follow the pattern "robot0", "robot1", etc. for consistency with per-robot topic naming.

---
