// SPDX-License-Identifier: MIT
#pragma once

#include "../core/Types.h"
#include "Entity.h"

struct GLFWwindow;

namespace DownPour {
using namespace DownPour::Types;

// Forward declaration
class ModelAdapter;

/**
 * @brief Represents a free-flying camera entity in the scene
 *
 * Simplified camera for minimal refactoring - supports free-fly movement
 * with WASD keys and mouse look (pitch/yaw rotation).
 */
class CameraEntity : public Entity {
public:
    using Entity::Entity;

    /**
     * @brief Construct with optional config source
     * @param name Entity name
     * @param scene Scene pointer
     * @param configSource Optional ModelAdapter for shared config (nullptr = standalone)
     */
    CameraEntity(const str& name, Scene* scene, ModelAdapter* configSource);

    /**
     * @brief Configuration for camera
     */
    struct CameraConfig {
        float fov       = 75.0f;    // Field of view in degrees
        float nearPlane = 0.1f;     // Near clipping plane
        float farPlane  = 10000.0f; // Far clipping plane
    };

    // Configuration
    void setConfig(const CameraConfig& cfg) { config = cfg; }
    const CameraConfig& getConfig() const { return config; }
    CameraConfig& getConfig() { return config; }

    void setFOV(float fov) { config.fov = fov; }
    float getFOV() const { return config.fov; }

    // World-space getters
    Vec3 getWorldPosition() const;
    Quat getWorldRotation() const;
    Vec3 getWorldForward() const;
    Vec3 getWorldUp() const;
    Vec3 getWorldRight() const;

    // Matrix generation
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix() const;

    void setAspectRatio(float aspect) { aspectRatio = aspect; }
    float getAspectRatio() const { return aspectRatio; }

    // Input processing (free-fly mode)
    void processInput(GLFWwindow* window, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset);

private:
    CameraConfig config;
    float aspectRatio = 16.0f / 9.0f;

    // Free-fly camera state
    float movementSpeed = 5.0f;  // m/s
    float yaw = -90.0f;          // Camera rotation (initialized to look forward)
    float pitch = 0.0f;          // Camera rotation

    ModelAdapter* configSource = nullptr;
};

}  // namespace DownPour
