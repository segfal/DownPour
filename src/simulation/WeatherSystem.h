// SPDX-License-Identifier: MIT
#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace DownPour {
namespace Simulation {

struct Raindrop {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    float size;
    bool active;
};

struct RainParams {
    uint32_t maxDrops;
    float spawnRate;        // drops per second
    float dropSpeed;        // base downward speed (m/s, positive)
    glm::vec3 wind;         // wind velocity (m/s)
    float fogDensity;       // Beer-Lambert fog coefficient
    float targetWetness;    // surface wetness at this level [0,1]
    float rainIntensity;    // shader intensity [0,1]
    float skyDarkening;     // sky overcast factor [0,1]
};

class WeatherSystem {
public:
    enum class WeatherState { Sunny, LowRain, HeavyRain, SevereRain };

    static constexpr uint32_t MAX_DROPS = 25000;

    WeatherSystem();
    ~WeatherSystem() = default;

    void cycleWeather();
    void update(float deltaTime, const glm::vec3& cameraPosition);

    WeatherState getState() const { return currentState; }
    float getRainIntensity() const;
    float getWetness() const { return currentWetness; }
    float getFogDensity() const;
    glm::vec3 getWind() const;
    float getSkyDarkening() const;
    uint32_t getActiveDropCount() const { return activeDropCount; }
    const std::vector<Raindrop>& getDrops() const { return drops; }
    const char* getStateName() const;

private:
    WeatherState currentState = WeatherState::Sunny;
    std::vector<Raindrop> drops;
    uint32_t activeDropCount = 0;
    float currentWetness = 0.0f;
    float spawnAccumulator = 0.0f;

    static const RainParams PARAMS[];

    const RainParams& getCurrentParams() const;
    void recycleDrop(Raindrop& drop, const glm::vec3& cameraPosition);
};

}  // namespace Simulation
}  // namespace DownPour
