cmake_minimum_required(VERSION 3.16)
project(warehouser_command)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)
find_package(nlohmann_json QUIET)

# If nlohmann_json not found via CMake, try pkg-config or use bundled
if(NOT nlohmann_json_FOUND)
  # Try to find header directly
  find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    HINTS
      /usr/include
      /usr/local/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  if(NLOHMANN_JSON_INCLUDE_DIR)
    message(STATUS "Found nlohmann/json.hpp at ${NLOHMANN_JSON_INCLUDE_DIR}")
    set(NLOHMANN_JSON_FOUND TRUE)
  else()
    message(STATUS "nlohmann/json not found - using bundled single-header version")
    # We'll include a minimal version in our include dir
  endif()
endif()

# Library
add_library(${PROJECT_NAME}_lib SHARED
  src/command_parser.cpp
  src/object_resolver.cpp
  src/command_node.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(nlohmann_json_FOUND)
  target_link_libraries(${PROJECT_NAME}_lib nlohmann_json::nlohmann_json)
elseif(NLOHMANN_JSON_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME}_lib PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()

ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  std_msgs
  warehouser_msgs
)

# Node executable
add_executable(command_node src/command_node_main.cpp)
target_link_libraries(command_node ${PROJECT_NAME}_lib)

# Install
install(TARGETS command_node
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
  ament_add_gtest(test_command_parser test/test_command_parser.cpp)
  target_link_libraries(test_command_parser ${PROJECT_NAME}_lib)
endif()

ament_package()
