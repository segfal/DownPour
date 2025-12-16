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
        return false;
    }

    // Attempt to load metadata
    if (!loadMetadata(filepath)) {
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
    logger.log("info", "Loading rich metadata: " + jsonPath);

    try {
        std::ifstream  f(jsonPath);
        nlohmann::json data = nlohmann::json::parse(f);

        // --- 1. Model Properties ---
        if (data.contains("model")) {
            auto m = data["model"];
            if (m.contains("targetLength"))
                targetLength = m["targetLength"];

            if (m.contains("orientation")) {
                auto o = m["orientation"];
                if (o.contains("euler") && o["euler"].is_array() && o["euler"].size() == 3) {
                    float x = o["euler"][0];
                    float y = o["euler"][1];
                    float z = o["euler"][2];
                    if (o.value("unit", "degrees") == "degrees") {
                        x = glm::radians(x);
                        y = glm::radians(y);
                        z = glm::radians(z);
                    }
                    modelRotation = Vec3(x, y, z);
                }
            }

            if (m.contains("scale")) {
                auto s = m["scale"];
                if (s.contains("xyz") && s["xyz"].is_array() && s["xyz"].size() == 3) {
                    modelScale = Vec3(s["xyz"][0], s["xyz"][1], s["xyz"][2]);
                } else if (s.contains("uniform")) {
                    modelScale = Vec3(s["uniform"].get<float>());
                }
            }

            if (m.contains("positionOffset") && m["positionOffset"].is_array() && m["positionOffset"].size() == 3) {
                positionOffset = Vec3(m["positionOffset"][0], m["positionOffset"][1], m["positionOffset"][2]);
            }
        }

        // --- 2. Camera Configuration ---
        if (data.contains("camera")) {
            cameraConfig.hasData = true;
            auto c               = data["camera"];
            auto parseCam        = [](const nlohmann::json& j, CameraConfig::CockpitCamera& out) {
                if (j.contains("position") && j["position"].contains("xyz") && j["position"]["xyz"].size() == 3) {
                    out.position = Vec3(j["position"]["xyz"][0], j["position"]["xyz"][1], j["position"]["xyz"][2]);
                }
                if (j.contains("rotation")) {
                    auto r = j["rotation"];
                    if (r.value("preferQuaternion", false) && r.contains("quaternion") && r["quaternion"].size() == 4) {
                        out.rotation =
                            Quat(r["quaternion"][3], r["quaternion"][0], r["quaternion"][1], r["quaternion"][2]);
                        out.useQuaternion = true;
                    } else if (r.contains("euler") && r["euler"].size() == 3) {
                        out.eulerRotation = Vec3(r["euler"][0], r["euler"][1], r["euler"][2]);
                        out.useQuaternion = false;
                    }
                }
                if (j.contains("fov"))
                    out.fov = j["fov"];
                if (j.contains("nearPlane"))
                    out.nearPlane = j["nearPlane"];
                if (j.contains("farPlane"))
                    out.farPlane = j["farPlane"];
            };

            if (c.contains("cockpit"))
                parseCam(c["cockpit"], cameraConfig.cockpit);

            if (c.contains("chase")) {
                cameraConfig.chase.distance  = c["chase"].value("distance", 5.0f);
                cameraConfig.chase.height    = c["chase"].value("height", 1.5f);
                cameraConfig.chase.stiffness = c["chase"].value("stiffness", 5.0f);
            }

            if (c.contains("thirdPerson")) {
                cameraConfig.thirdPerson.distance = c["thirdPerson"].value("distance", 8.0f);
                cameraConfig.thirdPerson.height   = c["thirdPerson"].value("height", 3.0f);
                cameraConfig.thirdPerson.angle    = c["thirdPerson"].value("angle", 0.0f);
            }
            cameraConfig.hasData = true;
        }

        // --- 3. Parts Configuration ---
        if (data.contains("parts")) {
            auto p = data["parts"];

            // Windshield
            if (p.contains("windshield")) {
                auto w                   = p["windshield"];
                windshieldConfig.enabled = w.value("enabled", true);
                if (w.contains("material")) {
                    windshieldConfig.material.transparency    = w["material"].value("transparency", 0.3f);
                    windshieldConfig.material.refractionIndex = w["material"].value("refractionIndex", 1.5f);
                }
                if (w.contains("wipers")) {
                    windshieldConfig.wipers.enabled = w["wipers"].value("enabled", true);
                    windshieldConfig.wipers.speed   = w["wipers"].value("speed", 90.0f);
                }
                windshieldConfig.hasData = true;
            }

            // Wheels
            if (p.contains("wheels")) {
                auto wh            = p["wheels"];
                wheelConfig.radius = wh.value("radius", 0.35f);
                wheelConfig.width  = wh.value("width", 0.22f);
                if (wh.contains("rotationAxis") && wh["rotationAxis"].size() == 3) {
                    wheelConfig.rotationAxis =
                        Vec3(wh["rotationAxis"][0], wh["rotationAxis"][1], wh["rotationAxis"][2]);
                }
                if (wh.contains("individual")) {
                    for (auto& [wheelName, wheelData] : wh["individual"].items()) {
                        WheelConfig::Individual ind;
                        ind.steerMultiplier = wheelData.value("steerMultiplier", 1.0f);
                        ind.driveMultiplier = wheelData.value("driveMultiplier", 1.0f);
                        if (wheelData.contains("position") && wheelData["position"].size() == 3) {
                            ind.position =
                                Vec3(wheelData["position"][0], wheelData["position"][1], wheelData["position"][2]);
                        }
                        wheelConfig.wheels[wheelName] = ind;
                    }
                }
                wheelConfig.hasData = true;
            }

            // Steering Wheel
            if (p.contains("steeringWheel")) {
                auto sw                         = p["steeringWheel"];
                steeringWheelConfig.maxRotation = sw.value("maxRotation", 450.0f);
                if (sw.contains("rotationAxis") && sw["rotationAxis"].size() == 3) {
                    steeringWheelConfig.rotationAxis =
                        Vec3(sw["rotationAxis"][0], sw["rotationAxis"][1], sw["rotationAxis"][2]);
                }
                if (sw.contains("position") && sw["position"].size() == 3) {
                    steeringWheelConfig.position = Vec3(sw["position"][0], sw["position"][1], sw["position"][2]);
                }
                steeringWheelConfig.hasData = true;
            }

            // Doors
            if (p.contains("doors")) {
                auto d                = p["doors"];
                doorsConfig.openAngle = d.value("openAngle", 45.0f);
                if (d.contains("individual")) {
                    for (auto& [doorName, doorData] : d["individual"].items()) {
                        DoorsConfig::Individual ind;
                        ind.openDirection = doorData.value("openDirection", 1.0f);
                        if (doorData.contains("hingePosition") && doorData["hingePosition"].size() == 3) {
                            ind.hingePosition = Vec3(doorData["hingePosition"][0], doorData["hingePosition"][1],
                                                     doorData["hingePosition"][2]);
                        }
                        doorsConfig.doors[doorName] = ind;
                    }
                }
                doorsConfig.hasData = true;
            }

            // Lights
            if (p.contains("lights")) {
                auto l          = p["lights"];
                auto parseLight = [](const nlohmann::json& j, LightsConfig::Light& out) {
                    out.intensity = j.value("intensity", 1.0f);
                    if (j.contains("color") && j["color"].size() == 3) {
                        out.color = Vec3(j["color"][0], j["color"][1], j["color"][2]);
                    }
                    out.range = j.value("range", 50.0f);
                };
                if (l.contains("headlights"))
                    parseLight(l["headlights"], lightsConfig.headlights);
                if (l.contains("taillights"))
                    parseLight(l["taillights"], lightsConfig.taillights);
                if (l.contains("brakelights"))
                    parseLight(l["brakelights"], lightsConfig.brakelights);
                lightsConfig.hasData = true;
            }
        }

        // --- 4. Role Mapping ---
        if (data.contains("roles")) {
            for (auto& [role, nodeName] : data["roles"].items()) {
                roleMap[role] = nodeName.get<std::string>();
            }
        }

        // --- 5. Physics Configuration ---
        if (data.contains("physics")) {
            auto ph                   = data["physics"];
            physics.wheelBase         = ph.value("wheelBase", physics.wheelBase);
            physics.trackWidth        = ph.value("trackWidth", physics.trackWidth);
            physics.wheelRadius       = ph.value("wheelRadius", physics.wheelRadius);
            physics.maxSteerAngle     = ph.value("maxSteerAngle", physics.maxSteerAngle);
            physics.maxAcceleration   = ph.value("maxAcceleration", physics.maxAcceleration);
            physics.maxBraking        = ph.value("maxBraking", physics.maxBraking);
            physics.mass              = ph.value("mass", 1500.0f);
            physics.dragCoefficient   = ph.value("dragCoefficient", 0.3f);
            physics.rollingResistance = ph.value("rollingResistance", 0.015f);
            physics.hasData           = true;
        }

        // --- 6. Animation Configuration ---
        if (data.contains("animation")) {
            auto a = data["animation"];
            if (a.contains("idleAnimations") && a["idleAnimations"].contains("engineVibration")) {
                auto ev                                                  = a["idleAnimations"]["engineVibration"];
                animationConfig.idleAnimations.engineVibration.enabled   = ev.value("enabled", false);
                animationConfig.idleAnimations.engineVibration.frequency = ev.value("frequency", 30.0f);
                animationConfig.idleAnimations.engineVibration.amplitude = ev.value("amplitude", 0.001f);
            }
            if (a.contains("turnSignals")) {
                auto ts                                    = a["turnSignals"];
                animationConfig.turnSignals.blinkFrequency = ts.value("blinkFrequency", 1.5f);
                if (ts.contains("leftNodes"))
                    animationConfig.turnSignals.leftNodes = ts["leftNodes"].get<std::vector<std::string>>();
                if (ts.contains("rightNodes"))
                    animationConfig.turnSignals.rightNodes = ts["rightNodes"].get<std::vector<std::string>>();
            }
            animationConfig.hasData = true;
        }

        // --- 7. Initial State Configuration ---
        if (data.contains("initialState")) {
            auto is = data["initialState"];
            if (is.contains("position") && is["position"].size() == 3) {
                spawnConfig.position = Vec3(is["position"][0], is["position"][1], is["position"][2]);
            }
            if (is.contains("rotation") && is["rotation"].size() == 3) {
                spawnConfig.rotation = Vec3(is["rotation"][0], is["rotation"][1], is["rotation"][2]);
            }
            spawnConfig.hasData = true;
        }

        // --- 8. Debug Configuration ---
        if (data.contains("debug")) {
            auto d                         = data["debug"];
            debugConfig.showColliders      = d.value("showColliders", false);
            debugConfig.showSkeleton       = d.value("showSkeleton", false);
            debugConfig.showCameraTarget   = d.value("showCameraTarget", true);
            debugConfig.showVelocityVector = d.value("showVelocityVector", false);
            debugConfig.hasData            = true;
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
