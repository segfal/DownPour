// SPDX-License-Identifier: MIT
#include "CarEntity.h"

#include "../renderer/ModelAdapter.h"
#include "Scene.h"
#include "SceneNode.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cmath>
#include <cstdio>

namespace DownPour {

CarEntity::CarEntity(const str& name, Scene* scene, ModelAdapter* adapter)
    : Entity(name, scene), adapter(adapter) {
    if (!adapter) return;

    // Read physics config from adapter
    const auto& phys = adapter->getPhysicsConfig();
    if (phys.hasData) {
        maxSpeed = phys.maxAcceleration * 6.0f;
        accel = phys.maxAcceleration;
        brakeForce = phys.maxBraking;
        maxSteerAngle = phys.maxSteerAngle;
        wheelBase = phys.wheelBase;
        wheelRadius = phys.wheelRadius;
        dragCoeff = phys.dragCoefficient;
        rollingResist = phys.rollingResistance;
    }
}

CarEntity::~CarEntity() {
    delete cockpitCamera;
}

const char* CarEntity::getStateName() const {
    switch (state) {
        case CarState::Idle: return "Idle";
        case CarState::Driving: return "Driving";
        case CarState::Braking: return "Braking";
        case CarState::Turning: return "Turning";
    }
    return "Unknown";
}

void CarEntity::initCockpitCamera(Scene* scene) {
    if (!scene || !getRootNode().isValid()) return;

    // Find the RootNode inside the glTF hierarchy (2 levels deep)
    // Hierarchy: Sketchfab_model (S=1000) -> FINAL_MODEL_24.fbx (S=0.01) -> RootNode
    // Attaching to RootNode means cockpit position is in mesh-space coordinates
    NodeHandle attachParent = scene->findNode("RootNode");
    if (!attachParent.isValid()) {
        // Fallback to car entity root
        attachParent = getRootNode();
    }

    // Create a camera node as child of the RootNode (mesh-space coords)
    NodeHandle camNode = scene->createNode("cockpit_camera", attachParent);

    // Position in mesh space: driver's eye position
    // Mesh is ~4.7m long, Y-up, center roughly at origin
    // X: +0.35 = left of center (LHD driver seat)
    // Y: +1.2  = eye height
    // Z: +0.3  = slightly forward of center
    Vec3 cockpitPos(0.35f, 1.2f, 0.3f);
    if (adapter) {
        const auto& camCfg = adapter->getCameraConfig();
        if (camCfg.hasData) {
            cockpitPos = camCfg.cockpit.position;
        }
    }

    SceneNode* node = scene->getNode(camNode);
    if (node) {
        node->setLocalPosition(cockpitPos);
        scene->markSubtreeDirty(camNode);
    }

    // Create camera entity attached to this node
    cockpitCamera = new CameraEntity("cockpit_cam", scene, adapter);
    cockpitCamera->addNode(camNode, "camera_root");

    // Configure camera FOV/planes
    if (adapter) {
        const auto& camCfg = adapter->getCameraConfig();
        if (camCfg.hasData) {
            CameraEntity::CameraConfig cfg;
            cfg.fov = camCfg.cockpit.fov;
            cfg.nearPlane = camCfg.cockpit.nearPlane;
            cfg.farPlane = camCfg.cockpit.farPlane;
            cockpitCamera->setConfig(cfg);
        }
    }

    // Apply camera orientation from sidecar JSON euler [pitch, yaw, roll] in degrees.
    // This sets both the node rotation AND CameraEntity's internal yaw/pitch
    // so mouse look stays consistent (no jump on first mouse movement).
    // Edit "rotation":{"euler":[pitch, yaw, roll]} in bmw_suv.glb.json to tune.
    float camYaw = 180.0f;   // default: look along +Z
    float camPitch = 0.0f;
    if (adapter) {
        const auto& camCfg = adapter->getCameraConfig();
        if (camCfg.hasData && !camCfg.cockpit.useQuaternion) {
            // eulerRotation = [pitch, yaw, roll] in degrees
            camPitch = camCfg.cockpit.eulerRotation.x;
            camYaw = camCfg.cockpit.eulerRotation.y;
        }
    }
    cockpitCamera->setInitialOrientation(camYaw, camPitch);

    // Debug: print camera setup info
    printf("=== Cockpit Camera Debug ===\n");
    printf("  Position (mesh space): %.2f, %.2f, %.2f\n", cockpitPos.x, cockpitPos.y, cockpitPos.z);
    printf("  Yaw: %.1f deg, Pitch: %.1f deg\n", camYaw, camPitch);
    Vec3 fwd = cockpitCamera->getWorldForward();
    Vec3 pos = cockpitCamera->getWorldPosition();
    printf("  World position: %.1f, %.1f, %.1f\n", pos.x, pos.y, pos.z);
    printf("  World forward:  %.3f, %.3f, %.3f\n", fwd.x, fwd.y, fwd.z);
    printf("  Tip: Edit 'rotation.euler' [pitch,yaw,roll] in bmw_suv.glb.json to adjust\n");
    printf("============================\n");
}

void CarEntity::update(float deltaTime, GLFWwindow* window) {
    updateInput(deltaTime, window);
    updatePhysics(deltaTime);
    updateState(window);
    updateAnimations();
}

void CarEntity::updateInput(float deltaTime, GLFWwindow* window) {
    bool throttle = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool brake = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool steerLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool steerRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    if (throttle) {
        speed += accel * deltaTime;
        if (speed > maxSpeed) speed = maxSpeed;
    } else if (brake) {
        speed -= brakeForce * deltaTime;
        if (speed < 0.0f) speed = 0.0f;
    } else {
        // Rolling resistance + drag
        float decel = rollingResist * 9.8f + dragCoeff * speed * speed * 0.001f;
        speed -= decel * deltaTime;
        if (speed < 0.1f) speed = 0.0f;
    }

    // Steering
    if (steerLeft) {
        steeringAngle += steerSpeed * deltaTime;
        if (steeringAngle > maxSteerAngle) steeringAngle = maxSteerAngle;
    } else if (steerRight) {
        steeringAngle -= steerSpeed * deltaTime;
        if (steeringAngle < -maxSteerAngle) steeringAngle = -maxSteerAngle;
    } else {
        // Return steering to center
        if (steeringAngle > 0.0f) {
            steeringAngle -= steerReturn * deltaTime;
            if (steeringAngle < 0.0f) steeringAngle = 0.0f;
        } else if (steeringAngle < 0.0f) {
            steeringAngle += steerReturn * deltaTime;
            if (steeringAngle > 0.0f) steeringAngle = 0.0f;
        }
    }
}

void CarEntity::updatePhysics(float deltaTime) {
    Scene* scene = getScene();
    if (!scene || !getRootNode().isValid()) return;

    SceneNode* node = scene->getNode(getRootNode());
    if (!node) return;

    // Lazily capture the original root node rotation on first call.
    // The glTF hierarchy has: Sketchfab_model (rot=-90°X, scale=1000)
    //   -> FINAL_MODEL_24.fbx (rot=+90°X, scale=0.01) -> RootNode
    // The -90°X on Sketchfab_model cancels the +90°X on FINAL_MODEL_24.
    // We MUST preserve this rotation and combine it with heading.
    if (!hasOriginalRotation) {
        originalRootRotation = node->localRotation;
        hasOriginalRotation = true;
        printf("=== Car Root Rotation Captured ===\n");
        printf("  Quat(w,x,y,z): %.4f, %.4f, %.4f, %.4f\n",
               originalRootRotation.w, originalRootRotation.x,
               originalRootRotation.y, originalRootRotation.z);
        printf("  Node position: %.1f, %.1f, %.1f\n",
               node->localPosition.x, node->localPosition.y, node->localPosition.z);
        printf("  Node scale: %.4f, %.4f, %.4f\n",
               node->localScale.x, node->localScale.y, node->localScale.z);
        printf("==================================\n");
    }

    if (speed < 0.01f) return;

    // Bicycle model steering
    float steerRad = glm::radians(steeringAngle);
    if (std::abs(steerRad) > 0.001f) {
        float turnRadius = wheelBase / std::tan(steerRad);
        float angularVel = speed / turnRadius;
        heading += angularVel * deltaTime;
    }

    // Move along heading direction
    Vec3 position = node->localPosition;
    position.x += speed * std::sin(heading) * deltaTime;
    position.z += speed * std::cos(heading) * deltaTime;

    node->localPosition = position;

    // Combine heading rotation with original root rotation
    // heading rotates around Y, then apply original rotation (e.g. -90°X)
    Quat headingQuat = glm::angleAxis(heading, Vec3(0.0f, 1.0f, 0.0f));
    node->localRotation = headingQuat * originalRootRotation;

    scene->markSubtreeDirty(getRootNode());

    // Accumulate wheel spin
    wheelRotation += (speed / wheelRadius) * deltaTime;
}

void CarEntity::updateState(GLFWwindow* window) {
    bool braking = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && speed > 0.1f;
    bool turning = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) && speed > 0.1f;

    if (speed < 0.1f) {
        state = CarState::Idle;
    } else if (braking) {
        state = CarState::Braking;
    } else if (turning) {
        state = CarState::Turning;
    } else {
        state = CarState::Driving;
    }
}

void CarEntity::updateAnimations() {
    // Animate the parent wheel group nodes (X3:Ani_Wheel_Scale_*)
    // Rotating the parent rotates all children (tyre + rim) together
    Quat wheelSpin = glm::angleAxis(wheelRotation, Vec3(1.0f, 0.0f, 0.0f));

    // Front wheels: steer + spin
    float steerRad = glm::radians(steeringAngle);
    Quat steerQuat = glm::angleAxis(steerRad, Vec3(0.0f, 1.0f, 0.0f));
    Quat frontWheelRot = steerQuat * wheelSpin;

    animateRotation("wheel_FL", frontWheelRot);
    animateRotation("wheel_FR", frontWheelRot);

    // Rear wheels: spin only
    animateRotation("wheel_BL", wheelSpin);
    animateRotation("wheel_BR", wheelSpin);

    // Steering wheel visual rotation (X3:Ani_Steer_Wheel_FL_2)
    float steeringWheelAngle = steeringAngle * (450.0f / maxSteerAngle);
    Quat steeringRot = glm::angleAxis(glm::radians(steeringWheelAngle), Vec3(0.0f, 0.0f, 1.0f));
    animateRotation("steering_wheel", steeringRot);
}

}  // namespace DownPour
