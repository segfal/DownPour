// SPDX-License-Identifier: MIT
#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

typedef glm::vec3 vec3;
typedef glm::mat3 mat3;
typedef glm::vec4 vec4;
typedef glm::mat4 mat4;
typedef float     f32;

class Camera {
public:
    Camera(vec3 pos, f32 aspect);
    Camera(vec3 pos, f32 aspect, f32 yaw, f32 pitch);
    Camera() : Camera(vec3(0.0f, 0.0f, 0.0f), 4.0f / 3.0f) {}
    mat4 getProjectionMatrix();
    void processInput(GLFWwindow* window, float deltaTime);   // for WASD movement
    void processMouseMovement(float xoffset, float yoffset);  // for looking around
    mat4 getViewMatrix();
    void setYaw(f32 newYaw);
    void setPitch(f32 newPitch);
    void setPosition(vec3 pos);
    void setFOV(f32 newFOV);
    void setFarPlane(f32 newFarPlane);

private:
    vec3 position;
    vec3 forward, right, up;
    f32  pitch, yaw;
    f32  fov, aspectRatio, nearPlane, farPlane;

    void updateCameraVectors();  // recalculate forward, right, up vectors
};
