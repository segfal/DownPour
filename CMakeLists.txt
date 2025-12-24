cmake_minimum_required(VERSION 3.10)
project(DownPour)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(headers/glfw)

# Vulkan
find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} 
    main.cpp
    src/DownPour.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    Vulkan::Vulkan
    "-framework Metal"
    "-framework QuartzCore"
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    headers/glfw/include
    headers/glm
)