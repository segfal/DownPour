// SPDX-License-Identifier: MIT
#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <vector>

namespace DownPour {
namespace Simulation {

/**
 * @brief Raindrop particle structure
 */
struct Raindrop {
    glm::vec3 position;  // Current position
    glm::vec3 velocity;  // Fall velocity
    float     lifetime;  // Time alive
    float     size;      // Droplet size
    bool      active;    // Is this drop alive?
};

/**
 * @brief Weather system managing rain simulation and atmospheric conditions
 *
 * This class provides the foundation for weather state management and rain effects.
 * Current implementation supports toggling between Sunny and Rainy states.
 * Future enhancements will include rain particle systems and windshield effects.
 */
class WeatherSystem {
public:
    /**
     * @brief Weather state enumeration
     */
    enum class WeatherState { Sunny, Rainy };

    WeatherSystem();
    ~WeatherSystem() = default;

    /**
     * @brief Toggle between weather states
     *
     * Switches between Sunny and Rainy states. This is typically called
     * when the user presses the 'R' key.
     */
    void toggleWeather();

    /**
     * @brief Get current weather state
     * @return Current weather state (Sunny or Rainy)
     */
    WeatherState getWeatherState() const { return currentState; }

    /**
     * @brief Set weather state directly
     * @param state The weather state to set
     */
    void setWeatherState(WeatherState state) { currentState = state; }

    /**
     * @brief Check if it's currently raining
     * @return true if weather state is Rainy, false otherwise
     */
    bool isRaining() const { return currentState == WeatherState::Rainy; }

    /**
     * @brief Update weather system state
     * @param deltaTime Time since last update in seconds
     */
    void update(float deltaTime);

    /**
     * @brief Render rain particles
     * @param cmd Command buffer for recording render commands
     * @param layout Pipeline layout
     * @param frameIndex Current frame index
     */
    void render(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t frameIndex);

    /**
     * @brief Get active raindrops
     * @return Vector of active raindrops
     */
    const std::vector<Raindrop>& getActiveDrops() const { return raindrops; }

private:
    WeatherState currentState;

    // Rain particle system
    std::vector<Raindrop>   raindrops;
    static constexpr size_t MAX_RAINDROPS = 5000;
    float                   spawnTimer    = 0.0f;
    float                   spawnRate     = 0.01f;  // Seconds between spawns

    /**
     * @brief Spawn a new raindrop
     */
    void spawnRaindrop();

    /**
     * @brief Update all raindrops
     * @param deltaTime Time since last update
     */
    void updateRaindrops(float deltaTime);

    /**
     * @brief Remove inactive raindrops
     */
    void cleanupInactiveDrops();
};

}  // namespace Simulation
}  // namespace DownPour
