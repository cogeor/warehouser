cmake_minimum_required(VERSION 3.16)
project(warehouser_observations)

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
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)

# Library with observation builder, lidar, odometry simulators, and noise model
add_library(${PROJECT_NAME}_lib SHARED
  src/observation_builder.cpp
  src/lidar_simulator.cpp
  src/odometry_simulator.cpp
  src/noise_model.cpp
)
target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  warehouser_msgs
)

# Observations node executable
add_executable(observations_node src/observations_node.cpp)
target_link_libraries(observations_node ${PROJECT_NAME}_lib)
ament_target_dependencies(observations_node
  rclcpp
  nav_msgs
  sensor_msgs
  warehouser_msgs
)

# Tests
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)

  # Lint tests
  ament_lint_auto_find_test_dependencies()

  # Unit tests
  ament_add_gtest(test_observation_builder test/test_observation_builder.cpp)
  target_link_libraries(test_observation_builder ${PROJECT_NAME}_lib)

  ament_add_gtest(test_lidar_simulator test/test_lidar_simulator.cpp)
  target_link_libraries(test_lidar_simulator ${PROJECT_NAME}_lib)
  ament_target_dependencies(test_lidar_simulator rclcpp)

  ament_add_gtest(test_odometry_simulator test/test_odometry_simulator.cpp)
  target_link_libraries(test_odometry_simulator ${PROJECT_NAME}_lib)

  ament_add_gtest(test_noise_model test/test_noise_model.cpp)
  target_link_libraries(test_noise_model ${PROJECT_NAME}_lib)

  # Integration tests (require nodes to be running)
  ament_add_gtest(test_observations_integration test/integration/test_observations_node.cpp)
  target_link_libraries(test_observations_integration ${PROJECT_NAME}_lib)
  ament_target_dependencies(test_observations_integration
    rclcpp warehouser_msgs
  )
endif()

# Install
install(TARGETS observations_node
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
