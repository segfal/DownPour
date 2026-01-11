// SPDX-License-Identifier: MIT
#include "simulation/WeatherSystem.h"
#include <iostream>
#include <algorithm>
#include <random>

namespace DownPour {
namespace Simulation {

// Constants for raindrop physics
constexpr float RAINDROP_MIN_SIZE = 0.1f;
constexpr float RAINDROP_MAX_SIZE = 0.2f;
constexpr float SPAWN_RADIUS = 20.0f;
constexpr float SPAWN_HEIGHT = 30.0f;

// Random number generator
static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution<float> distPos(-SPAWN_RADIUS, SPAWN_RADIUS);
static std::uniform_real_distribution<float> distSize(RAINDROP_MIN_SIZE, RAINDROP_MAX_SIZE);

WeatherSystem::WeatherSystem() 
    : currentState(WeatherState::Sunny) {
    std::cout << "Weather System initialized - Current state: Sunny" << std::endl;
}

void WeatherSystem::toggleWeather() {
    if (currentState == WeatherState::Sunny) {
        currentState = WeatherState::Rainy;
        std::cout << "Weather changed to: RAINY" << std::endl;
    } else {
        currentState = WeatherState::Sunny;
        std::cout << "Weather changed to: SUNNY" << std::endl;
        raindrops.clear();
    }
}

void WeatherSystem::spawnRaindrop() {
    if (currentState != WeatherState::Rainy) return;
    if (raindrops.size() >= MAX_RAINDROPS) return;
    
    Raindrop drop;
    // Spawn in a volume around camera
    drop.position = glm::vec3(
        distPos(gen),       // -20 to +20m
        SPAWN_HEIGHT,       // Start 30m up
        distPos(gen)        // -20 to +20m
    );
    drop.velocity = glm::vec3(0.0f, -9.8f, 0.0f);  // Gravity
    drop.lifetime = 0.0f;
    drop.size = distSize(gen);
    drop.active = true;
    
    raindrops.push_back(drop);
}

void WeatherSystem::updateRaindrops(float deltaTime) {
    for (auto& drop : raindrops) {
        if (!drop.active) continue;
        
        // Physics
        drop.position += drop.velocity * deltaTime;
        drop.lifetime += deltaTime;
        
        // Deactivate if hit ground or lived too long
        if (drop.position.y < 0.0f || drop.lifetime > 10.0f) {
            drop.active = false;
        }
    }
}

void WeatherSystem::cleanupInactiveDrops() {
    raindrops.erase(
        std::remove_if(raindrops.begin(), raindrops.end(),
            [](const Raindrop& drop) { return !drop.active; }),
        raindrops.end()
    );
}

void WeatherSystem::update(float deltaTime) {
    if (currentState != WeatherState::Rainy) {
        raindrops.clear();
        return;
    }
    
    // Spawn new drops
    spawnTimer += deltaTime;
    while (spawnTimer > spawnRate) {
        spawnRaindrop();
        spawnTimer -= spawnRate;
    }
    
    updateRaindrops(deltaTime);
    cleanupInactiveDrops();
}

void WeatherSystem::render(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t frameIndex) {
    // TODO: Implement rain particle rendering
    // This will be implemented when we have the rain particle pipeline
    (void)cmd;
    (void)layout;
    (void)frameIndex;
}

} // namespace Simulation
} // namespace DownPour
