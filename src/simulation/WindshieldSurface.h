// SPDX-License-Identifier: MIT
#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace DownPour {
namespace Simulation {

struct Raindrop;


class WindshieldSurface {
public:
    WindshieldSurface() = default;
    ~WindshieldSurface() = default;

    void initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool, VkQueue queue) {
        (void)device; (void)physicalDevice; (void)commandPool; (void)queue;
    }

    void cleanup(VkDevice device) { (void)device; }

    void update(float deltaTime, const std::vector<Raindrop>& raindrops) {
        (void)deltaTime; (void)raindrops;
    }

    void setWiperActive(bool active) { (void)active; }
    bool isWiperActive() const { return false; }
    float getWiperAngle() const { return 0.0f; }
    VkImageView getWetnessMapView() const { return VK_NULL_HANDLE; }
    VkImageView getFlowMapView() const { return VK_NULL_HANDLE; }
};

}  // namespace Simulation
}  // namespace DownPour
