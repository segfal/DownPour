// SPDX-License-Identifier: MIT
#pragma once

#include "../simulation/WeatherSystem.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <vector>

namespace DownPour {

struct RainQuadVertex {
    glm::vec2 corner;
};

struct RainInstance {
    glm::vec3 position;
    float alpha;
    glm::vec3 velocity;
    float size;
};

class RainRenderer {
public:
    static constexpr uint32_t MAX_INSTANCES = Simulation::WeatherSystem::MAX_DROPS;

    void init(VkDevice device, VkPhysicalDevice physicalDevice,
              VkCommandPool commandPool, VkQueue graphicsQueue,
              VkRenderPass renderPass, VkExtent2D extent,
              VkDescriptorSetLayout cameraDescriptorSetLayout,
              uint32_t framesInFlight);

    void updateInstances(const std::vector<Simulation::Raindrop>& drops, uint32_t frameIndex);

    void draw(VkCommandBuffer cmdBuffer, uint32_t frameIndex,
              VkDescriptorSet cameraDescriptorSet);

    void cleanup(VkDevice device);

    uint32_t getActiveCount() const { return activeInstanceCount; }

private:
    VkDevice device = VK_NULL_HANDLE;

    // Quad geometry (static)
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadVertexMemory = VK_NULL_HANDLE;
    VkBuffer quadIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadIndexMemory = VK_NULL_HANDLE;

    // Instance buffers (dynamic, one per frame-in-flight)
    std::vector<VkBuffer> instanceBuffers;
    std::vector<VkDeviceMemory> instanceMemories;
    std::vector<void*> instanceMapped;

    // Pipeline
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    uint32_t activeInstanceCount = 0;

    void createQuadGeometry(VkDevice device, VkPhysicalDevice physicalDevice,
                            VkCommandPool commandPool, VkQueue graphicsQueue);
    void createInstanceBuffers(VkDevice device, VkPhysicalDevice physicalDevice,
                               uint32_t framesInFlight);
    void createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
                        VkDescriptorSetLayout cameraDescriptorSetLayout);
};

}  // namespace DownPour
