// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"
#include "CameraEntity.h"

struct GLFWwindow;

namespace DownPour {

class ModelAdapter;

enum class CarState { Idle, Driving, Braking, Turning };

class CarEntity : public Entity {
public:
    CarEntity(const str& name, Scene* scene, ModelAdapter* adapter);
    ~CarEntity() override;

    // State
    CarState getState() const { return state; }
    const char* getStateName() const;

    // Update (call each frame with deltaTime and window for input)
    void update(float deltaTime, GLFWwindow* window);

    // Camera
    CameraEntity* getCockpitCamera() const { return cockpitCamera; }
    void initCockpitCamera(Scene* scene);

    // Model access
    ModelAdapter* getAdapter() const { return adapter; }

    // Heading (radians, 0 = +Z direction)
    float getHeading() const { return heading; }
    float getSpeed() const { return speed; }

private:
    ModelAdapter* adapter = nullptr;
    CameraEntity* cockpitCamera = nullptr;
    CarState state = CarState::Idle;

    // Physics
    float speed = 0.0f;
    float heading = 0.0f;          // radians, 0 = +Z
    float steeringAngle = 0.0f;    // current steering wheel angle (degrees)
    float wheelRotation = 0.0f;    // accumulated wheel spin (radians)

    // Original root node rotation (e.g. Sketchfab_model's -90°X)
    // Must be preserved and combined with heading, never overwritten
    Quat originalRootRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    bool hasOriginalRotation = false;

    // Config (read from adapter or defaults)
    float maxSpeed = 50.0f;
    float accel = 8.0f;
    float brakeForce = 15.0f;
    float maxSteerAngle = 35.0f;
    float steerSpeed = 90.0f;      // degrees/sec
    float steerReturn = 120.0f;    // degrees/sec return to center
    float wheelBase = 2.9f;
    float wheelRadius = 0.38f;
    float dragCoeff = 0.35f;
    float rollingResist = 0.015f;

    void updateInput(float deltaTime, GLFWwindow* window);
    void updatePhysics(float deltaTime);
    void updateState(GLFWwindow* window);
    void updateAnimations();
};

}  // namespace DownPour
