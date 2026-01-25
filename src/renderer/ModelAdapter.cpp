// SPDX-License-Identifier: MIT
#include "ModelAdapter.h"

#include "logger/Logger.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace DownPour {

ModelAdapter::~ModelAdapter() {
    // Note: Model pointer might be managed by DownPourApplication or ModelAdapter
    // For now, let's assume ModelAdapter owns it if we created it here.
    if (model) {
        // cleanup should be called by the owner before destruction
        delete model;
    }
}

bool ModelAdapter::load(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool, VkQueue graphicsQueue) {
    Log logger;
    logger.log("info", "Loading model via adapter: " + filepath);

    model = new Model();
    try {
        model->loadFromFile(filepath, device, physicalDevice, commandPool, graphicsQueue);
    } catch (const std::exception& e) {
        logger.log("error", "Failed to load model geometry: " + std::string(e.what()));
        return false;
    }

    // Attempt to load metadata
    if (!loadMetadata(filepath)) {
        logger.log("warning", "No metadata sidecar found for " + filepath + ", using defaults.");

        // Default target length from hierarchy if missing
        glm::vec3 minB, maxB;
        model->getHierarchyBounds(minB, maxB);
        targetLength = (maxB.z - minB.z);
    }

    return true;
}

bool ModelAdapter::loadMetadata(const std::string& filepath) {
    std::string jsonPath = filepath + ".json";
    if (!std::filesystem::exists(jsonPath)) {
        return false;
    }

    Log logger;
    logger.log("info", "Loading metadata: " + jsonPath);

    try {
        std::ifstream  f(jsonPath);
        nlohmann::json data = nlohmann::json::parse(f);

        // Load model properties
        if (data.contains("model")) {
            auto m = data["model"];
            if (m.contains("targetLength"))
                targetLength = m["targetLength"];
            if (m.contains("cockpitOffset") && m["cockpitOffset"].is_array() && m["cockpitOffset"].size() == 3) {
                cockpitOffset = glm::vec3(m["cockpitOffset"][0], m["cockpitOffset"][1], m["cockpitOffset"][2]);
            }
            if (m.contains("orientation") && m["orientation"].is_array() && m["orientation"].size() == 3) {
                modelRotation = glm::vec3(m["orientation"][0], m["orientation"][1], m["orientation"][2]);
            }
        }

        // Load role mapping
        if (data.contains("roles")) {
            for (auto& [role, nodeName] : data["roles"].items()) {
                roleMap[role] = nodeName;
            }
        }

        // Load physics config
        if (data.contains("config")) {
            auto c = data["config"];
            if (c.contains("wheelBase"))
                physics.wheelBase = c["wheelBase"];
            if (c.contains("trackWidth"))
                physics.trackWidth = c["trackWidth"];
            if (c.contains("wheelRadius"))
                physics.wheelRadius = c["wheelRadius"];
            if (c.contains("maxSteerAngle"))
                physics.maxSteerAngle = c["maxSteerAngle"];
            if (c.contains("maxAcceleration"))
                physics.maxAcceleration = c["maxAcceleration"];
            if (c.contains("maxBraking"))
                physics.maxBraking = c["maxBraking"];
        }

        return true;
    } catch (const std::exception& e) {
        logger.log("error", "Error parsing metadata " + jsonPath + ": " + e.what());
        return false;
    }
}

std::string ModelAdapter::getNodeNameForRole(const std::string& role) const {
    auto it = roleMap.find(role);
    if (it != roleMap.end()) {
        return it->second;
    }
    return "";
}

bool ModelAdapter::hasRole(const std::string& role) const {
    return roleMap.find(role) != roleMap.end();
}

}  // namespace DownPour
