// SPDX-License-Identifier: MIT
#pragma once

#include "../core/Types.h"
#include "Entity.h"

struct GLFWwindow;

// Camera mode enumeration
enum class CameraMode { Cockpit, Chase, ThirdPerson };

namespace DownPour {
using namespace DownPour::Types;

// Forward declaration
class ModelAdapter;

/**
 * @brief Represents a camera entity in the scene
 *
 * A camera entity that can be attached to other entities (like a car)
 * to follow their movement. Uses camera data from model metadata to
 * position itself correctly relative to the parent entity.
 *
 * PHASE 3: Supports optional ModelAdapter reference for shared configuration.
 * Can work standalone (testing, procedural) or with shared config (main game).
 */
class CameraEntity : public Entity {
public:
    using Entity::Entity;

    /**
     * @brief Construct with optional config source (PHASE 3)
     * @param name Entity name
     * @param scene Scene pointer
     * @param configSource Optional ModelAdapter for shared config (nullptr = standalone)
     */
    CameraEntity(const str& name, Scene* scene, ModelAdapter* configSource);

    /**
     * @brief Configuration for camera positioning and orientation
     */
    struct CameraConfig {

        Vec3 localOffset = Vec3(0.0f, 0.5f, 0.5f); // local offset
        Quat localRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f); //local rotation
        float fov       = 45.0f;  // Field of view in degrees
        float nearPlane = 0.1f; // near plane
        float farPlane  = 1000.0f; // far plane
    };

    /**
     * @brief Configuration for chase camera behavior
     */
    struct ChaseConfig {
        float distance = 5.0f;           // Distance behind car
        float height = 1.5f;             // Height above car
        float stiffness = 5.0f;          // Spring stiffness for smoothing
        Vec3 currentVelocity = Vec3(0.0f); // For smooth damping
    };

    /**
     * @brief Configuration for third-person camera behavior
     */
    struct ThirdPersonConfig {
        float distance = 8.0f;
        float height = 3.0f;
        float angle = 0.0f;              // Orbital angle around car
    };
    

    /**
     * @brief Set the camera configuration
     */
    void setConfig(const CameraConfig& cfg) { config = cfg; }

    /**
     * @brief Get the camera configuration
     */
    const CameraConfig& getConfig() const { return config; }
    CameraConfig&       getConfig() { return config; }

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

    // ========== Camera Mode Management ==========
    
    /**
     * @brief Set the camera mode
     */
    void setMode(CameraMode mode);

    /**
     * @brief Get the current camera mode
     */
    CameraMode getMode() const { return currentMode; }

    /**
     * @brief Cycle through camera modes
     */
    void cycleMode();

    /**
     * @brief Update camera based on current mode
     */
    void updateCameraMode(float deltaTime);

    // ========== Matrix Generation ==========

    /**
     * @brief Get the view matrix for rendering
     */
    Mat4 getViewMatrix() const;

    /**
     * @brief Get the projection matrix for rendering
     */
    Mat4 getProjectionMatrix() const;

    /**
     * @brief Set the aspect ratio for projection matrix
     */
    void setAspectRatio(float aspect) { aspectRatio = aspect; }

    /**
     * @brief Get the current aspect ratio
     */
    float getAspectRatio() const { return aspectRatio; }

    // ========== Input Processing ==========

    /**
     * @brief Process keyboard input for camera control
     */
    void processInput(GLFWwindow* window, float deltaTime);

    /**
     * @brief Process mouse movement for camera control
     */
    void processMouseMovement(float xoffset, float yoffset);

    // ========== Camera Configuration ==========

    /**
     * @brief Set field of view in degrees
     */
    void setFOV(float fov) { config.fov = fov; }

    /**
     * @brief Get field of view in degrees
     */
    float getFOV() const { return config.fov; }

    /**
     * @brief Set near and far clipping planes
     */
    void setNearFar(float near, float far) {
        config.nearPlane = near;
        config.farPlane = far;
    }

    /**
     * @brief Set chase camera distance
     */
    void setChaseDistance(float distance) { chaseConfig.distance = distance; }

    /**
     * @brief Set chase camera height
     */
    void setChaseHeight(float height) { chaseConfig.height = height; }

    /**
     * @brief Set chase camera stiffness (spring constant)
     */
    void setChaseStiffness(float stiffness) { chaseConfig.stiffness = stiffness; }

    /**
     * @brief Set third-person camera distance
     */
    void setThirdPersonDistance(float distance) { thirdConfig.distance = distance; }

    /**
     * @brief Set third-person camera height
     */
    void setThirdPersonHeight(float height) { thirdConfig.height = height; }

    /**
     * @brief Set third-person camera angle
     */
    void setThirdPersonAngle(float angle) { thirdConfig.angle = angle; }

private:
    CameraConfig config;  // Camera configuration (FOV, near/far planes, local offset/rotation)
    CameraMode currentMode = CameraMode::Cockpit;

    Entity* parentEntity = nullptr;  // Parent entity this camera is attached to

    ChaseConfig chaseConfig;
    ThirdPersonConfig thirdConfig;

    float aspectRatio = 16.0f / 9.0f;  // Default aspect ratio
    float pitch = 0.0f;                 // For mouse look
    float yaw = 0.0f;                   // For mouse look

    // PHASE 3: Optional reference to shared config source
    // If set, config is initialized from this instead of defaults
    // If nullptr, entity works standalone with local config
    ModelAdapter* configSource = nullptr;

    // Mode-specific update methods (implemented in Phase 3)
    void updateCockpitMode();
    void updateChaseMode(float deltaTime);
    void updateThirdPersonMode();
};

}  // namespace DownPour
