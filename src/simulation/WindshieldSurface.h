// SPDX-License-Identifier: MIT
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "WeatherSystem.h"

namespace DownPour {
namespace Simulation {

/**
 * @brief Windshield surface managing water droplets and wiper effects
 * 
 * This class simulates water accumulation on a car windshield and
 * handles wiper clearing animations. It manages wetness and flow maps
 * used by the windshield shader for realistic water effects.
 */
class WindshieldSurface {
public:
    WindshieldSurface() = default;
    ~WindshieldSurface() = default;
    
    /**
     * @brief Initialize windshield resources
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for operations
     * @param graphicsQueue Graphics queue
     */
    void initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool, VkQueue graphicsQueue);
    
    /**
     * @brief Clean up Vulkan resources
     * @param device Vulkan logical device
     */
    void cleanup(VkDevice device);
    
    /**
     * @brief Update windshield state
     * @param deltaTime Time since last update
     * @param raindrops Active rain particles
     */
    void update(float deltaTime, const std::vector<Raindrop>& raindrops);
    
    /**
     * @brief Set wiper active state
     * @param active Whether wipers should be running
     */
    void setWiperActive(bool active) { wiperActive = active; }
    
    /**
     * @brief Get current wiper angle
     * @return Wiper angle in degrees (-45 to +45)
     */
    float getWiperAngle() const { return wiperAngle; }
    
    /**
     * @brief Get wetness map image view
     * @return Vulkan image view for wetness map
     */
    VkImageView getWetnessMapView() const { return wetnessMapView; }
    
    /**
     * @brief Get flow map image view
     * @return Vulkan image view for flow map
     */
    VkImageView getFlowMapView() const { return flowMapView; }

private:
    // Wiper state
    bool wiperActive = false;
    float wiperAngle = 0.0f;      // -45 to +45 degrees
    float wiperSpeed = 90.0f;     // Degrees per second
    bool wiperDirection = true;   // true = right, false = left
    
    // Texture maps for shader
    VkImage wetnessMap = VK_NULL_HANDLE;
    VkDeviceMemory wetnessMapMemory = VK_NULL_HANDLE;
    VkImageView wetnessMapView = VK_NULL_HANDLE;
    
    VkImage flowMap = VK_NULL_HANDLE;
    VkDeviceMemory flowMapMemory = VK_NULL_HANDLE;
    VkImageView flowMapView = VK_NULL_HANDLE;
    
    /**
     * @brief Update wiper animation
     * @param deltaTime Time since last update
     */
    void updateWiper(float deltaTime);
    
    /**
     * @brief Update wetness based on rain
     * @param raindrops Active rain particles
     */
    void updateWetness(const std::vector<Raindrop>& raindrops);
    
    /**
     * @brief Create wetness map texture
     */
    void createWetnessMap(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue);
    
    /**
     * @brief Create flow map texture
     */
    void createFlowMap(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue);
    
    /**
     * @brief Helper to find memory type
     */
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                           VkMemoryPropertyFlags properties);
};

} // namespace Simulation
} // namespace DownPour
