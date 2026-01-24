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
    src/logger/Logger.cpp
    src/renderer/Camera.cpp
    src/renderer/Vertex.cpp
    src/renderer/Model.cpp
    src/renderer/MaterialManager.cpp
    src/simulation/WeatherSystem.cpp
    src/simulation/WindshieldSurface.cpp
    src/scene/SceneNode.cpp
    src/scene/Scene.cpp
    src/scene/Entity.cpp
    src/scene/SceneManager.cpp
    src/scene/SceneBuilder.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    Vulkan::Vulkan
    "-framework Metal"
    "-framework QuartzCore"
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    headers/glfw/include
    headers/glm
)

target_include_directories(${PROJECT_NAME} PRIVATE headers/tinygltf)
add_definitions(-DTINYGLTF_NOEXCEPTION)