// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"

namespace DownPour {

/**
 * @brief Represents a car entity in the scene
 *
 * Provides specialized access to car parts like wheels, steering wheel, and wipers.
 * This class inherits from Entity and defines standard roles for car components.
 */
class CarEntity : public Entity {
public:
    using Entity::Entity;

    // Standard roles for car nodes
    static constexpr const char* ROLE_WHEEL_FL       = "wheel_FL";
    static constexpr const char* ROLE_WHEEL_FR       = "wheel_FR";
    static constexpr const char* ROLE_WHEEL_RL       = "wheel_RL";
    static constexpr const char* ROLE_WHEEL_RR       = "wheel_RR";
    static constexpr const char* ROLE_STEERING_WHEEL = "steering_wheel";
    static constexpr const char* ROLE_WIPER_LEFT     = "left_wiper";
    static constexpr const char* ROLE_WIPER_RIGHT    = "right_wiper";
    static constexpr const char* ROLE_HOOD           = "hood";
    static constexpr const char* ROLE_DOOR_L         = "left_door";
    static constexpr const char* ROLE_DOOR_R         = "right_door";
    static constexpr const char* ROLE_HEADLIGHTS     = "headlights";
    static constexpr const char* ROLE_TAILLIGHTS     = "taillights";

    enum class Side { Left, Right };

    // Sub-structure for car specific state/properties for easier editing
    struct Config {
        // Physical dimensions (meters)
        float wheelBase   = 2.85f;  // Distance between front and rear axles
        float trackWidth  = 1.60f;  // Distance between left and right wheels
        float wheelRadius = 0.35f;  // Radius of the wheels
        float length      = 4.70f;  // Total car length

        // Steering & Dynamics
        float maxSteerAngle   = 35.0f;  // degrees
        float maxAcceleration = 5.0f;   // m/s^2
        float maxBraking      = 8.0f;   // m/s^2

        // Animation offsets/limits
        float doorOpenAngle = 45.0f;  // degrees
        float hoodOpenAngle = 30.0f;  // degrees
    };

    // ========================================================================
    // Configuration Getters/Setters
    // ========================================================================
    const Config& getConfig() const { return config; }
    Config&       getConfig() { return config; }
    void          setConfig(const Config& cfg) { config = cfg; }

    // ========================================================================
    // Node Getters (for specific car parts)
    // ========================================================================
    NodeHandle getWheelNode(Side side, bool front) const;
    NodeHandle getSteeringWheelNode() const;
    NodeHandle getWiperNode(Side side) const;
    NodeHandle getHeadlightsNode() const;
    NodeHandle getTaillightsNode() const;
    NodeHandle getHoodNode() const;
    NodeHandle getDoorNode(Side side) const;

    // ========================================================================
    // Physics/State Getters
    // ========================================================================
    float getWheelBase() const { return config.wheelBase; }
    float getTrackWidth() const { return config.trackWidth; }
    float getWheelRadius() const { return config.wheelRadius; }
    float getMaxSteerAngle() const { return config.maxSteerAngle; }
    float getLength() const { return config.length; }

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
};

}  // namespace DownPour
