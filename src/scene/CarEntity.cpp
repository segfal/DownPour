// SPDX-License-Identifier: MIT
#include "CarEntity.h"
#include "SceneNode.h"

#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace DownPour {

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
    currentSteeringAngle = glm::clamp(degrees, -config.maxSteerAngle, config.maxSteerAngle);

    NodeHandle steeringWheelFront = getSteeringWheelFrontNode();
    NodeHandle steeringWheelBack = getSteeringWheelBackNode();
    if (!steeringWheelFront.isValid() || !steeringWheelBack.isValid() || !getScene()) {
        return;
    }   

    // Rotate steering wheel around its local Z-axis
    glm::quat rotation = glm::angleAxis(glm::radians(currentSteeringAngle), glm::vec3(0.0f, 0.0f, 1.0f));
    animateRotation(ROLE_STEERING_WHEEL_FRONT, rotation);
    animateRotation(ROLE_STEERING_WHEEL_BACK, rotation);
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

}  // namespace DownPour
