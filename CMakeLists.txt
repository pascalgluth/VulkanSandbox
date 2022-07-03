cmake_minimum_required(VERSION 3.21)
project(VulkanSandbox)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)

file(GLOB SOURCES
        VulkanSandbox/imgui/*.h
        VulkanSandbox/imgui/*.cpp
        VulkanSandbox/*.h
        VulkanSandbox/*.cpp)

add_executable(VulkanSandbox ${SOURCES})

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})


find_package(ASSIMP REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${ASSIMP_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${ASSIMP_LIBRARIES})

target_include_directories(${PROJECT_NAME} PUBLIC Libs/include-linux)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/objects)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/textures)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/VulkanSandbox/shaders/*.spv
        ${CMAKE_BINARY_DIR}/shaders)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/VulkanSandbox/objects/*
        ${CMAKE_BINARY_DIR}/objects)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/VulkanSandbox/textures/*
        ${CMAKE_BINARY_DIR}/textures)