// SPDX-License-Identifier: MIT
#pragma once

#include "../core/Types.h"
#include "Model.h"

#include <json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace DownPour {

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
    Vec3  getCockpitOffset() const { return cockpitOffset; }
    Vec3  getModelRotation() const { return modelRotation; }

    // Physical configuration
    struct PhysicsConfig {
        float wheelBase       = 0.0f;
        float trackWidth      = 0.0f;
        float wheelRadius     = 0.0f;
        float maxSteerAngle   = 0.0f;
        float maxAcceleration = 0.0f;
        float maxBraking      = 0.0f;
    };
    const PhysicsConfig& getPhysicsConfig() const { return physics; }

private:
    Model* model = nullptr;

    // Metadata from JSON
    float targetLength  = 0.0f;
    Vec3  cockpitOffset = Vec3(0.0f);
    Vec3  modelRotation = Vec3(0.0f);

    std::unordered_map<std::string, std::string> roleMap;
    PhysicsConfig                                physics;

    bool loadMetadata(const std::string& filepath);
};

}  // namespace DownPour
