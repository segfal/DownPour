// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"
#include "../core/Types.h"

namespace DownPour {
using namespace DownPour::Types;

/**
 * @brief Represents a camera entity in the scene
 *
 * A camera entity that can be attached to other entities (like a car)
 * to follow their movement. Uses camera data from model metadata to
 * position itself correctly relative to the parent entity.
 */
class CameraEntity : public Entity {
public:
    using Entity::Entity;

    /**
     * @brief Configuration for camera positioning and orientation
     */
    struct CameraConfig {
        // Offset from parent entity in local space
        Vec3 localOffset   = Vec3(0.0f, 0.5f, 0.5f);

        // Local rotation (quaternion)
        Quat localRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);

        // Camera intrinsics    
        float fov         = 45.0f;  // Field of view in degrees
        float nearPlane   = 0.1f;
        float farPlane    = 1000.0f;
    };

    /**
     * @brief Set the camera configuration
     */
    void setConfig(const CameraConfig& cfg) { config = cfg; }

    /**
     * @brief Get the camera configuration
     */
    const CameraConfig& getConfig() const { return config; }
    CameraConfig& getConfig() { return config; }

    /**
     * @brief Set the local offset from parent
     */
    void setLocalOffset(const Vec3& offset) { config.localOffset = offset; }

    /**
     * @brief Get the local offset from parent
     */
    Vec3 getLocalOffset() const { return config.localOffset; }

    /**
     * @brief Set the local rotation
     */
    void setLocalRotation(const Quat& rotation) { config.localRotation = rotation; }

    /**
     * @brief Get the local rotation
     */
    Quat getLocalRotation() const { return config.localRotation; }

    /**
     * @brief Get world-space camera position
     *
     * Combines parent entity's world transform with local offset
     */
    Vec3 getWorldPosition() const;

    /**
     * @brief Get world-space camera orientation
     *
     * Combines parent entity's world rotation with local rotation
     */
    Quat getWorldRotation() const;

    /**
     * @brief Get forward direction in world space
     */
    Vec3 getWorldForward() const;

    /**
     * @brief Get up direction in world space
     */
    Vec3 getWorldUp() const;

    /**
     * @brief Get right direction in world space
     */
    Vec3 getWorldRight() const;

    /**
     * @brief Attach this camera to a parent entity
     *
     * Creates a child node under the parent's root node with the
     * local offset and rotation from config.
     */
    void attachToParent(Entity* parent);

private:
    CameraConfig config;
    Entity*      parentEntity = nullptr;
};

}  // namespace DownPour
