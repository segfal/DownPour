// SPDX-License-Identifier: MIT
#include "CameraEntity.h"

#include "../renderer/ModelAdapter.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>  // For lerp
#include <GLFW/glfw3.h>  // For input handling

namespace DownPour {
using namespace DownPour::Types;

// ============================================================================
// PHASE 3: Constructor with Optional Config Source
// ============================================================================

CameraEntity::CameraEntity(const str& name, Scene* scene, ModelAdapter* configSource)
    : Entity(name, scene), configSource(configSource) {
    // If configSource provided, initialize config from it
    if (configSource) {
        const auto& camCfg = configSource->getCameraConfig();
        if (camCfg.hasData) {
            // Cockpit camera config
            const auto& cockpit = camCfg.cockpit;
            config.localOffset = cockpit.position;
            if (cockpit.useQuaternion) {
                config.localRotation = cockpit.rotation;
            } else {
                config.localRotation = glm::quat(glm::radians(cockpit.eulerRotation));
            }
            config.fov = cockpit.fov;
            config.nearPlane = cockpit.nearPlane;
            config.farPlane = cockpit.farPlane;

            // Chase camera config
            chaseConfig.distance = camCfg.chase.distance;
            chaseConfig.height = camCfg.chase.height;
            chaseConfig.stiffness = camCfg.chase.stiffness;

            // Third-person camera config
            thirdConfig.distance = camCfg.thirdPerson.distance;
            thirdConfig.height = camCfg.thirdPerson.height;
            thirdConfig.angle = camCfg.thirdPerson.angle;
        }
    }
    // Otherwise, entity uses default config values from member initialization
}

// ============================================================================
// Position and Rotation Queries
// ============================================================================

Vec3 CameraEntity::getWorldPosition() const {
    Scene* scenePtr = getScene();
    if (!scenePtr || !getRootNode().isValid()) {
        return config.localOffset;
    }

    // Get the camera node's world transform
    const SceneNode* node = scenePtr->getNode(getRootNode());
    if (!node) {
        return config.localOffset;
    }

    // Extract position from world transform
    return Vec3(node->worldTransform[3]);
}

Quat CameraEntity::getWorldRotation() const {
    Scene* scenePtr = getScene();
    if (!scenePtr || !getRootNode().isValid()) {
        return config.localRotation;
    }

    // Get the camera node's world transform
    const SceneNode* node = scenePtr->getNode(getRootNode());
    if (!node) {
        return config.localRotation;
    }

    // Extract rotation from world transform
    Vec3 scale;
    Quat rotation;
    Vec3 translation;
    Vec3 skew;
    Vec4 perspective;
    glm::decompose(node->worldTransform, scale, rotation, translation, skew, perspective);

    return rotation;
}

Vec3 CameraEntity::getWorldForward() const {
    Quat worldRot = getWorldRotation();
    // Forward is -Z in camera space
    return glm::rotate(worldRot, Vec3(0.0f, 0.0f, -1.0f));
}

Vec3 CameraEntity::getWorldUp() const {
    Quat worldRot = getWorldRotation();
    // Up is +Y in camera space
    return glm::rotate(worldRot, Vec3(0.0f, 1.0f, 0.0f));
}

Vec3 CameraEntity::getWorldRight() const {
    Quat worldRot = getWorldRotation();
    // Right is +X in camera space
    return glm::rotate(worldRot, Vec3(1.0f, 0.0f, 0.0f));
}

void CameraEntity::attachToParent(Entity* parent) {
    if (!parent || !parent->getScene()) {
        return;
    }

    parentEntity    = parent;
    Scene* scenePtr = parent->getScene();

    // Create a camera node as a child of the parent's root
    NodeHandle parentRoot = parent->getRootNode();
    if (!parentRoot.isValid()) {
        return;
    }

    // Create the camera node
    NodeHandle cameraNode = scenePtr->createNode(getName() + "_camera_node", parentRoot);
    if (!cameraNode.isValid()) {
        return;
    }

    // Set the local transform from offset and rotation
    SceneNode* node = scenePtr->getNode(cameraNode);
    if (node) {
        node->setLocalPosition(config.localOffset);
        node->setLocalRotation(config.localRotation);
        scenePtr->markSubtreeDirty(cameraNode);
    }

    // Store as our root node
    addNode(cameraNode, "camera_root");
}

// ========== Matrix Generation ==========

Mat4 CameraEntity::getViewMatrix() const {
    // Use scene graph world position and rotation
    Vec3 worldPos = getWorldPosition();
    Vec3 forward = getWorldForward();
    Vec3 up = getWorldUp();
    
    // Create look-at matrix
    return glm::lookAt(worldPos, worldPos + forward, up);
}

Mat4 CameraEntity::getProjectionMatrix() const {
    return glm::perspective(
        glm::radians(config.fov),
        aspectRatio,
        config.nearPlane,
        config.farPlane
    );
}

// ========== Camera Mode Management ==========

void CameraEntity::setMode(CameraMode mode) {
    currentMode = mode;
}

void CameraEntity::cycleMode() {
    switch (currentMode) {
        case CameraMode::Cockpit:
            currentMode = CameraMode::Chase;
            break;
        case CameraMode::Chase:
            currentMode = CameraMode::ThirdPerson;
            break;
        case CameraMode::ThirdPerson:
            currentMode = CameraMode::Cockpit;
            break;
    }
}

void CameraEntity::updateCameraMode(float deltaTime) {
    switch (currentMode) {
        case CameraMode::Cockpit:
            updateCockpitMode();
            break;
        case CameraMode::Chase:
            updateChaseMode(deltaTime);
            break;
        case CameraMode::ThirdPerson:
            updateThirdPersonMode();
            break;
    }
}

// ========== Mode-Specific Updates (Stubs for Phase 3) ==========

void CameraEntity::updateCockpitMode() {
    // Nothing to do! Scene graph automatically handles parent following.
    // Camera is attached as child with localOffset.
    // worldTransform = parentTransform * localTransform
}

void CameraEntity::updateChaseMode(float deltaTime) {
    if (!parentEntity) return;
    
    // Get parent car's position and rotation (from scene graph)
    Vec3 carPos = parentEntity->getPosition();
    Quat carRot = parentEntity->getRotation();
    
    // Calculate target position behind car
    // Car's local forward after model rotation is +Z in world space
    Vec3 carForward = glm::rotate(carRot, Vec3(0.0f, 0.0f, 1.0f));
    Vec3 targetCameraPos = carPos - carForward * chaseConfig.distance + Vec3(0.0f, chaseConfig.height, 0.0f);
    
    // Smooth damping using Spring-Damper system
    // This creates a smooth follow with slight lag
    const float dampingRatio = 0.7f;  // Slightly underdamped for natural feel
    Vec3 currentPos = getWorldPosition();
    Vec3 displacement = currentPos - targetCameraPos;
    Vec3 dampingForce = -2.0f * dampingRatio * chaseConfig.stiffness * chaseConfig.currentVelocity;
    Vec3 springForce = -chaseConfig.stiffness * displacement;
    Vec3 acceleration = springForce + dampingForce;
    
    chaseConfig.currentVelocity += acceleration * deltaTime;
    Vec3 newPos = currentPos + chaseConfig.currentVelocity * deltaTime;
    
    // Update local offset (convert from world to parent-relative)
    // For now, set world position directly via local offset
    // TODO: Properly convert world position to local offset relative to parent
    setLocalOffset(newPos - carPos);
    
    // Update scene graph
    Scene* scene = getScene();
    if (scene && getRootNode().isValid()) {
        SceneNode* node = scene->getNode(getRootNode());
        if (node) {
            node->setLocalPosition(newPos - carPos);
            scene->markSubtreeDirty(getRootNode());
        }
    }
}

void CameraEntity::updateThirdPersonMode() {
    if (!parentEntity) return;
    
    // Get parent car's position (from scene graph)
    Vec3 carPos = parentEntity->getPosition();
    
    // Orbital camera around car
    // User can control orbit angle with mouse or it auto-rotates
    float orbitX = sin(glm::radians(thirdConfig.angle)) * thirdConfig.distance;
    float orbitZ = cos(glm::radians(thirdConfig.angle)) * thirdConfig.distance;
    
    Vec3 newPos = carPos + Vec3(orbitX, thirdConfig.height, orbitZ);
    
    // Update local offset (convert from world to parent-relative)
    setLocalOffset(newPos - carPos);
    
    // Update scene graph
    Scene* scene = getScene();
    if (scene && getRootNode().isValid()) {
        SceneNode* node = scene->getNode(getRootNode());
        if (node) {
            node->setLocalPosition(newPos - carPos);
            scene->markSubtreeDirty(getRootNode());
        }
    }
}

// ========== Input Processing (Stubs for Phase 4) ==========

void CameraEntity::processInput(GLFWwindow* window, float deltaTime) {
    // Input handling primarily for debugging/free camera mode
    // In normal gameplay, camera follows the car automatically
    const float cameraSpeed = 2.5f * deltaTime;  // Frame-rate independent movement
    
    Vec3 pos = getWorldPosition();
    Vec3 forward = getWorldForward();
    Vec3 right = getWorldRight();
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        pos += cameraSpeed * forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        pos -= cameraSpeed * forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        pos -= cameraSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        pos += cameraSpeed * right;
    
    // Update position (only works if not attached to parent)
    if (!parentEntity) {
        setLocalOffset(pos);
        Scene* scene = getScene();
        if (scene && getRootNode().isValid()) {
            SceneNode* node = scene->getNode(getRootNode());
            if (node) {
                node->setLocalPosition(pos);
                scene->markSubtreeDirty(getRootNode());
            }
        }
    }
}

void CameraEntity::processMouseMovement(float xoffset, float yoffset) {
    // Mouse movement controls third-person orbital angle
    if (currentMode == CameraMode::ThirdPerson) {
        const float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        
        // Update orbital angle
        thirdConfig.angle += xoffset;
        
        // Constrain angle to 0-360 range
        if (thirdConfig.angle > 360.0f)
            thirdConfig.angle -= 360.0f;
        if (thirdConfig.angle < 0.0f)
            thirdConfig.angle += 360.0f;
    }
    // Other modes can implement pitch/yaw if needed
    else {
        const float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        
        yaw += xoffset;
        pitch += yoffset;
        
        // Constrain pitch to prevent screen flip
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
}

}  // namespace DownPour
