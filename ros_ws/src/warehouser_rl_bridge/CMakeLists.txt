cmake_minimum_required(VERSION 3.16)
project(warehouser_rl_bridge)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(MSVC)
  add_compile_options(/W4)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)

# Library with reward calculator and strategies
add_library(${PROJECT_NAME}_lib SHARED
  src/reward_calculator.cpp
  src/reward_strategy.cpp
  src/occupancy_tracker.cpp
  src/exploration_reward.cpp
)
target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  warehouser_msgs
)

# RL Bridge node executable
add_executable(rl_bridge_node src/rl_bridge_node.cpp)
target_link_libraries(rl_bridge_node ${PROJECT_NAME}_lib)
ament_target_dependencies(rl_bridge_node
  rclcpp
  std_msgs
  std_srvs
  geometry_msgs
  warehouser_msgs
)

# Tests
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)

  # Lint tests
  ament_lint_auto_find_test_dependencies()

  # Unit tests
  ament_add_gtest(test_reward_calculator test/test_reward_calculator.cpp)
  target_link_libraries(test_reward_calculator ${PROJECT_NAME}_lib)

  ament_add_gtest(test_reward_strategy test/test_reward_strategy.cpp)
  target_link_libraries(test_reward_strategy ${PROJECT_NAME}_lib)

  ament_add_gtest(test_exploration_reward test/test_exploration_reward.cpp)
  target_link_libraries(test_exploration_reward ${PROJECT_NAME}_lib)

  ament_add_gtest(test_multi_robot test/test_multi_robot.cpp)
  target_link_libraries(test_multi_robot ${PROJECT_NAME}_lib)

  # Integration tests (require nodes to be running)
  ament_add_gtest(test_rl_bridge_integration test/integration/test_rl_bridge_node.cpp)
  target_link_libraries(test_rl_bridge_integration ${PROJECT_NAME}_lib)
  ament_target_dependencies(test_rl_bridge_integration
    rclcpp warehouser_msgs
  )
endif()

# Install
install(TARGETS rl_bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}_lib
  EXPORT ${PROJECT_NAME}_libTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

# Export library
ament_export_targets(${PROJECT_NAME}_libTargets HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp warehouser_msgs)

ament_package()
