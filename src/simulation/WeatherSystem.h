// SPDX-License-Identifier: MIT
#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace DownPour {
namespace Simulation {

// Stubbed WeatherSystem - minimal implementation for refactoring
struct Raindrop {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    float size;
    bool active;
};

class WeatherSystem {
public:
    enum class WeatherState { Sunny, Rainy };

    WeatherSystem() = default;
    ~WeatherSystem() = default;

    void toggleWeather() {}
    WeatherState getState() const { return WeatherState::Sunny; }
    void update(float deltaTime) { (void)deltaTime; }
    const std::vector<Raindrop>& getActiveDrops() const { return drops; }

private:
    WeatherState currentState = WeatherState::Sunny;
    std::vector<Raindrop> drops;
};

}  // namespace Simulation
}  // namespace DownPour
