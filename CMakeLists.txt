cmake_minimum_required(VERSION 4.0)
project(hwrt)

set(CMAKE_CXX_STANDARD 20)

add_executable(hwrt main.cpp)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(hwrt PRIVATE vulkan glfw)