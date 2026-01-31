// SPDX-License-Identifier: MIT
#include "CarEntity.h"

#include "../logger/Logger.h"
#include "../renderer/ModelAdapter.h"
#include "SceneNode.h"

#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <stdexcept>
#include <vector>

namespace DownPour {

// ============================================================================
// PHASE 3: Constructor with Optional Config Source
// ============================================================================

CarEntity::CarEntity(const str& name, Scene* scene, ModelAdapter* configSource)
    : Entity(name, scene), configSource(configSource) {
    // Entity initialized with optional config source
    // If configSource is set, getters will query it instead of local config
    // If nullptr, entity works standalone (must set config manually)

    // If using ModelAdapter, validate that all required config is present
    if (configSource) {
        validateConfig();
    }
}

// ============================================================================
// Configuration Validation
// ============================================================================

void CarEntity::validateConfig() const {
    if (!configSource) {
        return;  // No validation needed for standalone entities
    }

    const auto& phys = configSource->getPhysicsConfig();
    const auto& doors = configSource->getDoorsConfig();

    // Check physics config is present
    if (!phys.hasData) {
        throw std::runtime_error(
            "CarEntity '" + getName() + "': ModelAdapter missing physics configuration! "
            "Ensure GLB model has accompanying JSON with 'physics' section."
        );
    }

    // Validate all required physics properties
    std::vector<std::pair<std::string, float>> requiredProps = {
        {"wheelBase", phys.wheelBase},
        {"trackWidth", phys.trackWidth},
        {"wheelRadius", phys.wheelRadius},
        {"maxSteerAngle", phys.maxSteerAngle},
        {"maxAcceleration", phys.maxAcceleration},
        {"maxBraking", phys.maxBraking},
        {"mass", phys.mass},
        {"dragCoefficient", phys.dragCoefficient},
        {"rollingResistance", phys.rollingResistance}
    };

    for (const auto& [propName, value] : requiredProps) {
        if (value <= 0.0f) {
            throw std::runtime_error(
                "CarEntity '" + getName() + "': Required physics property '" + propName +
                "' is missing or invalid (value: " + std::to_string(value) + "). "
                "Check GLB model's JSON configuration."
            );
        }
    }

    // Check door config (optional but recommended)
    if (!doors.hasData || doors.openAngle <= 0.0f) {
        // Log warning but don't fail - doors might not be animated
        Log logger;
        logger.log("warning", "CarEntity '" + getName() + "': Door configuration missing. "
                              "Doors will not be animatable.");
    }

    // Note: hoodOpenAngle is not yet loaded from JSON by ModelAdapter
    // TODO: Add hood config support to ModelAdapter

    // Success - all required config present
    Log logger;
    logger.log("info", "CarEntity '" + getName() + "' config validated: "
                       "wheelBase=" + std::to_string(phys.wheelBase) + "m, "
                       "mass=" + std::to_string(phys.mass) + "kg, "
                       "maxAccel=" + std::to_string(phys.maxAcceleration) + "m/s²");
}

// ============================================================================
// PHASE 3: Smart Getters (Query configSource if available, else local config)
// ============================================================================

float CarEntity::getWheelBase() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.wheelBase > 0.0f) {
            return phys.wheelBase;
        }
    }
    return config.wheelBase;
}

float CarEntity::getTrackWidth() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.trackWidth > 0.0f) {
            return phys.trackWidth;
        }
    }
    return config.trackWidth;
}

float CarEntity::getWheelRadius() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.wheelRadius > 0.0f) {
            return phys.wheelRadius;
        }
    }
    return config.wheelRadius;
}

float CarEntity::getMaxSteerAngle() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.maxSteerAngle > 0.0f) {
            return phys.maxSteerAngle;
        }
    }
    return config.maxSteerAngle;
}

float CarEntity::getMaxAcceleration() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.maxAcceleration > 0.0f) {
            return phys.maxAcceleration;
        }
    }
    return config.maxAcceleration;
}

float CarEntity::getMaxBraking() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.maxBraking > 0.0f) {
            return phys.maxBraking;
        }
    }
    return config.maxBraking;
}

float CarEntity::getMass() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData && phys.mass > 0.0f) {
            return phys.mass;
        }
    }
    return config.mass;
}

float CarEntity::getDragCoefficient() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData) {
            return phys.dragCoefficient;
        }
    }
    return config.dragCoefficient;
}

float CarEntity::getRollingResistance() const {
    if (configSource) {
        const auto& phys = configSource->getPhysicsConfig();
        if (phys.hasData) {
            return phys.rollingResistance;
        }
    }
    return config.rollingResistance;
}

float CarEntity::getDoorOpenAngle() const {
    if (configSource) {
        const auto& doors = configSource->getDoorsConfig();
        if (doors.hasData && doors.openAngle > 0.0f) {
            return doors.openAngle;
        }
    }
    return config.doorOpenAngle;
}

// ============================================================================
// Node Getters
// ============================================================================

NodeHandle CarEntity::getWheelNode(Side side, bool front) const {
    const char* role = nullptr;
    if (front) {
        role = (side == Side::Left) ? ROLE_WHEEL_FL : ROLE_WHEEL_FR;
    } else {
        role = (side == Side::Left) ? ROLE_WHEEL_RL : ROLE_WHEEL_RR;
    }
    return getNode(role);
}

NodeHandle CarEntity::getSteeringWheelFrontNode() const {
    return getNode(ROLE_STEERING_WHEEL_FRONT);
}

NodeHandle CarEntity::getSteeringWheelBackNode() const {
    return getNode(ROLE_STEERING_WHEEL_BACK);
}

NodeHandle CarEntity::getWiperNode(Side side) const {
    const char* role = (side == Side::Left) ? ROLE_WIPER_LEFT : ROLE_WIPER_RIGHT;
    return getNode(role);
}

NodeHandle CarEntity::getHeadlightsNode() const {
    return getNode(ROLE_HEADLIGHTS);
}

NodeHandle CarEntity::getTaillightsNode() const {
    return getNode(ROLE_TAILLIGHTS);
}

NodeHandle CarEntity::getHoodNode() const {
    return getNode(ROLE_HOOD);
}

NodeHandle CarEntity::getDoorNode(Side side) const {
    const char* role = (side == Side::Left) ? ROLE_DOOR_L : ROLE_DOOR_R;
    return getNode(role);
}

// ============================================================================
// Control Methods
// ============================================================================

void CarEntity::setSteeringAngle(float degrees) {
    // Store the steering wheel visual rotation (not clamped to road wheel limits)
    // The 450 degree limit is managed by the caller in DownPour.cpp
    currentSteeringAngle = degrees;

    if (!getScene()) {
        return;
    }

    // Animation rotation around local X-axis (trackball-style rotation in wheel's local space)
    glm::quat animRot = glm::angleAxis(glm::radians(currentSteeringAngle), glm::vec3(1.0f, 0.0f, 0.0f));

    // Combine with base rotation: base * anim (animation in local space)
    // This preserves the original mesh orientation while adding the steering rotation
    auto itFront = baseRotations.find(ROLE_STEERING_WHEEL_FRONT);
    if (itFront != baseRotations.end()) {
        glm::quat combined = itFront->second * animRot;
        animateRotation(ROLE_STEERING_WHEEL_FRONT, combined);
    }

    auto itBack = baseRotations.find(ROLE_STEERING_WHEEL_BACK);
    if (itBack != baseRotations.end()) {
        glm::quat combined = itBack->second * animRot;
        animateRotation(ROLE_STEERING_WHEEL_BACK, combined);
    }
}

void CarEntity::setWheelRotation(float radians) {
    currentWheelRotation = radians;

    if (!getScene()) {
        return;
    }

    // Rotate all 4 wheels around their local X-axis
    glm::quat rotation = glm::angleAxis(radians, glm::vec3(1.0f, 0.0f, 0.0f));

    // Apply to all wheels
    const char* wheelRoles[] = {ROLE_WHEEL_FL, ROLE_WHEEL_FR, ROLE_WHEEL_RL, ROLE_WHEEL_RR};
    for (const char* role : wheelRoles) {
        NodeHandle wheel = getNode(role);
        if (wheel.isValid()) {
            animateRotation(role, rotation);
        }
    }
}

void CarEntity::setWiperAngle(float degrees) {
    currentWiperAngle = degrees;

    if (!getScene()) {
        return;
    }

    // Animate wiper rotation (typically around Y-axis for windshield wipers)
    glm::quat rotation = glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));

    // Apply to both wipers (might have different base angles)
    NodeHandle leftWiper = getWiperNode(Side::Left);
    if (leftWiper.isValid()) {
        animateRotation(ROLE_WIPER_LEFT, rotation);
    }

    NodeHandle rightWiper = getWiperNode(Side::Right);
    if (rightWiper.isValid()) {
        // Right wiper might mirror the left one
        animateRotation(ROLE_WIPER_RIGHT, rotation);
    }
}

void CarEntity::setLights(bool on) {
    // TODO: Toggle emissive materials for headlights and taillights
    // This requires MaterialManager integration to modify material properties
    // For now, this is a stub that can be implemented when material
    // property modification is added to MaterialManager

    if (!getScene()) {
        return;
    }

    // Placeholder: Could modify emissive strength or toggle visibility
    // NodeHandle headlights = getHeadlightsNode();
    // NodeHandle taillights = getTaillightsNode();
    // ... modify material emissive property ...
}

void CarEntity::openDoor(Side side, bool open) {
    NodeHandle door = getDoorNode(side);
    if (!door.isValid() || !getScene()) {
        return;
    }

    const char* roleName    = (side == Side::Left) ? ROLE_DOOR_L : ROLE_DOOR_R;
    float       targetAngle = open ? config.doorOpenAngle : 0.0f;

    // Doors typically rotate around their hinge (Y-axis)
    // Left door opens outward (positive rotation), right door might be opposite
    float     angle    = (side == Side::Left) ? targetAngle : -targetAngle;
    glm::quat rotation = glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    animateRotation(roleName, rotation);
}

void CarEntity::openHood(bool open) {
    NodeHandle hood = getHoodNode();
    if (!hood.isValid() || !getScene()) {
        return;
    }

    float targetAngle = open ? config.hoodOpenAngle : 0.0f;

    // Hood typically rotates around its hinge at the front (X-axis)
    glm::quat rotation = glm::angleAxis(glm::radians(targetAngle), glm::vec3(1.0f, 0.0f, 0.0f));

    animateRotation(ROLE_HOOD, rotation);
}

// ============================================================================
// Base Rotation Management
// ============================================================================

void CarEntity::captureBaseRotations() {
    // List of roles that need base rotation preservation for animation
    const char* animatableRoles[] = {
        ROLE_STEERING_WHEEL_FRONT, ROLE_STEERING_WHEEL_BACK, ROLE_WHEEL_FL, ROLE_WHEEL_FR, ROLE_WHEEL_RL, ROLE_WHEEL_RR,
        ROLE_WIPER_LEFT,           ROLE_WIPER_RIGHT,         ROLE_DOOR_L,   ROLE_DOOR_R,   ROLE_HOOD};

    for (const char* role : animatableRoles) {
        NodeHandle handle = getNode(role);
        if (handle.isValid() && getScene()) {
            SceneNode* node = getScene()->getNode(handle);
            if (node) {
                baseRotations[role] = node->localRotation;
            }
        }
    }
}

}  // namespace DownPour
