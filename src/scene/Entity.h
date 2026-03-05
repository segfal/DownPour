// SPDX-License-Identifier: MIT
#pragma once

#include "../core/Types.h"
#include "Scene.h"
#include "SceneNode.h"

#include <string>
#include <unordered_map>
#include <vector>
namespace DownPour {
typedef std::string str;
using namespace DownPour::Types;


class Entity {
public:

    Entity(const str& name, Scene* scene);
    virtual ~Entity() = default;

    // Node management
    void                    addNode(NodeHandle node, const str& role = "");
    NodeHandle              getNode(const str& role) const;
    std::vector<NodeHandle> getAllNodes() const;

    // Root transform (applies to entire entity)
    void setPosition(const Vec3& pos);
    void setRotation(const Quat& rot);
    void setScale(const Vec3& scale);

    Vec3 getPosition() const;
    Quat getRotation() const;
    Vec3 getScale() const;

    // Convenience transform methods
    void translate(const Vec3& delta);
    void rotate(const Quat& delta);

    // Animation support (for specific parts)
    void animate(const str& role, const Mat4& localTransform);
    void animatePosition(const str& role, const Vec3& position);
    void animateRotation(const str& role, const Quat& rotation);
    void animateScale(const str& role, const Vec3& scale);

    // Accessors
    NodeHandle getRootNode() const { return rootNode; }
    const str& getName() const { return name; }
    Scene*     getScene() const { return scene; }

private:
    str        name;
    Scene*     scene;
    NodeHandle rootNode;
    Vec3 position = Vec3(0.0f);
    Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    Vec3 scale = Vec3(1.0f);
    // Named node roles (e.g., "wheel_FL", "door_left", "steering_wheel")
    std::unordered_map<str, NodeHandle> namedNodes;
};

}  // namespace DownPour
