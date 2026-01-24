// SPDX-License-Identifier: MIT
#include "Entity.h"

namespace DownPour {

Entity::Entity(const std::string& name, Scene* scene) : name(name), scene(scene), rootNode() {}

void Entity::addNode(NodeHandle node, const std::string& role) {
    if (!node.isValid()) {
        return;
    }

    // First node added becomes the root
    if (!rootNode.isValid()) {
        rootNode = node;
    }

    // Store named node role
    if (!role.empty()) {
        namedNodes[role] = node;
    }
}

NodeHandle Entity::getNode(const std::string& role) const {
    auto it = namedNodes.find(role);
    if (it != namedNodes.end()) {
        return it->second;
    }
    return NodeHandle{};  // Invalid handle
}

std::vector<NodeHandle> Entity::getAllNodes() const {
    std::vector<NodeHandle> nodes;
    nodes.push_back(rootNode);
    for (const auto& [role, handle] : namedNodes) {
        if (handle != rootNode) {  // Avoid duplicates
            nodes.push_back(handle);
        }
    }
    return nodes;
}

void Entity::setPosition(const glm::vec3& pos) {
    if (!scene || !rootNode.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(rootNode);
    if (node) {
        node->setLocalPosition(pos);
        scene->markSubtreeDirty(rootNode);
    }
}

void Entity::setRotation(const glm::quat& rot) {
    if (!scene || !rootNode.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(rootNode);
    if (node) {
        node->setLocalRotation(rot);
        scene->markSubtreeDirty(rootNode);
    }
}

void Entity::setScale(const glm::vec3& scale) {
    if (!scene || !rootNode.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(rootNode);
    if (node) {
        node->setLocalScale(scale);
        scene->markSubtreeDirty(rootNode);
    }
}

glm::vec3 Entity::getPosition() const {
    if (!scene || !rootNode.isValid()) {
        return glm::vec3(0.0f);
    }

    const SceneNode* node = scene->getNode(rootNode);
    return node ? node->localPosition : glm::vec3(0.0f);
}

glm::quat Entity::getRotation() const {
    if (!scene || !rootNode.isValid()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const SceneNode* node = scene->getNode(rootNode);
    return node ? node->localRotation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec3 Entity::getScale() const {
    if (!scene || !rootNode.isValid()) {
        return glm::vec3(1.0f);
    }

    const SceneNode* node = scene->getNode(rootNode);
    return node ? node->localScale : glm::vec3(1.0f);
}

void Entity::translate(const glm::vec3& delta) {
    setPosition(getPosition() + delta);
}

void Entity::rotate(const glm::quat& delta) {
    setRotation(delta * getRotation());
}

void Entity::animate(const std::string& role, const glm::mat4& localTransform) {
    if (!scene) {
        return;
    }

    NodeHandle handle = getNode(role);
    if (!handle.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(handle);
    if (node) {
        node->setLocalTransform(localTransform);
        scene->markSubtreeDirty(handle);
    }
}

void Entity::animatePosition(const std::string& role, const glm::vec3& position) {
    if (!scene) {
        return;
    }

    NodeHandle handle = getNode(role);
    if (!handle.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(handle);
    if (node) {
        node->setLocalPosition(position);
        scene->markSubtreeDirty(handle);
    }
}

void Entity::animateRotation(const std::string& role, const glm::quat& rotation) {
    if (!scene) {
        return;
    }

    NodeHandle handle = getNode(role);
    if (!handle.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(handle);
    if (node) {
        node->setLocalRotation(rotation);
        scene->markSubtreeDirty(handle);
    }
}

void Entity::animateScale(const std::string& role, const glm::vec3& scale) {
    if (!scene) {
        return;
    }

    NodeHandle handle = getNode(role);
    if (!handle.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(handle);
    if (node) {
        node->setLocalScale(scale);
        scene->markSubtreeDirty(handle);
    }
}

}  // namespace DownPour
