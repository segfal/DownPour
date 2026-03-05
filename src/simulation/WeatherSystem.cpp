// SPDX-License-Identifier: MIT
#include "WeatherSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace DownPour {
namespace Simulation {

// Per-state rain parameters (scaled for 1000x environment)
//                          maxDrops  spawnRate  speed      wind                           fogDensity    wetness  intensity  darkening
const RainParams WeatherSystem::PARAMS[] = {
    /* Sunny  */  {     0,      0.0f,     0.0f, {0.0f, 0.0f, 0.0f},        0.0000002f,   0.0f,    0.0f,      0.0f  },
    /* Low    */  {  2000,    400.0f,  8000.0f, {1000.0f, 0.0f, 500.0f},    0.00000035f,  0.3f,    0.33f,     0.15f },
    /* Heavy  */  { 10000,   2000.0f, 10000.0f, {3000.0f, 0.0f, 1000.0f},   0.0000006f,   0.7f,    0.66f,     0.40f },
    /* Severe */  { 25000,   5000.0f, 12000.0f, {6000.0f, 0.0f, 3000.0f},   0.0000012f,   1.0f,    1.00f,     0.70f },
};

WeatherSystem::WeatherSystem() {
    drops.resize(MAX_DROPS);
    for (auto& drop : drops) {
        drop.active = false;
    }
}

void WeatherSystem::cycleWeather() {
    int next = (static_cast<int>(currentState) + 1) % 4;
    currentState = static_cast<WeatherState>(next);
    spawnAccumulator = 0.0f;
}

const RainParams& WeatherSystem::getCurrentParams() const {
    return PARAMS[static_cast<int>(currentState)];
}

float WeatherSystem::getRainIntensity() const { return getCurrentParams().rainIntensity; }
float WeatherSystem::getFogDensity() const { return getCurrentParams().fogDensity; }
glm::vec3 WeatherSystem::getWind() const { return getCurrentParams().wind; }
float WeatherSystem::getSkyDarkening() const { return getCurrentParams().skyDarkening; }

const char* WeatherSystem::getStateName() const {
    static const char* names[] = {"Sunny", "Low Rain", "Heavy Rain", "Severe Rain"};
    return names[static_cast<int>(currentState)];
}

// Random float in [0, 1)
static float randf() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

// Random float in [-1, 1)
static float randfs() {
    return randf() * 2.0f - 1.0f;
}

void WeatherSystem::recycleDrop(Raindrop& drop, const glm::vec3& cameraPosition) {
    const auto& params = getCurrentParams();

    // Spawn in a concentrated box around camera (1000x scale)
    // Tighter radius = denser rain appearance with same drop count
    constexpr float SPAWN_RADIUS = 30000.0f;  // 30m * 1000 (concentrated around camera)
    constexpr float SPAWN_HEIGHT = 20000.0f;   // 20m * 1000 above camera

    drop.position.x = cameraPosition.x + randfs() * SPAWN_RADIUS;
    drop.position.y = cameraPosition.y + 3000.0f + randf() * SPAWN_HEIGHT;
    drop.position.z = cameraPosition.z + randfs() * SPAWN_RADIUS;

    // Velocity: downward + wind + slight random variation
    drop.velocity.x = params.wind.x + randfs() * 500.0f;
    drop.velocity.y = -params.dropSpeed - randf() * 2000.0f;
    drop.velocity.z = params.wind.z + randfs() * 500.0f;

    // Realistic rain streak size — thin streaks, not blobs
    // At 1000x scale: 1-3 units wide, stretched by velocity in shader
    drop.size = 1.0f + randf() * 2.0f;
    drop.lifetime = 0.0f;
    drop.active = true;
}

void WeatherSystem::update(float deltaTime, const glm::vec3& cameraPosition) {
    const auto& params = getCurrentParams();

    // Ramp wetness toward target (3s ramp-up, 5s decay)
    float target = params.targetWetness;
    if (currentWetness < target) {
        currentWetness = std::min(currentWetness + deltaTime / 3.0f, target);
    } else if (currentWetness > target) {
        currentWetness = std::max(currentWetness - deltaTime / 5.0f, target);
    }

    constexpr float SPAWN_RADIUS = 30000.0f; // match recycleDrop
    activeDropCount = 0;

    // Update existing drops
    for (auto& drop : drops) {
        if (!drop.active) continue;

        drop.position += drop.velocity * deltaTime;
        drop.lifetime += deltaTime;

        bool hitGround = drop.position.y < -1000.0f;
        bool outOfRange = std::abs(drop.position.x - cameraPosition.x) > SPAWN_RADIUS * 1.5f ||
                          std::abs(drop.position.z - cameraPosition.z) > SPAWN_RADIUS * 1.5f;
        bool tooOld = drop.lifetime > 15.0f;

        if (hitGround || outOfRange || tooOld) {
            if (params.maxDrops > 0 && activeDropCount < params.maxDrops) {
                recycleDrop(drop, cameraPosition);
            } else {
                drop.active = false;
                continue;
            }
        }

        activeDropCount++;
    }

    // Spawn new drops to fill pool
    if (params.spawnRate > 0.0f && activeDropCount < params.maxDrops) {
        spawnAccumulator += params.spawnRate * deltaTime;
        int toSpawn = static_cast<int>(spawnAccumulator);
        spawnAccumulator -= static_cast<float>(toSpawn);

        int maxNew = static_cast<int>(params.maxDrops) - static_cast<int>(activeDropCount);
        toSpawn = std::min(toSpawn, maxNew);

        for (int i = 0; i < toSpawn; i++) {
            // Find first inactive slot
            for (auto& drop : drops) {
                if (!drop.active) {
                    recycleDrop(drop, cameraPosition);
                    activeDropCount++;
                    break;
                }
            }
        }
    }
}

}  // namespace Simulation
}  // namespace DownPour
