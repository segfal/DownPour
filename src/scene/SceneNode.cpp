// SPDX-License-Identifier: MIT
#include "SceneNode.h"

#include <glm/gtc/matrix_transform.hpp>

namespace DownPour {

glm::mat4 SceneNode::getLocalTransform() const {
    // Build TRS matrix: T * R * S
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), localPosition);
    transform           = transform * glm::mat4_cast(localRotation);
    transform           = glm::scale(transform, localScale);
    return transform;
}

void SceneNode::setLocalTransform(const glm::mat4& transform) {
    // Decompose matrix into TRS
    // Extract translation
    localPosition = glm::vec3(transform[3]);

    // Extract scale
    glm::vec3 scaleX = glm::vec3(transform[0]);
    glm::vec3 scaleY = glm::vec3(transform[1]);
    glm::vec3 scaleZ = glm::vec3(transform[2]);
    localScale       = glm::vec3(glm::length(scaleX), glm::length(scaleY), glm::length(scaleZ));

    // Extract rotation (remove scale from basis vectors)
    glm::mat3 rotationMatrix;
    rotationMatrix[0] = scaleX / localScale.x;
    rotationMatrix[1] = scaleY / localScale.y;
    rotationMatrix[2] = scaleZ / localScale.z;
    localRotation     = glm::quat_cast(rotationMatrix);

    isDirty = true;
}

void SceneNode::setLocalPosition(const glm::vec3& pos) {
    localPosition = pos;
    isDirty       = true;
}

void SceneNode::setLocalRotation(const glm::quat& rot) {
    localRotation = rot;
    isDirty       = true;
}

void SceneNode::setLocalScale(const glm::vec3& scale) {
    localScale = scale;
    isDirty    = true;
}

}  // namespace DownPour
