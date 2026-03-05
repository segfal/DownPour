#include "DownPour.h"

#include "logger/Logger.h"
#include "renderer/TerrainGeometry.h"
#include "vulkan/VulkanTypes.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace DownPour {

void Application::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void Application::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "DownPour - Rain Simulator", nullptr, nullptr);

    // Setup mouse input
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);  
}

void Application::initVulkan() {
    // Initialize Vulkan core (instance, device, surface, queues)
    vulkanContext.initialize(window);

    // Find depth format and initialize swap chain
    VkFormat depthFormat = ResourceManager::findDepthFormat(vulkanContext.getPhysicalDevice());
    swapChainManager.initialize(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                                vulkanContext.getSurface(), window, depthFormat);

    createDepthResources();
    swapChainManager.createFramebuffers(vulkanContext.getDevice(), depthImageView);

    createDescriptorSetLayout();
    createMaterialDescriptorSetLayout();
    createGraphicsPipeline();
    createWorldPipeline();
    createCarPipeline();
    createTerrainPipeline();
    createScreenRainPipeline();
    createCommandPool();

    // Initialize material manager
    materialManager = new MaterialManager(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(), commandPool,
                                          vulkanContext.getGraphicsQueue());

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    // Initialize material manager descriptor support
    materialManager->initDescriptorSupport(materialDescriptorSetLayout, descriptorPool, MAX_FRAMES_IN_FLIGHT);

    createCommandBuffers();
    loadRoadModel();

    // Create descriptor sets for materials loaded from road model
    materialManager->createDescriptorSetsForExistingMaterials();
    createTerrain();

    // Initialize rain renderer
    rainRenderer = new RainRenderer();
    rainRenderer->init(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                       commandPool, vulkanContext.getGraphicsQueue(),
                       swapChainManager.getRenderPass(), swapChainManager.getExtent(),
                       descriptorSetLayout, MAX_FRAMES_IN_FLIGHT);

    loadCarModel();
    initCamera();
    createSyncObjects();

    lastFrameTime = glfwGetTime();
}


void Application::cleanup() {
    safeDestroy(depthImageView, vkDestroyImageView);
    safeDestroy(depthImage, vkDestroyImage);
    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(vulkanContext.getDevice(), depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    // Clean up swap chain resources
    swapChainManager.cleanup(vulkanContext.getDevice());

    // Clean up sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vulkanContext.getDevice(), imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(vulkanContext.getDevice(), renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(vulkanContext.getDevice(), inFlightFences[i], nullptr);
    }

    safeDestroy(descriptorPool, vkDestroyDescriptorPool);
    safeDestroy(descriptorSetLayout, vkDestroyDescriptorSetLayout);
    safeDestroy(materialDescriptorSetLayout, vkDestroyDescriptorSetLayout);

    // Clean up material manager
    if (materialManager) {
        materialManager->cleanup();
        delete materialManager;
        materialManager = nullptr;
    }

    // Clear material ID mappings
    roadMaterialIds.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(vulkanContext.getDevice(), uniformBuffers[i], nullptr);
        vkFreeMemory(vulkanContext.getDevice(), uniformBuffersMemory[i], nullptr);
    }

    safeDestroy(commandPool, vkDestroyCommandPool);
    safeDestroy(graphicsPipeline, vkDestroyPipeline);
    safeDestroy(pipelineLayout, vkDestroyPipelineLayout);

    // Clean up car model (carModelPtr is owned by carAdapter)
    if (carModelPtr) {
        carModelPtr->cleanup(vulkanContext.getDevice());
        carModelPtr = nullptr;
    }
    delete carAdapter;
    carAdapter = nullptr;
    carMaterialIds.clear();

    // Clean up road model BEFORE destroying device
    if (roadModelPtr) {
        roadModelPtr->cleanup(vulkanContext.getDevice());
        delete roadModelPtr;
        roadModelPtr = nullptr;
    }
    safeDestroy(carPipeline, vkDestroyPipeline);
    safeDestroy(carTransparentPipeline, vkDestroyPipeline);
    safeDestroy(worldPipeline, vkDestroyPipeline);
    safeDestroy(worldPipelineLayout, vkDestroyPipelineLayout);

    // Clean up rain renderer
    if (rainRenderer) {
        rainRenderer->cleanup(vulkanContext.getDevice());
        delete rainRenderer;
        rainRenderer = nullptr;
    }

    // Clean up terrain
    if (terrainGeometry) {
        terrainGeometry->cleanup(vulkanContext.getDevice());
        delete terrainGeometry;
        terrainGeometry = nullptr;
    }
    safeDestroy(terrainPipeline, vkDestroyPipeline);
    safeDestroy(terrainPipelineLayout, vkDestroyPipelineLayout);
    safeDestroy(screenRainPipeline, vkDestroyPipeline);
    safeDestroy(screenRainPipelineLayout, vkDestroyPipelineLayout);

    // VulkanContext handles cleanup of instance, device, surface
    vulkanContext.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(CameraUBO);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ResourceManager::createBuffer(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(), bufferSize,
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      uniformBuffers[i], uniformBuffersMemory[i]);

        // Persistent mapping
        vkMapMemory(vulkanContext.getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void Application::updateUniformBuffer(uint32_t currentImage) {
    CameraUBO ubo{};
    if (cameraEntity) {
        ubo.view = cameraEntity->getViewMatrix();
        ubo.proj = cameraEntity->getProjectionMatrix();
    } else {
        // Fallback if cameraEntity not initialized yet
        float aspect = static_cast<float>(swapChainManager.getExtent().width) /
                       static_cast<float>(swapChainManager.getExtent().height);
        ubo.view = glm::mat4(1.0f);
        ubo.proj = glm::perspective(glm::radians(75.0f), aspect, 0.1f, 10000.0f);
    }
    ubo.proj[1][1] *= -1;  // GLM for OpenGL, flip Y for Vulkan
    ubo.viewProj = ubo.proj * ubo.view;

    // Sun direction (normalized) and intensity
    // PBR shaders divide diffuse by PI for energy conservation, so sun
    // radiance must be > 1.0 to compensate. 5.0 gives a bright noon look.
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.4f, 0.8f, 0.3f));
    ubo.sunDirection = glm::vec4(sunDir, 5.0f);  // w = HDR sun intensity (bright noon)

    // Camera position
    if (cameraEntity) {
        Scene* scene = sceneManager.getActiveScene();
        if (scene) {
            NodeHandle cameraNodeHandle = cameraEntity->getNode("camera_root");
            SceneNode* cameraNode = scene->getNode(cameraNodeHandle);
            if (cameraNode) {
                glm::vec3 camPos = glm::vec3(cameraNode->worldTransform[3]);  // Extract position from transform
                ubo.cameraPosition = glm::vec4(camPos, static_cast<float>(glfwGetTime()));  // w = time
            }
        }
    }

    // Weather parameters for shaders
    glm::vec3 wind = weatherSystem.getWind();
    ubo.weatherParams = glm::vec4(
        weatherSystem.getRainIntensity(),
        weatherSystem.getWetness(),
        wind.x,
        wind.z
    );

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Application::createGraphicsPipeline() {
    // Create pipeline layout
    pipelineLayout = PipelineFactory::createPipelineLayout(vulkanContext.getDevice(), {descriptorSetLayout});

    // Create skybox pipeline (renders at depth=1.0, always behind everything)
    PipelineConfig config;
    config.vertShader        = "basic.vert.spv";
    config.fragShader        = "basic.frag.spv";
    config.layout            = pipelineLayout;
    config.cullMode          = VK_CULL_MODE_NONE;
    config.enableDepthWrite  = false;                     // Don't write depth
    config.depthCompareOp    = VK_COMPARE_OP_LESS_OR_EQUAL;  // Allow skybox at depth=1.0

    graphicsPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config,
                                                       swapChainManager.getRenderPass(), swapChainManager.getExtent());
}

Vulkan::QueueFamilyIndices Application::findQueueFamilies(VkPhysicalDevice device) {
    return vulkanContext.findQueueFamilies(device);
}

void Application::createCommandPool() {
    auto indices = vulkanContext.findQueueFamilies(vulkanContext.getPhysicalDevice());

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = indices.graphicsFamily.value();
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vulkanContext.getDevice(), &info, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool!");
}

void Application::createCommandBuffers() {
    commandBuffers.resize(swapChainManager.getFramebuffers().size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(vulkanContext.getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers!");
}

void Application::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo     fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vulkanContext.getDevice(), &semInfo, nullptr, &imageAvailableSemaphores[i]) !=
                VK_SUCCESS ||
            vkCreateSemaphore(vulkanContext.getDevice(), &semInfo, nullptr, &renderFinishedSemaphores[i]) !=
                VK_SUCCESS ||
            vkCreateFence(vulkanContext.getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}

void Application::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    // Uniform buffer pool
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    // Combined image sampler pool for materials
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // Car model has ~100 materials, each needs 3 samplers × 2 frames = 600+ descriptors
    poolSizes[1].descriptorCount = 1000;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 500);  // Camera + materials
    if (vkCreateDescriptorPool(vulkanContext.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}

void Application::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(vulkanContext.getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets!");

    // Bind each descriptor set to its uniform buffer
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(CameraUBO);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = descriptorSets[i];
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(vulkanContext.getDevice(), 1, &descriptorWrite, 0, nullptr);
    }
}

void Application::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanContext.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) !=
        VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout!");
}

void Application::createMaterialDescriptorSetLayout() {
    // 3 bindings: baseColor, normalMap, metallicRoughness
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    // Binding 0: Base color sampler
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Normal map sampler
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding 2: Metallic-roughness sampler
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkanContext.getDevice(), &layoutInfo, nullptr, &materialDescriptorSetLayout) !=
        VK_SUCCESS)
        throw std::runtime_error("Failed to create material descriptor set layout!");
}

void Application::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t frameIndex) {
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &begin);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.05f, 0.05f, 0.07f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass        = swapChainManager.getRenderPass();
    rp.framebuffer       = swapChainManager.getFramebuffers()[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = swapChainManager.getExtent();
    rp.clearValueCount   = 2;
    rp.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // Push constant struct: mat4 model (64 bytes) + float alphaMultiplier (4 bytes) = 68 bytes
    struct PushData {
        glm::mat4 model;
        float     alphaMultiplier;
    };

    // 1. Draw road FIRST (writes depth)

    if (roadModelPtr && roadModelPtr->getIndexCount() > 0 && !roadMaterialIds.empty()) {
        VkBuffer     roadVertexBuffers[] = {roadModelPtr->getVertexBuffer()};
        VkDeviceSize roadOffsets[]       = {0};

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 0, 1,
                                &descriptorSets[frameIndex], 0, nullptr);

        PushData roadPush{roadModelPtr->getModelMatrix(), 1.0f};
        vkCmdPushConstants(cmd, worldPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushData), &roadPush);

        VkDescriptorSet roadMaterialDescSet = materialManager->getDescriptorSet(roadMaterialIds[0], frameIndex);
        if (roadMaterialDescSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 1, 1,
                                    &roadMaterialDescSet, 0, nullptr);
        }

        vkCmdBindVertexBuffers(cmd, 0, 1, roadVertexBuffers, roadOffsets);
        vkCmdBindIndexBuffer(cmd, roadModelPtr->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, roadModelPtr->getIndexCount(), 1, 0, 0, 0);
    }

    // 1.5. Draw car — two passes: opaque first, then transparent (glass/mirrors)
    if (carModelPtr && carModelPtr->getIndexCount() > 0) {
        VkBuffer     carVertexBuffers[] = {carModelPtr->getVertexBuffer()};
        VkDeviceSize carOffsets[]       = {0};

        vkCmdBindVertexBuffers(cmd, 0, 1, carVertexBuffers, carOffsets);
        vkCmdBindIndexBuffer(cmd, carModelPtr->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        Scene* scene = sceneManager.getActiveScene();
        if (scene) {
            auto batches = scene->getRenderBatches();

            // Pass 1: Opaque car parts (depth write ON, no blending)
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 0, 1,
                                    &descriptorSets[frameIndex], 0, nullptr);

            for (const auto& batch : batches) {
                if (batch.model != carModelPtr) continue;
                for (const SceneNode* node : batch.nodes) {
                    if (!node->renderData || !node->renderData->isVisible) continue;
                    if (node->renderData->indexCount == 0) continue;
                    if (node->renderData->isTransparent) continue;  // Skip transparent in pass 1

                    PushData push{node->worldTransform, 1.0f};
                    vkCmdPushConstants(cmd, worldPipelineLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(PushData), &push);

                    VkDescriptorSet matDescSet = materialManager->getDescriptorSet(
                        node->renderData->materialId, frameIndex);
                    if (matDescSet != VK_NULL_HANDLE) {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 1, 1,
                                                &matDescSet, 0, nullptr);
                    }

                    vkCmdDrawIndexed(cmd, node->renderData->indexCount, 1,
                                     node->renderData->indexStart, 0, 0);
                }
            }

            // Pass 2: Transparent car parts (blending ON, depth write OFF)
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carTransparentPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 0, 1,
                                    &descriptorSets[frameIndex], 0, nullptr);

            for (const auto& batch : batches) {
                if (batch.model != carModelPtr) continue;
                for (const SceneNode* node : batch.nodes) {
                    if (!node->renderData || !node->renderData->isVisible) continue;
                    if (node->renderData->indexCount == 0) continue;
                    if (!node->renderData->isTransparent) continue;  // Only transparent in pass 2

                    // Get material alpha from MaterialManager
                    float alpha = materialManager->getProperties(node->renderData->materialId).alphaValue;

                    PushData push{node->worldTransform, alpha};
                    vkCmdPushConstants(cmd, worldPipelineLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(PushData), &push);

                    VkDescriptorSet matDescSet = materialManager->getDescriptorSet(
                        node->renderData->materialId, frameIndex);
                    if (matDescSet != VK_NULL_HANDLE) {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 1, 1,
                                                &matDescSet, 0, nullptr);
                    }

                    vkCmdDrawIndexed(cmd, node->renderData->indexCount, 1,
                                     node->renderData->indexStart, 0, 0);
                }
            }
        }
    }

    // 2. Draw terrain (grass strips alongside road)
    if (terrainGeometry && terrainGeometry->getIndexCount() > 0) {
        VkBuffer     terrainVertexBuffers[] = {terrainGeometry->getVertexBuffer()};
        VkDeviceSize terrainOffsets[]       = {0};

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline);

        // Bind camera descriptor set (set 0)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipelineLayout, 0, 1,
                                &descriptorSets[frameIndex], 0, nullptr);

        // Bind grass material descriptor set (set 1)
        VkDescriptorSet grassMaterialDescSet = materialManager->getDescriptorSet(grassMaterialId, frameIndex);
        if (grassMaterialDescSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipelineLayout, 1, 1,
                                    &grassMaterialDescSet, 0, nullptr);
        }

        vkCmdBindVertexBuffers(cmd, 0, 1, terrainVertexBuffers, terrainOffsets);
        vkCmdBindIndexBuffer(cmd, terrainGeometry->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, terrainGeometry->getIndexCount(), 1, 0, 0, 0);
    }

    // 3. Draw rain particles (transparent, after opaques, before skybox)
    if (rainRenderer && rainRenderer->getActiveCount() > 0) {
        rainRenderer->draw(cmd, frameIndex, descriptorSets[frameIndex]);
    }

    // 4. Draw skybox LAST (fills remaining pixels at depth=1.0)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[frameIndex], 0,
                            nullptr);
    vkCmdDraw(cmd, 36, 1, 0, 0);

    // 5. Screen-space rain drops on camera (fullscreen overlay)
    if (weatherSystem.getRainIntensity() > 0.01f) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, screenRainPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, screenRainPipelineLayout, 0, 1,
                                &descriptorSets[frameIndex], 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);  // Fullscreen triangle
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer");
}
// ==== Buffer Helper Methods (TODO) ====

void Application::drawFrame() {
    // Wait for previous frame
    vkWaitForFences(vulkanContext.getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vulkanContext.getDevice(), 1, &inFlightFences[currentFrame]);

    // Acquire image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(vulkanContext.getDevice(), swapChainManager.getSwapChain(), UINT64_MAX,
                          imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // Update uniforms
    updateUniformBuffer(currentFrame);

    // Record commands
    vkResetCommandBuffer(commandBuffers[imageIndex], 0);
    recordCommandBuffer(commandBuffers[imageIndex], imageIndex, currentFrame);

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo         submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &imageAvailableSemaphores[currentFrame];
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &commandBuffers[imageIndex];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];

    if (vkQueueSubmit(vulkanContext.getGraphicsQueue(), 1, &submit, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    // Present
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    present.swapchainCount     = 1;
    VkSwapchainKHR swapChain   = swapChainManager.getSwapChain();
    present.pSwapchains        = &swapChain;
    present.pImageIndices      = &imageIndex;

    vkQueuePresentKHR(vulkanContext.getPresentQueue(), &present);

    // Advance frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float currentTime = glfwGetTime();
        float deltaTime   = currentTime - lastFrameTime;
        lastFrameTime     = currentTime;

        // Toggle cursor capture with ESC key
        static bool escPressed = false;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escPressed) {
            cursorCaptured = !cursorCaptured;
            glfwSetInputMode(window, GLFW_CURSOR,
                cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (cursorCaptured) {
                firstMouse = true;
            }
            escPressed = true;
        } else if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) {
            escPressed = false;
        }

        // Toggle rain with R key (Sunny → Low → Heavy → Severe → Sunny)
        static bool rPressed = false;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rPressed) {
            weatherSystem.cycleWeather();
            Log logger;
            logger.log("info", std::string("Weather: ") + weatherSystem.getStateName());
            rPressed = true;
        } else if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) {
            rPressed = false;
        }

        // Car / camera input
        if (carEntity && cursorCaptured) {
            carEntity->update(deltaTime, window);
        } else if (cameraEntity && cursorCaptured) {
            cameraEntity->processInput(window, deltaTime);
        }

        // Update weather system (particle physics)
        glm::vec3 camPos(0.0f);
        if (cameraEntity) {
            Scene* scene = sceneManager.getActiveScene();
            if (scene) {
                NodeHandle cameraNodeHandle = cameraEntity->getNode("camera_root");
                SceneNode* cameraNode = scene->getNode(cameraNodeHandle);
                if (cameraNode) {
                    camPos = glm::vec3(cameraNode->worldTransform[3]);
                }
            }
        }
        weatherSystem.update(deltaTime, camPos);

        // Update rain GPU instances
        if (rainRenderer) {
            rainRenderer->updateInstances(weatherSystem.getDrops(), currentFrame);
        }

        // Update scene transforms
        Scene* activeScene = sceneManager.getActiveScene();
        if (activeScene) {
            activeScene->updateTransforms();
        }

        updateUniformBuffer(currentFrame);
        drawFrame();
    }

    vkDeviceWaitIdle(vulkanContext.getDevice());
}

void Application::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    // Only process mouse movement when cursor is captured
    if (!app->cursorCaptured) {
        return;
    }

    if (app->firstMouse) {
        app->lastX      = xpos;
        app->lastY      = ypos;
        app->firstMouse = false;
    }

    float xoffset = xpos - app->lastX;
    float yoffset = app->lastY - ypos;  // Reversed: y-coordinates go bottom to top

    app->lastX = xpos;
    app->lastY = ypos;

    if (app->cameraEntity) {
        app->cameraEntity->processMouseMovement(xoffset, yoffset);
    }
}

void Application::createDepthResources() {
    VkFormat depthFormat = ResourceManager::findDepthFormat(vulkanContext.getPhysicalDevice());

    ResourceManager::createImage(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                                 swapChainManager.getExtent().width, swapChainManager.getExtent().height, depthFormat,
                                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = depthImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(vulkanContext.getDevice(), &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view!");
    }
}

void Application::createWorldPipeline() {
    // Create pipeline layout with camera UBO (set 0), material textures (set 1), and push constants
    // Push constants: mat4 model (64 bytes) + float alphaMultiplier (4 bytes) = 68 bytes
    // Both vertex and fragment stages can access the full range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(glm::mat4) + sizeof(float);  // 68 bytes

    worldPipelineLayout = PipelineFactory::createPipelineLayoutWithPushConstants(
        vulkanContext.getDevice(), {descriptorSetLayout, materialDescriptorSetLayout}, {pushConstantRange});

    // Create pipeline
    PipelineConfig config;
    config.vertShader = "world.vert.spv";
    config.fragShader = "world.frag.spv";
    config.layout     = worldPipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;

    worldPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config, swapChainManager.getRenderPass(),
                                                    swapChainManager.getExtent());
}

void Application::createCarPipeline() {
    // Car opaque pipeline: shares worldPipelineLayout, car-specific PBR shaders
    PipelineConfig config;
    config.vertShader = "car.vert.spv";
    config.fragShader = "car.frag.spv";
    config.layout     = worldPipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;

    carPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config, swapChainManager.getRenderPass(),
                                                  swapChainManager.getExtent());

    // Car transparent pipeline: same shaders but with alpha blending, no depth write
    PipelineConfig transparentConfig;
    transparentConfig.vertShader       = "car.vert.spv";
    transparentConfig.fragShader       = "car.frag.spv";
    transparentConfig.layout           = worldPipelineLayout;
    transparentConfig.cullMode         = VK_CULL_MODE_NONE;
    transparentConfig.enableBlending   = true;
    transparentConfig.enableDepthWrite = false;

    carTransparentPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), transparentConfig,
                                                              swapChainManager.getRenderPass(),
                                                              swapChainManager.getExtent());
}

void Application::loadRoadModel() {
    roadAdapter = new ModelAdapter();
    if (!roadAdapter->load("assets/models/road.glb", vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                           commandPool, vulkanContext.getGraphicsQueue())) {
        throw std::runtime_error("Failed to load road model via adapter");
    }
    roadModelPtr = roadAdapter->getModel();

    glm::vec3 minBounds = roadModelPtr->getMinBounds();
    glm::vec3 maxBounds = roadModelPtr->getMaxBounds();

    Log roadLog;
    roadLog.log("info", "Road bounds min: (" + std::to_string(minBounds.x) + ", " +
                std::to_string(minBounds.y) + ", " + std::to_string(minBounds.z) + ")");
    roadLog.log("info", "Road bounds max: (" + std::to_string(maxBounds.x) + ", " +
                std::to_string(maxBounds.y) + ", " + std::to_string(maxBounds.z) + ")w");
    roadLog.log("info", "Road vertices: " + std::to_string(roadModelPtr->getIndexCount()) + " indices");

    // Position road at ground level (Y=0) and scale up environment
    glm::mat4 roadTransform = glm::mat4(1.0f);
    roadTransform           = glm::translate(roadTransform, glm::vec3(0.0f, 0.0f, 0.0f));
    roadTransform           = glm::scale(roadTransform, glm::vec3(3000.0f, 1.0f, 1000.0f)); // 3x wider in X, Y=1 to keep flat
    roadModelPtr->setModelMatrix(roadTransform);

    // Load GPU resources for road materials
    const auto& roadMaterials = roadModelPtr->getMaterials();
    for (size_t i = 0; i < roadMaterials.size(); i++) {
        uint32_t gpuId     = materialManager->createMaterial(roadMaterials[i]);
        roadMaterialIds[i] = gpuId;
    }

    // Create RoadEntity in main scene
    if (sceneManager.getScene("main")) {
        RoadEntity* roadEntity = sceneManager.createEntity<RoadEntity>("road", "main");
        // Road is drawn via legacy loop in recordCommandBuffer
    }
}

void Application::loadCarModel() {
    carAdapter = new ModelAdapter();
    if (!carAdapter->load("assets/models/bmw_suv.glb", vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                           commandPool, vulkanContext.getGraphicsQueue())) {
        Log logger;
        logger.log("warning", "Failed to load car model - continuing without car");
        delete carAdapter;
        carAdapter = nullptr;
        return;
    }
    carModelPtr = carAdapter->getModel();

    Log logger;
    glm::vec3 minBounds = carModelPtr->getMinBounds();
    glm::vec3 maxBounds = carModelPtr->getMaxBounds();
    logger.log("info", "Car bounds min: (" + std::to_string(minBounds.x) + ", " +
                std::to_string(minBounds.y) + ", " + std::to_string(minBounds.z) + ")");
    logger.log("info", "Car bounds max: (" + std::to_string(maxBounds.x) + ", " +
                std::to_string(maxBounds.y) + ", " + std::to_string(maxBounds.z) + ")");

    // Set car model matrix (identity for now, positioned via scene node)
    carModelPtr->setModelMatrix(glm::mat4(1.0f));

    // Load GPU resources for car materials
    const auto& carMaterials = carModelPtr->getMaterials();
    for (size_t i = 0; i < carMaterials.size(); i++) {
        uint32_t gpuId    = materialManager->createMaterial(carMaterials[i]);
        carMaterialIds[i] = gpuId;
    }
    materialManager->createDescriptorSetsForExistingMaterials();

    // Create car entity in main scene
    Scene* scene = sceneManager.getScene("main");
    if (!scene) {
        scene = sceneManager.createScene("main");
    }

    carEntity = sceneManager.createEntity<CarEntity>("car", "main", carAdapter);

    // Build scene graph from glTF hierarchy
    auto rootHandles = SceneBuilder::buildFromModel(scene, carModelPtr, carMaterialIds);

    // Use first root as entity root
    if (!rootHandles.empty()) {
        carEntity->addNode(rootHandles[0], "car_root");

        // Map GLB node names to entity roles using sidecar data
        if (carAdapter) {
            for (const auto& roleName : {"steering_wheel",
                    "wheel_FL", "wheel_FR", "wheel_BL", "wheel_BR",
                    "hood", "trunk",
                    "disc_FL", "disc_FR", "disc_BL", "disc_BR"}) {
                std::string nodeName = carAdapter->getNodeNameForRole(roleName);
                if (!nodeName.empty()) {
                    NodeHandle nh = scene->findNode(nodeName);
                    if (nh.isValid()) {
                        carEntity->addNode(nh, roleName);
                    }
                }
            }
        }

        // Position car on the road
        SceneNode* carRoot = scene->getNode(rootHandles[0]);
        if (carRoot) {
            carRoot->setLocalPosition(glm::vec3(0.0f, 0.0f, 3000.0f));
            scene->markSubtreeDirty(rootHandles[0]);
        }
    }

    // Initialize cockpit camera
    carEntity->initCockpitCamera(scene);

    logger.log("info", "Car entity created with " + std::to_string(carMaterials.size()) + " materials");
}

void Application::initCamera() {
    // Get or create main scene
    Scene* scene = sceneManager.getScene("main");
    if (!scene) {
        scene = sceneManager.createScene("main");
    }

    float aspect = static_cast<float>(swapChainManager.getExtent().width) /
                   static_cast<float>(swapChainManager.getExtent().height);

    // Use cockpit camera if car entity is available
    if (carEntity && carEntity->getCockpitCamera()) {
        cameraEntity = carEntity->getCockpitCamera();
        cameraEntity->setAspectRatio(aspect);

        Log logger;
        logger.log("info", "Cockpit camera initialized (attached to car)");
    } else {
        // Fallback: free-fly camera
        cameraEntity = sceneManager.createEntity<CameraEntity>("free_camera", "main", nullptr);
        cameraEntity->setAspectRatio(aspect);

        NodeHandle cameraNode = scene->createNode("camera_root");
        cameraEntity->addNode(cameraNode, "camera_root");

        SceneNode* node = scene->getNode(cameraNode);
        if (node) {
            node->setLocalPosition(glm::vec3(0.0f, 2000.0f, 5000.0f));
            scene->markSubtreeDirty(cameraNode);
        }

        CameraEntity::CameraConfig config;
        config.fov = 75.0f;
        config.nearPlane = 1.0f;
        config.farPlane = 10000000.0f;
        cameraEntity->setConfig(config);

        Log logger;
        logger.log("info", "Free-flying camera initialized at (0, 2000, 5000)");
    }

    // Set active scene
    sceneManager.setActiveScene("main");
}

void Application::createScreenRainPipeline() {
    // Layout: camera UBO only (reuse descriptorSetLayout for set 0)
    screenRainPipelineLayout =
        PipelineFactory::createPipelineLayout(vulkanContext.getDevice(), {descriptorSetLayout});

    PipelineConfig config;
    config.vertShader     = "screen_rain.vert.spv";
    config.fragShader     = "screen_rain.frag.spv";
    config.layout         = screenRainPipelineLayout;
    config.enableBlending    = true;
    config.enableDepthWrite  = false;
    config.depthCompareOp    = VK_COMPARE_OP_ALWAYS;  // Overlay ignores depth
    config.cullMode          = VK_CULL_MODE_NONE;
    config.noVertexInput     = true;  // Fullscreen triangle from gl_VertexIndex

    screenRainPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config,
                                                          swapChainManager.getRenderPass(), swapChainManager.getExtent());
}

void Application::createTerrainPipeline() {
    // Create pipeline layout with camera UBO (set 0) and material textures (set 1)
    terrainPipelineLayout = PipelineFactory::createPipelineLayout(vulkanContext.getDevice(),
                                                                  {descriptorSetLayout, materialDescriptorSetLayout});

    // Create pipeline
    PipelineConfig config;
    config.vertShader = "terrain.vert.spv";
    config.fragShader = "terrain.frag.spv";
    config.layout     = terrainPipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;  // Render both sides of grass

    terrainPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config,
                                                      swapChainManager.getRenderPass(), swapChainManager.getExtent());
}

void Application::createTerrain() {
    // Generate terrain geometry
    terrainGeometry = new TerrainGeometry();

    // Match terrain to scaled road model bounds (1000x scale, 3x wider road in X)
    // Road bounds: X [-459,000, +459,000] (153*3000), Z [-25,000,000, +25,000,000]
    float roadWidth     = 920000.0f;  // ~306*3000, slightly wider than scaled road
    float terrainWidth  = 500000.0f;  // 500 * 1000, grass on each side of road
    float terrainLength = 6000000.0f; // 6000 * 1000, terrain along Z
    float texTileSize   = 10000.0f;   // 10 * 1000, repeat grass texture at scaled interval

    // Center terrain on Z=0 (road extends ±25km, camera starts at Z=5)
    terrainGeometry->generateCentered(roadWidth, terrainWidth, terrainLength, texTileSize);
    terrainGeometry->createBuffers(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(), commandPool,
                                   vulkanContext.getGraphicsQueue());

    // Load grass texture
    Material grassMaterial;
    grassMaterial.name             = "grass";
    grassMaterial.baseColorTexture = "assets/textures/grass/grass_diff.jpg";
    grassMaterialId                = materialManager->createMaterial(grassMaterial);

    Log logger;
    logger.log("info", "Terrain created with " + std::to_string(terrainGeometry->getIndexCount() / 3) + " triangles");
}

}  // namespace DownPour
