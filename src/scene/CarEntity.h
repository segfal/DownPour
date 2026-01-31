// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"
#include "SceneNode.h"

#include <unordered_map>

namespace DownPour {

// Forward declaration to avoid circular dependency
class ModelAdapter;

/**
 * @brief Represents a car entity in the scene
 *
 * Provides specialized access to car parts like wheels, steering wheel, and wipers.
 * This class inherits from Entity and defines standard roles for car components.
 *
 * PHASE 3: Supports optional ModelAdapter reference for shared configuration.
 * Can work standalone (testing, procedural) or with shared config (main game).
 */
class CarEntity : public Entity {
public:
    using Entity::Entity;

    /**
     * @brief Construct with optional config source (PHASE 3)
     * @param name Entity name
     * @param scene Scene pointer
     * @param configSource Optional ModelAdapter for shared config (nullptr = standalone)
     */
    CarEntity(const str& name, Scene* scene, ModelAdapter* configSource);


    // Standard roles for car nodes
    static constexpr const char* ROLE_WHEEL_FL             = "wheel_FL";
    static constexpr const char* ROLE_WHEEL_FR             = "wheel_FR";
    static constexpr const char* ROLE_WHEEL_RL             = "wheel_RL";
    static constexpr const char* ROLE_WHEEL_RR             = "wheel_RR";
    static constexpr const char* ROLE_STEERING_WHEEL_FRONT = "steering_wheel_front";
    static constexpr const char* ROLE_STEERING_WHEEL_BACK  = "steering_wheel_back";
    static constexpr const char* ROLE_WIPER_LEFT           = "left_wiper";
    static constexpr const char* ROLE_WIPER_RIGHT          = "right_wiper";
    static constexpr const char* ROLE_HOOD                 = "hood";
    static constexpr const char* ROLE_DOOR_L               = "left_door";
    static constexpr const char* ROLE_DOOR_R               = "right_door";
    static constexpr const char* ROLE_HEADLIGHTS           = "headlights";
    static constexpr const char* ROLE_TAILLIGHTS           = "taillights";

    enum class Side { Left, Right };

    // Sub-structure for car specific state/properties
    // NO HARDCODED DEFAULTS! All values must come from GLB model's JSON configuration.
    // Sentinel value 0.0f indicates "not configured" - will cause validation error.
    struct Config {
        // Physical dimensions (meters) - REQUIRED from JSON
        float wheelBase   = 0.0f;  // Distance between front and rear axles
        float trackWidth  = 0.0f;  // Distance between left and right wheels
        float wheelRadius = 0.0f;  // Radius of the wheels
        float length      = 0.0f;  // Total car length (set from ModelAdapter.targetLength)

        // Steering & Dynamics - REQUIRED from JSON
        float maxSteerAngle   = 0.0f;  // degrees
        float maxAcceleration = 0.0f;  // m/s^2
        float maxBraking      = 0.0f;  // m/s^2

        // Advanced physics - REQUIRED from JSON
        float mass              = 0.0f;
        float dragCoefficient   = 0.0f;
        float rollingResistance = 0.0f;

        // Animation offsets/limits - REQUIRED from JSON
        float doorOpenAngle = 0.0f;  // degrees
        float hoodOpenAngle = 0.0f;  // degrees
    };

    // ========================================================================
    // Configuration Getters/Setters
    // ========================================================================
    const Config& getConfig() const { return config; }
    Config&       getConfig() { return config; }
    void          setConfig(const Config& cfg) { config = cfg; }

    /**
     * @brief Validate that all required config values are present
     * @throws std::runtime_error if any required config is missing (0.0f sentinel value)
     */
    void validateConfig() const;

    // ========================================================================
    // Node Getters (for specific car parts)
    // ========================================================================
    NodeHandle getWheelNode(Side side, bool front) const;
    NodeHandle getSteeringWheelFrontNode() const;
    NodeHandle getSteeringWheelBackNode() const;
    NodeHandle getWiperNode(Side side) const;
    NodeHandle getHeadlightsNode() const;
    NodeHandle getTaillightsNode() const;
    NodeHandle getHoodNode() const;
    NodeHandle getDoorNode(Side side) const;

    // ========================================================================
    // Physics/State Getters (PHASE 3: Smart getters query configSource if available)
    // ========================================================================
    float getWheelBase() const;
    float getTrackWidth() const;
    float getWheelRadius() const;
    float getMaxSteerAngle() const;
    float getMaxAcceleration() const;
    float getMaxBraking() const;
    float getMass() const;
    float getDragCoefficient() const;
    float getRollingResistance() const;
    float getDoorOpenAngle() const;
    float getLength() const { return config.length; }  // Always from local config

    // ========================================================================
    // Control Methods (Animation/State Control)
    // ========================================================================
    void setSteeringAngle(float degrees);  // Rotate steering wheel
    void setWheelRotation(float radians);  // Rotate all wheels (for driving)
    void setWiperAngle(float degrees);     // Control wiper animation
    void setLights(bool on);               // Toggle emissive materials
    void openDoor(Side side, bool open);   // Animate door rotation
    void openHood(bool open);              // Animate hood rotation

    // ========================================================================
    // Base Rotation Management
    // ========================================================================
    // Call after entity is built to capture initial rotations from GLTF model
    void captureBaseRotations();

    // ========================================================================
    // State Tracking
    // ========================================================================
    float getCurrentSteeringAngle() const { return currentSteeringAngle; }
    float getCurrentWheelRotation() const { return currentWheelRotation; }
    float getCurrentWiperAngle() const { return currentWiperAngle; }

private:
    Config config;

    // Current animation state
    float currentSteeringAngle = 0.0f;  // degrees
    float currentWheelRotation = 0.0f;  // radians (accumulated)
    float currentWiperAngle    = 0.0f;  // degrees

    // Base rotations from model (captured after loading)
    // Used to combine with animation rotations to preserve original mesh orientation
    std::unordered_map<std::string, Quat> baseRotations;

    // PHASE 3: Optional reference to shared config source
    // If set, getters query this instead of local config
    // If nullptr, entity works standalone with local config
    ModelAdapter* configSource = nullptr;
};

}  // namespace DownPour
