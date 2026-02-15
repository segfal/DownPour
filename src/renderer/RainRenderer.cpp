// SPDX-License-Identifier: MIT
#include "RainRenderer.h"

#include "../core/PipelineFactory.h"
#include "../core/ResourceManager.h"

#include <cstring>
#include <stdexcept>

namespace DownPour {

void RainRenderer::init(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool, VkQueue graphicsQueue,
                        VkRenderPass renderPass, VkExtent2D extent,
                        VkDescriptorSetLayout cameraDescriptorSetLayout,
                        uint32_t framesInFlight) {
    this->device = device;
    createQuadGeometry(device, physicalDevice, commandPool, graphicsQueue);
    createInstanceBuffers(device, physicalDevice, framesInFlight);
    createPipeline(device, renderPass, extent, cameraDescriptorSetLayout);
}

void RainRenderer::createQuadGeometry(VkDevice device, VkPhysicalDevice physicalDevice,
                                      VkCommandPool commandPool, VkQueue graphicsQueue) {
    // Unit quad: 4 corners
    RainQuadVertex vertices[4] = {
        {{-0.5f, -0.5f}},  // bottom-left
        {{ 0.5f, -0.5f}},  // bottom-right
        {{-0.5f,  0.5f}},  // top-left
        {{ 0.5f,  0.5f}},  // top-right
    };
    uint32_t indices[6] = {0, 1, 2, 2, 1, 3};

    VkDeviceSize vertSize = sizeof(vertices);
    VkDeviceSize idxSize = sizeof(indices);

    // Staging → device-local for quad vertices
    VkBuffer stagingVert, stagingIdx;
    VkDeviceMemory stagingVertMem, stagingIdxMem;

    ResourceManager::createBuffer(device, physicalDevice, vertSize,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingVert, stagingVertMem);
    void* data;
    vkMapMemory(device, stagingVertMem, 0, vertSize, 0, &data);
    memcpy(data, vertices, vertSize);
    vkUnmapMemory(device, stagingVertMem);

    ResourceManager::createBuffer(device, physicalDevice, vertSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  quadVertexBuffer, quadVertexMemory);
    ResourceManager::copyBuffer(device, commandPool, graphicsQueue, stagingVert, quadVertexBuffer, vertSize);

    vkDestroyBuffer(device, stagingVert, nullptr);
    vkFreeMemory(device, stagingVertMem, nullptr);

    // Staging → device-local for quad indices
    ResourceManager::createBuffer(device, physicalDevice, idxSize,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingIdx, stagingIdxMem);
    vkMapMemory(device, stagingIdxMem, 0, idxSize, 0, &data);
    memcpy(data, indices, idxSize);
    vkUnmapMemory(device, stagingIdxMem);

    ResourceManager::createBuffer(device, physicalDevice, idxSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  quadIndexBuffer, quadIndexMemory);
    ResourceManager::copyBuffer(device, commandPool, graphicsQueue, stagingIdx, quadIndexBuffer, idxSize);

    vkDestroyBuffer(device, stagingIdx, nullptr);
    vkFreeMemory(device, stagingIdxMem, nullptr);
}

void RainRenderer::createInstanceBuffers(VkDevice device, VkPhysicalDevice physicalDevice,
                                         uint32_t framesInFlight) {
    VkDeviceSize bufferSize = sizeof(RainInstance) * MAX_INSTANCES;

    instanceBuffers.resize(framesInFlight);
    instanceMemories.resize(framesInFlight);
    instanceMapped.resize(framesInFlight);

    for (uint32_t i = 0; i < framesInFlight; i++) {
        ResourceManager::createBuffer(device, physicalDevice, bufferSize,
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      instanceBuffers[i], instanceMemories[i]);
        vkMapMemory(device, instanceMemories[i], 0, bufferSize, 0, &instanceMapped[i]);
    }
}

void RainRenderer::createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
                                  VkDescriptorSetLayout cameraDescriptorSetLayout) {
    // Pipeline layout: only camera UBO (set 0)
    pipelineLayout = PipelineFactory::createPipelineLayout(device, {cameraDescriptorSetLayout});

    // Custom vertex input: binding 0 = quad corner, binding 1 = instance data
    PipelineConfig config;
    config.vertShader = "rain.vert.spv";
    config.fragShader = "rain.frag.spv";
    config.layout = pipelineLayout;
    config.enableBlending = true;
    config.enableDepthWrite = false;
    config.depthCompareOp = VK_COMPARE_OP_LESS;
    config.cullMode = VK_CULL_MODE_NONE;

    // Binding 0: per-vertex quad corner (vec2)
    VkVertexInputBindingDescription quadBinding{};
    quadBinding.binding = 0;
    quadBinding.stride = sizeof(RainQuadVertex);
    quadBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Binding 1: per-instance rain data
    VkVertexInputBindingDescription instanceBinding{};
    instanceBinding.binding = 1;
    instanceBinding.stride = sizeof(RainInstance);
    instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    config.customBindings = {quadBinding, instanceBinding};

    // Attributes
    VkVertexInputAttributeDescription attrs[5] = {};

    // location 0: corner (binding 0)
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;

    // location 1: position (binding 1)
    attrs[1].binding = 1;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(RainInstance, position);

    // location 2: alpha (binding 1)
    attrs[2].binding = 1;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32_SFLOAT;
    attrs[2].offset = offsetof(RainInstance, alpha);

    // location 3: velocity (binding 1)
    attrs[3].binding = 1;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[3].offset = offsetof(RainInstance, velocity);

    // location 4: size (binding 1)
    attrs[4].binding = 1;
    attrs[4].location = 4;
    attrs[4].format = VK_FORMAT_R32_SFLOAT;
    attrs[4].offset = offsetof(RainInstance, size);

    config.customAttributes = {attrs, attrs + 5};

    pipeline = PipelineFactory::createPipeline(device, config, renderPass, extent);
}

void RainRenderer::updateInstances(const std::vector<Simulation::Raindrop>& drops, uint32_t frameIndex) {
    auto* dst = static_cast<RainInstance*>(instanceMapped[frameIndex]);
    uint32_t count = 0;

    for (const auto& drop : drops) {
        if (!drop.active) continue;
        if (count >= MAX_INSTANCES) break;

        dst[count].position = drop.position;
        dst[count].alpha = 1.0f;
        dst[count].velocity = drop.velocity;
        dst[count].size = drop.size;
        count++;
    }

    activeInstanceCount = count;
}

void RainRenderer::draw(VkCommandBuffer cmdBuffer, uint32_t frameIndex,
                        VkDescriptorSet cameraDescriptorSet) {
    if (activeInstanceCount == 0) return;

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &cameraDescriptorSet, 0, nullptr);

    VkBuffer vertexBuffers[2] = {quadVertexBuffer, instanceBuffers[frameIndex]};
    VkDeviceSize offsets[2] = {0, 0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, quadIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmdBuffer, 6, activeInstanceCount, 0, 0, 0);
}

void RainRenderer::cleanup(VkDevice device) {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < instanceBuffers.size(); i++) {
        if (instanceMapped[i]) {
            vkUnmapMemory(device, instanceMemories[i]);
        }
        vkDestroyBuffer(device, instanceBuffers[i], nullptr);
        vkFreeMemory(device, instanceMemories[i], nullptr);
    }
    instanceBuffers.clear();
    instanceMemories.clear();
    instanceMapped.clear();

    if (quadVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, quadVertexBuffer, nullptr);
        vkFreeMemory(device, quadVertexMemory, nullptr);
    }
    if (quadIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, quadIndexBuffer, nullptr);
        vkFreeMemory(device, quadIndexMemory, nullptr);
    }
}

}  // namespace DownPour
