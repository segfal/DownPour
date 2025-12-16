// SPDX-License-Identifier: MIT
#include "CameraEntity.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace DownPour {
using namespace DownPour::Types;

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

    parentEntity = parent;
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

}  // namespace DownPour
