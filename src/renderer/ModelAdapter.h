// SPDX-License-Identifier: MIT
#pragma once

#include "../core/Types.h"
#include "Model.h"

#include <json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace DownPour {
using namespace Types;

/**
 * @brief Adapter for loading models with external configuration (JSON sidecar)
 *
 * This handles the loading of both GLB/GLTF geometry and its associated
 * metadata (roles, physical properties, target dimensions).
 */
class ModelAdapter {
public:
    ModelAdapter() = default;
    ~ModelAdapter();

    /**
     * @brief Load model and optional sidecar configuration
     * @param filepath Path to .gltf or .glb file
     */
    bool load(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
              VkQueue graphicsQueue);

    // Getters
    Model* getModel() const { return model; }

    // Role mapping (role name -> node name)
    std::string getNodeNameForRole(const std::string& role) const;
    bool        hasRole(const std::string& role) const;

    // Model properties
    float getTargetLength() const { return targetLength; }
    Vec3  getModelRotation() const { return modelRotation; }
    Vec3  getModelScale() const { return modelScale; }
    Vec3  getPositionOffset() const { return positionOffset; }

    // Camera configuration (expanded)
    struct CameraConfig {
        struct CockpitCamera {
            Vec3  position      = Vec3(0.0f);
            Quat  rotation      = Quat(1.0f, 0.0f, 0.0f, 0.0f);
            Vec3  eulerRotation = Vec3(0.0f);
            bool  useQuaternion = true;
            float fov           = 75.0f;
            float nearPlane     = 0.1f;
            float farPlane      = 10000.0f;
        };
        struct ChaseCamera {
            float distance  = 5.0f;
            float height    = 1.5f;
            float stiffness = 5.0f;
        };
        struct ThirdPersonCamera {
            float distance = 8.0f;
            float height   = 3.0f;
            float angle    = 0.0f;
        };
        CockpitCamera     cockpit;
        ChaseCamera       chase;
        ThirdPersonCamera thirdPerson;
        bool              hasData = false;
    };
    const CameraConfig& getCameraConfig() const { return cameraConfig; }

    // Windshield configuration
    struct WindshieldConfig {
        bool enabled = true;
        struct Material {
            float transparency    = 0.3f;
            float refractionIndex = 1.5f;
            Vec3  tint            = Vec3(0.9f, 0.95f, 1.0f);
        } material;
        struct Wipers {
            bool  enabled      = true;
            float speed        = 90.0f;
            Vec2  angleRange   = Vec2(-45.0f, 45.0f);
            float parkingAngle = -45.0f;
        } wipers;
        struct RainEffects {
            bool  enabled     = true;
            float dropletSize = 0.002f;
            float flowSpeed   = 0.5f;
        } rainEffects;
        bool hasData = false;
    };
    const WindshieldConfig& getWindshieldConfig() const { return windshieldConfig; }

    // Wheel configuration
    struct WheelConfig {
        float radius       = 0.35f;
        float width        = 0.22f;
        Vec3  rotationAxis = Vec3(1.0f, 0.0f, 0.0f);
        struct Individual {
            float steerMultiplier = 0.0f;
            float driveMultiplier = 1.0f;
            Vec3  position        = Vec3(0.0f);
        };
        std::unordered_map<std::string, Individual> wheels;  // "front_left", "front_right", etc.
        bool                                        hasData = false;
    };
    const WheelConfig& getWheelConfig() const { return wheelConfig; }

    // Steering wheel configuration
    struct SteeringWheelConfig {
        float maxRotation  = 450.0f;
        Vec3  rotationAxis = Vec3(0.0f, 0.0f, 1.0f);
        float sensitivity  = 1.0f;
        float returnSpeed  = 3.0f;
        Vec3  position     = Vec3(0.0f);
        bool  hasData      = false;
    };
    const SteeringWheelConfig& getSteeringWheelConfig() const { return steeringWheelConfig; }

    // Lights configuration
    struct LightsConfig {
        struct Light {
            float intensity = 1.0f;
            Vec3  color     = Vec3(1.0f);
            float range     = 50.0f;
            float spotAngle = 45.0f;
        };
        Light headlights;
        Light taillights;
        Light brakelights;
        bool  hasData = false;
    };
    const LightsConfig& getLightsConfig() const { return lightsConfig; }

    // Doors configuration
    struct DoorsConfig {
        float openAngle    = 45.0f;
        float openSpeed    = 2.0f;
        Vec3  rotationAxis = Vec3(0.0f, 0.0f, 1.0f);
        struct Individual {
            Vec3  hingePosition = Vec3(0.0f);
            float openDirection = 1.0f;
        };
        std::unordered_map<std::string, Individual> doors;
        bool                                        hasData = false;
    };
    const DoorsConfig& getDoorsConfig() const { return doorsConfig; }

    // Animation configuration
    struct AnimationConfig {
        struct IdleAnimations {
            struct EngineVibration {
                bool  enabled   = false;
                float frequency = 30.0f;
                float amplitude = 0.001f;
            } engineVibration;
        } idleAnimations;
        struct TurnSignals {
            float                    blinkFrequency = 1.5f;
            std::vector<std::string> leftNodes;
            std::vector<std::string> rightNodes;
        } turnSignals;
        bool hasData = false;
    };
    const AnimationConfig& getAnimationConfig() const { return animationConfig; }

    // Physical configuration
    struct PhysicsConfig {
        float wheelBase         = 0.0f;
        float trackWidth        = 0.0f;
        float wheelRadius       = 0.0f;
        float maxSteerAngle     = 0.0f;
        float maxAcceleration   = 0.0f;
        float maxBraking        = 0.0f;
        float mass              = 1500.0f;
        float dragCoefficient   = 0.3f;
        float rollingResistance = 0.015f;
        bool  hasData           = false;
    };
    const PhysicsConfig& getPhysicsConfig() const { return physics; }

    // Initial State configuration
    struct SpawnConfig {
        Vec3 position = Vec3(0.0f);
        Vec3 rotation = Vec3(0.0f);
        bool hasData  = false;
    };
    const SpawnConfig& getSpawnConfig() const { return spawnConfig; }

    // Debug configuration
    struct DebugConfig {
        bool showColliders      = false;
        bool showSkeleton       = false;
        bool showCameraTarget   = true;
        bool showVelocityVector = false;
        bool hasData            = false;
    };
    const DebugConfig& getDebugConfig() const { return debugConfig; }

private:
    Model* model = nullptr;

    // Metadata from JSON
    float targetLength   = 0.0f;
    Vec3  modelRotation  = Vec3(0.0f);
    Vec3  modelScale     = Vec3(1.0f);
    Vec3  positionOffset = Vec3(0.0f);

    CameraConfig        cameraConfig;
    WindshieldConfig    windshieldConfig;
    WheelConfig         wheelConfig;
    SteeringWheelConfig steeringWheelConfig;
    DoorsConfig         doorsConfig;
    LightsConfig        lightsConfig;
    AnimationConfig     animationConfig;
    SpawnConfig         spawnConfig;
    DebugConfig         debugConfig;

    std::unordered_map<std::string, std::string> roleMap;
    PhysicsConfig                                physics;

    bool loadMetadata(const std::string& filepath);
};

}  // namespace DownPour
