cmake_minimum_required(VERSION 3.16)
project(warehouser_task)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)

# Library
add_library(${PROJECT_NAME}_lib SHARED
  src/task_state_machine.cpp
  src/task_manager_node.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  std_msgs
  std_srvs
  geometry_msgs
  warehouser_msgs
)

# Node executable
add_executable(task_manager_node src/task_manager_node_main.cpp)
target_link_libraries(task_manager_node ${PROJECT_NAME}_lib)

# Install
install(TARGETS task_manager_node
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}_lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_task_state_machine test/test_task_state_machine.cpp)
  target_link_libraries(test_task_state_machine ${PROJECT_NAME}_lib)
endif()

ament_package()
