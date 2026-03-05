// SPDX-License-Identifier: MIT
#include "CameraEntity.h"

#include "../renderer/ModelAdapter.h"
#include "Scene.h"
#include "SceneNode.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <GLFW/glfw3.h>

namespace DownPour {
using namespace DownPour::Types;

CameraEntity::CameraEntity(const str& name, Scene* scene, ModelAdapter* configSource)
    : Entity(name, scene), configSource(configSource) {
    // If configSource provided, initialize config from it
    if (configSource) {
        const auto& camCfg = configSource->getCameraConfig();
        if (camCfg.hasData) {
            config.fov = camCfg.cockpit.fov;
            config.nearPlane = camCfg.cockpit.nearPlane;
            config.farPlane = camCfg.cockpit.farPlane;
        }
    }
}

Vec3 CameraEntity::getWorldPosition() const {
    if (!getRootNode().isValid() || !getScene()) {
        return Vec3(0.0f);
    }

    const SceneNode* node = getScene()->getNode(getRootNode());
    if (!node) {
        return Vec3(0.0f);
    }

    // Extract position from world transform
    return Vec3(node->worldTransform[3]);
}

Quat CameraEntity::getWorldRotation() const {
    if (!getRootNode().isValid() || !getScene()) {
        return Quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const SceneNode* node = getScene()->getNode(getRootNode());
    if (!node) {
        return Quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    // Decompose world transform to extract rotation
    Vec3 scale, translation, skew;
    Quat rotation;
    Vec4 perspective;
    glm::decompose(node->worldTransform, scale, rotation, translation, skew, perspective);

    return rotation;
}

Vec3 CameraEntity::getWorldForward() const {
    Quat rotation = getWorldRotation();
    return rotation * Vec3(0.0f, 0.0f, -1.0f);  // Forward is -Z
}

Vec3 CameraEntity::getWorldUp() const {
    Quat rotation = getWorldRotation();
    return rotation * Vec3(0.0f, 1.0f, 0.0f);  // Up is +Y
}

Vec3 CameraEntity::getWorldRight() const {
    Quat rotation = getWorldRotation();
    return rotation * Vec3(1.0f, 0.0f, 0.0f);  // Right is +X
}

Mat4 CameraEntity::getViewMatrix() const {
    Vec3 position = getWorldPosition();
    Vec3 forward = getWorldForward();
    Vec3 up = getWorldUp();

    return glm::lookAt(position, position + forward, up);
}

Mat4 CameraEntity::getProjectionMatrix() const {
    return glm::perspective(glm::radians(config.fov), aspectRatio, config.nearPlane, config.farPlane);
}

void CameraEntity::processInput(GLFWwindow* window, float deltaTime) {
    Scene* scene = getScene();
    if (!scene || !getRootNode().isValid()) return;

    SceneNode* node = scene->getNode(getRootNode());
    if (!node) return;

    Vec3 position = node->localPosition;
    Vec3 forward = getWorldForward();
    Vec3 right = getWorldRight();
    Vec3 up(0.0f, 1.0f, 0.0f);  // World up

    float velocity = movementSpeed * deltaTime;

    // WASD movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += right * velocity;

    // Vertical movement
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        position += up * velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        position -= up * velocity;

    node->localPosition = position;
    scene->markSubtreeDirty(getRootNode());
}

void CameraEntity::processMouseMovement(float xoffset, float yoffset) {
    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch -= yoffset;  // Inverted Y

    // Constrain pitch to prevent gimbal lock
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // Update camera rotation in scene graph
    Scene* scene = getScene();
    if (!scene || !getRootNode().isValid()) return;

    SceneNode* node = scene->getNode(getRootNode());
    if (!node) return;

    // Convert pitch/yaw to quaternion
    Quat pitchQuat = glm::angleAxis(glm::radians(pitch), Vec3(1.0f, 0.0f, 0.0f));
    Quat yawQuat = glm::angleAxis(glm::radians(yaw), Vec3(0.0f, 1.0f, 0.0f));
    Quat rotation = yawQuat * pitchQuat;

    node->localRotation = rotation;
    scene->markSubtreeDirty(getRootNode());
}

void CameraEntity::setInitialOrientation(float yawDeg, float pitchDeg) {
    yaw = yawDeg;
    pitch = glm::clamp(pitchDeg, -89.0f, 89.0f);

    // Apply to node so the camera is oriented correctly before any mouse movement
    Scene* scene = getScene();
    if (!scene || !getRootNode().isValid()) return;

    SceneNode* node = scene->getNode(getRootNode());
    if (!node) return;

    Quat pitchQuat = glm::angleAxis(glm::radians(pitch), Vec3(1.0f, 0.0f, 0.0f));
    Quat yawQuat = glm::angleAxis(glm::radians(yaw), Vec3(0.0f, 1.0f, 0.0f));
    node->localRotation = yawQuat * pitchQuat;
    scene->markSubtreeDirty(getRootNode());
}

}  // namespace DownPour
