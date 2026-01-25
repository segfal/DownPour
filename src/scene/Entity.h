// SPDX-License-Identifier: MIT
#pragma once

#include "Scene.h"
#include "SceneNode.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace DownPour {

// Note: Types are inherited from SceneNode.h which includes core/Types.h

/**
 * @brief High-level game object composed of multiple SceneNodes
 *
 * Represents a logical entity like a car, which consists of multiple
 * parts (body, wheels, doors, etc.) that share a common root transform.
 * Provides a convenient API for manipulating complex hierarchical objects.
 */
class Entity {
public:
    /**
     * @brief Construct a new Entity
     * @param name Entity name (e.g., "player_car", "enemy_01")
     * @param scene Scene that owns this entity's nodes
     */
    Entity(const std::string& name, Scene* scene);
    virtual ~Entity() = default;

    // Node management
    void                    addNode(NodeHandle node, const std::string& role = "");
    NodeHandle              getNode(const std::string& role) const;
    std::vector<NodeHandle> getAllNodes() const;

    // Root transform (applies to entire entity)
    void setPosition(const glm::vec3& pos);
    void setRotation(const glm::quat& rot);
    void setScale(const glm::vec3& scale);

    glm::vec3 getPosition() const;
    glm::quat getRotation() const;
    glm::vec3 getScale() const;

    // Convenience transform methods
    void translate(const glm::vec3& delta);
    void rotate(const glm::quat& delta);

    // Animation support (for specific parts)
    void animate(const std::string& role, const glm::mat4& localTransform);
    void animatePosition(const std::string& role, const glm::vec3& position);
    void animateRotation(const std::string& role, const glm::quat& rotation);
    void animateScale(const std::string& role, const glm::vec3& scale);

    // Accessors
    NodeHandle         getRootNode() const { return rootNode; }
    const std::string& getName() const { return name; }
    Scene*             getScene() const { return scene; }

private:
    std::string name;
    Scene*      scene;
    NodeHandle  rootNode;

    // Named node roles (e.g., "wheel_FL", "door_left", "steering_wheel")
    std::unordered_map<std::string, NodeHandle> namedNodes;
};

}  // namespace DownPour
