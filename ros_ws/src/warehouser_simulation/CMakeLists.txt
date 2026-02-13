cmake_minimum_required(VERSION 3.16)
project(warehouser_simulation)

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
find_package(rosgraph_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)

# Library with entity classes and world manager
add_library(${PROJECT_NAME}_lib SHARED
  src/entity.cpp
  src/robot.cpp
  src/pickable_object.cpp
  src/wall.cpp
  src/zone.cpp
  src/world_manager.cpp
)
target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  std_msgs
  warehouser_msgs
)

# Simulation node executable
add_executable(simulation_node src/simulation_node.cpp)
target_link_libraries(simulation_node ${PROJECT_NAME}_lib)
ament_target_dependencies(simulation_node
  rclcpp
  std_msgs
  std_srvs
  geometry_msgs
  rosgraph_msgs
  warehouser_msgs
)

# Tests
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)

  # Lint tests
  ament_lint_auto_find_test_dependencies()

  # Unit tests
  ament_add_gtest(test_entity test/test_entity.cpp)
  target_link_libraries(test_entity ${PROJECT_NAME}_lib)

  ament_add_gtest(test_robot test/test_robot.cpp)
  target_link_libraries(test_robot ${PROJECT_NAME}_lib)

  ament_add_gtest(test_world_manager test/test_world_manager.cpp)
  target_link_libraries(test_world_manager ${PROJECT_NAME}_lib)

  ament_add_gtest(test_robot_collision test/test_robot_collision.cpp)
  target_link_libraries(test_robot_collision ${PROJECT_NAME}_lib)

  # Integration tests (require nodes to be running)
  ament_add_gtest(test_simulation_integration test/integration/test_simulation_node.cpp)
  target_link_libraries(test_simulation_integration ${PROJECT_NAME}_lib)
  ament_target_dependencies(test_simulation_integration
    rclcpp std_msgs std_srvs geometry_msgs warehouser_msgs
  )
endif()

# Install
install(TARGETS simulation_node
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

# Export library for other packages
ament_export_targets(${PROJECT_NAME}_libTargets HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp std_msgs warehouser_msgs)

ament_package()
