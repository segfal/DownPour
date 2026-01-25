// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"

namespace DownPour {

/**
 * @brief Represents a road entity in the scene
 *
 * Provides specialized management for road geometry and segments.
 */
class RoadEntity : public Entity {
public:
    using Entity::Entity;

    struct Config {
        float    width    = 10.0f;     // Road width in meters
        float    length   = 50000.0f;  // Total road length (~50km from model)
        uint32_t segments = 1000;      // Number of visual/logical segments
    };

    const Config& getConfig() const { return config; }
    Config&       getConfig() { return config; }

private:
    Config config;
};

}  // namespace DownPour
