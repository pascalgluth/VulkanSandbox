cmake_minimum_required(VERSION 3.21)
project(VulkanSandbox)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES
        VulkanSandbox/imgui/*.h
        VulkanSandbox/imgui/*.cpp
        VulkanSandbox/*.h
        VulkanSandbox/*.cpp)

add_executable(VulkanSandbox ${SOURCES})

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})

find_package(SDL2 REQUIRED)
include_directories(${PROJECT_NAME} ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})

include_directories(${PROJECT_NAME} Libs/include)