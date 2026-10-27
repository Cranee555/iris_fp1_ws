cmake_minimum_required(VERSION 3.8)
project(communication)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(udp_sender src/udp_sender.cpp)
ament_target_dependencies(udp_sender rclcpp std_msgs)

install(TARGETS udp_sender
  DESTINATION lib/${PROJECT_NAME})

ament_package()
