// SPDX-License-Identifier: MIT
#pragma once

namespace DownPour {
namespace Simulation {

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
    enum class WeatherState {
        Sunny,
        Rainy
    };

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
     * 
     * TODO: Will be expanded to update particle systems, wind effects, etc.
     */
    void update(float deltaTime);

private:
    WeatherState currentState;
    
    // TODO: Add rain particle system
    // TODO: Add windshield droplet management
    // TODO: Add wind simulation
    // TODO: Add weather transition effects
};

} // namespace Simulation
} // namespace DownPour
