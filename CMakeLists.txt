cmake_minimum_required(VERSION 3.10)
project(DownPour)

set(CMAKE_CXX_STANDARD 17)


set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(headers/glfw)


add_library(glad STATIC
    headers/glad/generated/src/gl.c
)
target_include_directories(glad PUBLIC headers/glad/generated/include)

find_package(OpenGL REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    glad
    OpenGL::GL
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    headers/glfw/include
    headers/glad/generated/include
)