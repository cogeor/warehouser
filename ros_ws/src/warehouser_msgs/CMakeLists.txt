cmake_minimum_required(VERSION 3.16)
project(warehouser_msgs)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

# Generate messages and services
rosidl_generate_interfaces(${PROJECT_NAME}
  # Messages
  "msg/Entity.msg"
  "msg/WorldState.msg"
  "msg/Observation.msg"
  "msg/LidarDebug.msg"
  "msg/Goal.msg"
  "msg/Action.msg"
  "msg/TaskStatus.msg"
  # Services
  "srv/RLStep.srv"
  "srv/RLReset.srv"
  "srv/LoadModel.srv"
  "srv/SimStep.srv"
  "srv/GetObservation.srv"
  "srv/SetGoal.srv"
  "srv/SimReset.srv"
  DEPENDENCIES std_msgs
)

# Testing
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
