#include "renderer/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include "logger/Logger.h"

Camera::Camera(vec3 pos, f32 aspect)
    : position(pos), aspectRatio(aspect), pitch(0.0f), yaw(-90.0f), fov(45.0f), nearPlane(0.1f), farPlane(100.0f) {
    updateCameraVectors();
}

Camera::Camera(vec3 pos, f32 aspect, f32 yawAngle, f32 pitchAngle)
    : position(pos),
      aspectRatio(aspect),
      pitch(pitchAngle),
      yaw(yawAngle),
      fov(45.0f),
      nearPlane(0.1f),
      farPlane(100.0f) {
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
    right   = glm::normalize(glm::cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    up      = glm::normalize(glm::cross(right, forward));
}

void Camera::processInput(GLFWwindow* window, float deltaTime) {
    const float cameraSpeed = 2.5f * deltaTime;  // Frame-rate independent movement

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

// ============================================================================
// Camera Mode Management
// ============================================================================

void Camera::setMode(CameraMode mode) {
    if (currentMode != mode) {
        currentMode = mode;
        std::cout << "[Camera] Switched to ";
        switch (mode) {
            case CameraMode::Cockpit:
                std::cout << "Cockpit";
                break;
            case CameraMode::Chase:
                std::cout << "Chase";
                chaseCurrentVelocity = vec3(0.0f);  // Reset velocity
                break;
            case CameraMode::ThirdPerson:
                std::cout << "Third Person";
                break;
        }
        std::cout << " mode" << std::endl;
    }
}

void Camera::cycleMode() {
    switch (currentMode) {
        case CameraMode::Cockpit:
            setMode(CameraMode::Chase);
            break;
        case CameraMode::Chase:
            setMode(CameraMode::ThirdPerson);
            break;
        case CameraMode::ThirdPerson:
            setMode(CameraMode::Cockpit);
            break;
    }
}

void Camera::setCameraTarget(const vec3& target, const quat& rotation) {
    targetPosition = target;
    targetRotation = rotation;
}

void Camera::updateCameraMode(float deltaTime) {
    switch (currentMode) {
        case CameraMode::Cockpit:
            updateCockpitCamera();
            break;
        case CameraMode::Chase:
            updateChaseCamera(deltaTime);
            break;
        case CameraMode::ThirdPerson:
            updateThirdPersonCamera();
            break;
    }
}

// ============================================================================
// Mode-Specific Camera Updates
// ============================================================================

void Camera::updateCockpitCamera() {
    // Position camera at cockpit offset relative to car
    mat4 carTransform    = glm::translate(mat4(1.0f), targetPosition) * glm::toMat4(targetRotation);
    vec4 worldCockpitPos = carTransform * vec4(cockpitOffset, 1.0f);
    position             = vec3(worldCockpitPos);

    // Look direction matches car's forward direction
    // The car model's local forward after 90° X rotation is +Z in world space
    vec3 carForward = targetRotation * vec3(0.0f, 0.0f, 1.0f);  // Changed to +Z
    forward         = glm::normalize(carForward);
    right           = glm::normalize(glm::cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    up              = glm::normalize(glm::cross(right, forward));

    // Log camera position periodically (every 60 frames)
    static int frameCount = 0;
    if (++frameCount % 60 == 0) {  // Log every 60 frames (~1 second at 60fps)
        Log logger;
        logger.log("position",
            "Camera: (" + std::to_string(position.x) + ", " +
            std::to_string(position.y) + ", " +
            std::to_string(position.z) + ") | Car: (" +
            std::to_string(targetPosition.x) + ", " +
            std::to_string(targetPosition.y) + ", " +
            std::to_string(targetPosition.z) + ")");
    }
}

void Camera::updateChaseCamera(float deltaTime) {
    // Calculate target position behind car
    // The car model's local forward after 90° X rotation is +Z in world space
    vec3 carForward      = targetRotation * vec3(0.0f, 0.0f, 1.0f);  // Changed to +Z
    vec3 targetCameraPos = targetPosition - carForward * chaseDistance + vec3(0.0f, chaseHeight, 0.0f);

    // Smooth damping using Spring-Damper system
    // This creates a smooth follow with slight lag
    const float dampingRatio = 0.7f;  // Slightly underdamped for natural feel
    vec3        displacement = position - targetCameraPos;
    vec3        dampingForce = -2.0f * dampingRatio * chaseStiffness * chaseCurrentVelocity;
    vec3        springForce  = -chaseStiffness * displacement;
    vec3        acceleration = springForce + dampingForce;

    chaseCurrentVelocity += acceleration * deltaTime;
    position += chaseCurrentVelocity * deltaTime;

    // Look at car
    forward = glm::normalize(targetPosition - position);
    right   = glm::normalize(glm::cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    up      = glm::normalize(glm::cross(right, forward));
}

void Camera::updateThirdPersonCamera() {
    // Orbital camera around car
    // User can control orbit angle with mouse or it auto-rotates
    float orbitX = sin(glm::radians(thirdPersonAngle)) * thirdPersonDistance;
    float orbitZ = cos(glm::radians(thirdPersonAngle)) * thirdPersonDistance;

    position = targetPosition + vec3(orbitX, thirdPersonHeight, orbitZ);

    // Look at car
    forward = glm::normalize(targetPosition - position);
    right   = glm::normalize(glm::cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    up      = glm::normalize(glm::cross(right, forward));
}