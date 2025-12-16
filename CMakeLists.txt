cmake_minimum_required(VERSION 3.10)
project(DownPour LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Optional CUDA support for testing
option(BUILD_CUDA_TEST "Build CUDA test executable" OFF)

if(BUILD_CUDA_TEST)
    enable_language(CUDA)
    find_package(CUDAToolkit)
    
    if(CUDAToolkit_FOUND)
        add_executable(test_cuda test_cuda.cu)
        set_target_properties(test_cuda PROPERTIES 
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_STANDARD 11
        )
        target_link_libraries(test_cuda PRIVATE CUDA::cudart)
        message(STATUS "CUDA test executable will be built")
    else()
        message(WARNING "CUDA Toolkit not found, skipping CUDA test")
    endif()
endif()


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