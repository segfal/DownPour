// SPDX-License-Identifier: MIT
#include "Entity.h"

#include "../core/Types.h"

#include <string>
namespace DownPour {
typedef std::string str;
using namespace DownPour::Types;

Entity::Entity(const str& name, Scene* scene) : name(name), scene(scene), rootNode() {}

void Entity::addNode(NodeHandle node, const str& role) {
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

NodeHandle Entity::getNode(const str& role) const {
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

void Entity::setPosition(const Vec3& pos) {
    if (!scene || !rootNode.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(rootNode);
    if (node) {
        node->setLocalPosition(pos);
        scene->markSubtreeDirty(rootNode);
    }
}

void Entity::setRotation(const Quat& rot) {
    if (!scene || !rootNode.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(rootNode);
    if (node) {
        node->setLocalRotation(rot);
        scene->markSubtreeDirty(rootNode);
    }
}

void Entity::setScale(const Vec3& scale) {
    if (!scene || !rootNode.isValid()) {
        return;
    }

    SceneNode* node = scene->getNode(rootNode);
    if (node) {
        node->setLocalScale(scale);
        scene->markSubtreeDirty(rootNode);
    }
}

Vec3 Entity::getPosition() const {
    if (!scene || !rootNode.isValid()) {
        return Vec3(0.0f);
    }

    const SceneNode* node = scene->getNode(rootNode);
    return node ? node->localPosition : Vec3(0.0f);
}

Quat Entity::getRotation() const {
    if (!scene || !rootNode.isValid()) {
        return Quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const SceneNode* node = scene->getNode(rootNode);
    return node ? node->localRotation : Quat(1.0f, 0.0f, 0.0f, 0.0f);
}

Vec3 Entity::getScale() const {
    if (!scene || !rootNode.isValid()) {
        return Vec3(1.0f);
    }

    const SceneNode* node = scene->getNode(rootNode);
    return node ? node->localScale : Vec3(1.0f);
}

void Entity::translate(const Vec3& delta) {
    setPosition(getPosition() + delta);
}

void Entity::rotate(const Quat& delta) {
    setRotation(delta * getRotation());
}

void Entity::animate(const str& role, const Mat4& localTransform) {
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

void Entity::animatePosition(const str& role, const Vec3& position) {
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

void Entity::animateRotation(const str& role, const Quat& rotation) {
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

void Entity::animateScale(const str& role, const Vec3& scale) {
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
