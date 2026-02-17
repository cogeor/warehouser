cmake_minimum_required(VERSION 3.16)
project(warehouser_inference)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(warehouser_msgs REQUIRED)

# Try to find ONNX Runtime
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
  HINTS
    ${ONNXRUNTIME_ROOT}/include
    /usr/local/include/onnxruntime
    /usr/include/onnxruntime
)

find_library(ONNXRUNTIME_LIBRARY onnxruntime
  HINTS
    ${ONNXRUNTIME_ROOT}/lib
    /usr/local/lib
    /usr/lib
)

if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
  set(ONNXRUNTIME_FOUND TRUE)
  message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")
else()
  set(ONNXRUNTIME_FOUND FALSE)
  message(WARNING "ONNX Runtime not found - building with stub implementation")
endif()

# Library
add_library(${PROJECT_NAME}_lib SHARED
  src/policy_inference.cpp
  src/inference_node.cpp
)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(ONNXRUNTIME_FOUND)
  target_include_directories(${PROJECT_NAME}_lib PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME}_lib ${ONNXRUNTIME_LIBRARY})
  target_compile_definitions(${PROJECT_NAME}_lib PRIVATE ONNXRUNTIME_AVAILABLE)
endif()

ament_target_dependencies(${PROJECT_NAME}_lib
  rclcpp
  std_msgs
  geometry_msgs
  warehouser_msgs
)

# Node executable
add_executable(inference_node src/inference_node_main.cpp)
target_link_libraries(inference_node ${PROJECT_NAME}_lib)

# Install
install(TARGETS inference_node
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

install(DIRECTORY models
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_policy_inference test/test_policy_inference.cpp)
  target_link_libraries(test_policy_inference ${PROJECT_NAME}_lib)
  # Propagate ONNXRUNTIME_AVAILABLE to tests so #ifdef guards compile consistently
  if(ONNXRUNTIME_FOUND)
    target_compile_definitions(test_policy_inference PRIVATE ONNXRUNTIME_AVAILABLE)
  endif()
endif()

ament_package()
