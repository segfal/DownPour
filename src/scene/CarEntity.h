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

    const Config& getConfig() const { return config; }
    Config&       getConfig() { return config; }

    // Control methods (stubs for future implementation)
    void setLights(bool on) { /* Toggle emissive materials */ }
    void openDoor(Side side, bool open) { /* Animate door node rotation */ }
    void openHood(bool open) { /* Animate hood node rotation */ }

private:
    Config config;
};

}  // namespace DownPour
