#include "renderer/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(vec3 pos, f32 aspect)
    : position(pos), aspectRatio(aspect), pitch(0.0f), yaw(-90.0f),
      fov(45.0f), nearPlane(0.1f), farPlane(100.0f) {
    updateCameraVectors();
}

Camera::Camera(vec3 pos, f32 aspect, f32 yawAngle, f32 pitchAngle)
    : position(pos), aspectRatio(aspect), pitch(pitchAngle), yaw(yawAngle),
      fov(45.0f), nearPlane(0.1f), farPlane(100.0f) {
    updateCameraVectors();
}

void Camera::setYaw(f32 newYaw) {
    yaw = newYaw;
    updateCameraVectors();
}

void Camera::setPitch(f32 newPitch) {
    pitch = glm::clamp(newPitch, -89.0f, 89.0f);
    updateCameraVectors();
}

void Camera::setPosition(vec3 pos) {
    position = pos;
}

void Camera::setFOV(f32 newFOV) {
    fov = glm::clamp(newFOV, 30.0f, 120.0f);
}

void Camera::setFarPlane(f32 newFarPlane) {
    farPlane = newFarPlane;
}

mat4 Camera::getViewMatrix() {
    return glm::lookAt(position, position + forward, up);
}

mat4 Camera::getProjectionMatrix() {
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void Camera::updateCameraVectors() {
    vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(front);
    right = glm::normalize(glm::cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, forward));
}

void Camera::processInput(GLFWwindow* window, float deltaTime) {
    const float cameraSpeed = 2.5f * deltaTime; // Frame-rate independent movement
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += cameraSpeed * forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= cameraSpeed * forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= cameraSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += cameraSpeed * right;
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    
    yaw += xoffset;
    pitch += yoffset;
    
    // Constrain pitch to prevent screen flip
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
    
    updateCameraVectors();
}