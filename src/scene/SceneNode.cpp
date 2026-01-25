// SPDX-License-Identifier: MIT
#include "SceneNode.h"

#include "core/Types.h"

#include <glm/gtc/matrix_transform.hpp>

namespace DownPour {
using namespace DownPour::Types;

Mat4 SceneNode::getLocalTransform() const {
    // Build TRS matrix: T * R * S
    Mat4 transform = glm::translate(Mat4(1.0f), localPosition);
    transform      = transform * glm::mat4_cast(localRotation);
    transform      = glm::scale(transform, localScale);
    return transform;
}

void SceneNode::setLocalTransform(const Mat4& transform) {
    // Decompose matrix into TRS
    // Extract translation
    localPosition = Vec3(transform[3]);

    // Extract scale
    Vec3 scaleX = Vec3(transform[0]);
    Vec3 scaleY = Vec3(transform[1]);
    Vec3 scaleZ = Vec3(transform[2]);
    localScale  = Vec3(glm::length(scaleX), glm::length(scaleY), glm::length(scaleZ));

    // Extract rotation (remove scale from basis vectors)
    Mat3 rotationMatrix;
    rotationMatrix[0] = scaleX / localScale.x;
    rotationMatrix[1] = scaleY / localScale.y;
    rotationMatrix[2] = scaleZ / localScale.z;
    localRotation     = glm::quat_cast(rotationMatrix);

    isDirty = true;
}

void SceneNode::setLocalPosition(const Vec3& pos) {
    localPosition = pos;
    isDirty       = true;
}

void SceneNode::setLocalRotation(const Quat& rot) {
    localRotation = rot;
    isDirty       = true;
}

void SceneNode::setLocalScale(const Vec3& scale) {
    localScale = scale;
    isDirty    = true;
}

}  // namespace DownPour
