#include "DownPour.h"

#include "logger/Logger.h"
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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);  // Hide and capture cursor
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
    createGraphicsPipeline();
    createWorldPipeline();
    createCommandPool();

    // Initialize material manager
    materialManager = new MaterialManager(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(), commandPool,
                                          vulkanContext.getGraphicsQueue());

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    loadRoadModel();

    // Load and setup car model
    loadCarModel();
    createCarPipeline();
    createCarTransparentPipeline();
    createCarDescriptorSets();

    // Initialize windshield surface
    windshield.initialize(vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(), commandPool,
                          vulkanContext.getGraphicsQueue());
    createWindshieldPipeline();

    createSyncObjects();

    float aspect = static_cast<float>(swapChainManager.getExtent().width) /
                   static_cast<float>(swapChainManager.getExtent().height);
    // Initialize camera (will be updated by updateCamera based on camera mode)
    camera = Camera({0.0f, 0.0f, 0.0f}, aspect);
    // Set FOV (wider for cockpit, normal for external)
    camera.setFOV(75.0f);
    // Set far plane to 10km so we can see the entire 6.5km road
    camera.setFarPlane(10000.0f);

    // Set initial camera mode to Cockpit
    camera.setMode(CameraMode::Cockpit);
    camera.setCockpitOffset(cockpitOffset);

    lastFrameTime = glfwGetTime();
}

void Application::cleanup() {
    // Clean up windshield resources
    windshield.cleanup(vulkanContext.getDevice());
    safeDestroy(windshieldPipeline, vkDestroyPipeline);
    safeDestroy(windshieldPipelineLayout, vkDestroyPipelineLayout);
    safeDestroy(windshieldDescriptorLayout, vkDestroyDescriptorSetLayout);

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

    // Clean up material manager
    if (materialManager) {
        materialManager->cleanup();
        delete materialManager;
        materialManager = nullptr;
    }

    // Clear material ID mappings
    carMaterialIds.clear();
    roadMaterialIds.clear();

    // Clean up car resources
    if (carModelPtr) {
        carModelPtr->cleanup(vulkanContext.getDevice());
        delete carModelPtr;
        carModelPtr = nullptr;
    }
    safeDestroy(carPipeline, vkDestroyPipeline);
    safeDestroy(carTransparentPipeline, vkDestroyPipeline);
    safeDestroy(carPipelineLayout, vkDestroyPipelineLayout);
    safeDestroy(carDescriptorSetLayout, vkDestroyDescriptorSetLayout);
    safeDestroy(carDescriptorPool, vkDestroyDescriptorPool);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(vulkanContext.getDevice(), uniformBuffers[i], nullptr);
        vkFreeMemory(vulkanContext.getDevice(), uniformBuffersMemory[i], nullptr);
    }

    safeDestroy(commandPool, vkDestroyCommandPool);
    safeDestroy(graphicsPipeline, vkDestroyPipeline);
    safeDestroy(pipelineLayout, vkDestroyPipelineLayout);

    // Clean up road model BEFORE destroying device
    if (roadModelPtr) {
        roadModelPtr->cleanup(vulkanContext.getDevice());
        delete roadModelPtr;
        roadModelPtr = nullptr;
    }
    safeDestroy(worldPipeline, vkDestroyPipeline);
    safeDestroy(worldPipelineLayout, vkDestroyPipelineLayout);

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
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.proj[1][1] *= -1;  // GLM for OpenGL, flip Y for Vulkan
    ubo.viewProj = ubo.proj * ubo.view;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Application::createGraphicsPipeline() {
    // Create pipeline layout
    pipelineLayout = PipelineFactory::createPipelineLayout(vulkanContext.getDevice(), {descriptorSetLayout});

    // Create pipeline
    PipelineConfig config;
    config.vertShader = "basic.vert.spv";
    config.fragShader = "basic.frag.spv";
    config.layout     = pipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;

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
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
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
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanContext.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) !=
        VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout!");
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

    // 1. Draw skybox
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[frameIndex], 0,
                            nullptr);
    vkCmdDraw(cmd, 36, 1, 0, 0);

    // ========================================================================
    // 2. Draw Road (glTF Model with PBR Materials)
    // ========================================================================
    // The road model (~50km long) is rendered with asphalt textures using
    // the same PBR pipeline as the car. This ensures consistent material
    // rendering and lighting across the scene.
    // Rendering Order: Skybox → Road (opaque) → Car (opaque) → Car (transparent)
    if (roadModelPtr->getIndexCount() > 0) {
        const auto&  roadMaterials       = roadModelPtr->getMaterials();
        VkBuffer     roadVertexBuffers[] = {roadModelPtr->getVertexBuffer()};
        VkDeviceSize roadOffsets[]       = {0};

        if (!roadMaterials.empty()) {
            // Road has PBR materials - use car pipeline (car.vert/frag shaders)
            // Materials include: base color (asphalt_01_diff_2k.jpg), roughness map
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipeline);

            // Push road model matrix (identity at ground level Y=0)
            glm::mat4 roadMatrix = roadModelPtr->getModelMatrix();
            vkCmdPushConstants(cmd, carPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &roadMatrix);

            // Bind road geometry buffers
            vkCmdBindVertexBuffers(cmd, 0, 1, roadVertexBuffers, roadOffsets);
            vkCmdBindIndexBuffer(cmd, roadModelPtr->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            // Render each material primitive (typically 1 material for roads)
            for (size_t i = 0; i < roadMaterials.size(); i++) {
                const auto& material = roadMaterials[i];

                // Get GPU material resources (textures + descriptor set)
                uint32_t        gpuId         = roadMaterialIds[i];
                VkDescriptorSet matDescriptor = materialManager->getDescriptorSet(gpuId, frameIndex);

                // Bind descriptor sets: [0] = Camera UBO, [1] = Material textures
                std::vector<VkDescriptorSet> sets = {descriptorSets[frameIndex], matDescriptor};
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipelineLayout, 0,
                                        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

                // Draw this material's index range
                vkCmdDrawIndexed(cmd, material.indexCount, 1, material.indexStart, 0, 0);
            }
        } else {
            // Fallback: Road has no materials - use simple world pipeline (untextured)
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 0, 1,
                                    &descriptorSets[frameIndex], 0, nullptr);
            vkCmdBindVertexBuffers(cmd, 0, 1, roadVertexBuffers, roadOffsets);
            vkCmdBindIndexBuffer(cmd, roadModelPtr->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, roadModelPtr->getIndexCount(), 1, 0, 0, 0);
        }
    }

    // 3. Draw car using scene graph (hierarchical rendering)
    Scene* activeScene = sceneManager.getActiveScene();

    if (activeScene) {
        // Update all transforms in the scene
        activeScene->updateTransforms();

        // Get render batches (grouped by model and transparency)
        auto batches = activeScene->getRenderBatches();

        // DEBUG: Track rendering statistics
        static bool debugPrinted        = false;
        int         totalNodes          = 0;
        int         skippedNoDescriptor = 0;
        int         drawnNodes          = 0;

        for (const auto& batch : batches) {
            if (!batch.model || batch.nodes.empty())
                continue;

            // Bind appropriate pipeline based on transparency
            VkPipeline pipeline = batch.isTransparent ? carTransparentPipeline : carPipeline;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            static bool loggedTransparency = false;
            if (!loggedTransparency && batch.isTransparent) {
                loggedTransparency = true;
            }

            // Bind model's vertex and index buffers ONCE per batch
            VkBuffer     vertexBuffers[] = {batch.model->getVertexBuffer()};
            VkDeviceSize offsets[]       = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, batch.model->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            // Draw each node instance with its own world transform
            for (SceneNode* node : batch.nodes) {
                totalNodes++;
                if (!node || !node->renderData || !node->renderData->isVisible)
                    continue;

                // Push THIS NODE's world transform (includes parent transforms)
                vkCmdPushConstants(cmd, carPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                                   &node->worldTransform);

                // Bind material descriptor set
                uint32_t        matId         = node->renderData->materialId;
                VkDescriptorSet matDescriptor = materialManager->getDescriptorSet(matId, frameIndex);

                // Skip nodes with no descriptor set (materials without textures)
                if (matDescriptor == VK_NULL_HANDLE) {
                    skippedNoDescriptor++;
                    continue;
                }

                // Bind descriptor sets: [0] = Camera UBO, [1] = Material textures
                std::vector<VkDescriptorSet> sets = {descriptorSets[frameIndex], matDescriptor};
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipelineLayout, 0,
                                        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

                // Draw this node's geometry
                vkCmdDrawIndexed(cmd, node->renderData->indexCount, 1, node->renderData->indexStart, 0, 0);
                drawnNodes++;
            }
        }
    }

    // 4. Draw debug markers (if enabled)

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
        float currentTime = glfwGetTime();
        float deltaTime   = currentTime - lastFrameTime;
        lastFrameTime     = currentTime;

        // Toggle cursor capture with ESC key
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            cursorCaptured = !cursorCaptured;
            if (cursorCaptured) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;  // Reset to avoid camera jump
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            // Small delay to prevent multiple toggles
            while (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwPollEvents();
            }
        }

        // Toggle weather with R key
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            weatherSystem.toggleWeather();
            // Small delay to prevent multiple toggles
            while (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                glfwPollEvents();
            }
        }

        // Update car physics
        updateCarPhysics(deltaTime);

        // Update camera based on car position and rotation
        if (camera.getMode() == CameraMode::Cockpit) {
            updateCameraForCockpit();
        }
        camera.updateCameraMode(deltaTime);

        // Update weather system
        weatherSystem.update(deltaTime);

        // Update windshield with rain data
        windshield.update(deltaTime, weatherSystem.getActiveDrops());

        // Handle wiper control with I key
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
            windshield.setWiperActive(true);
        } else {
            windshield.setWiperActive(false);
        }

        // Toggle debug visualization with V key
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
            debugVisualizationEnabled = !debugVisualizationEnabled;
            // Small delay to prevent multiple toggles
            while (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
                glfwPollEvents();
            }
        }

        // Cycle camera mode with C key
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            camera.cycleMode();
            // Small delay to prevent multiple toggles
            while (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
                glfwPollEvents();
            }
        }

        // Adjust cockpit offset with numpad keys (fine control)
        const float adjustStep    = 0.1f;  // 10cm increments
        bool        offsetChanged = false;

        // X axis (left/right) - Numpad 4/6
        if (glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS) {
            cockpitOffset.x -= adjustStep;
            offsetChanged = true;
        }
        if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS) {
            cockpitOffset.x += adjustStep;
            offsetChanged = true;
        }

        // Y axis (forward/back in model space) - Numpad 8/2
        if (glfwGetKey(window, GLFW_KEY_KP_8) == GLFW_PRESS) {
            cockpitOffset.y += adjustStep;
            offsetChanged = true;
        }
        if (glfwGetKey(window, GLFW_KEY_KP_2) == GLFW_PRESS) {
            cockpitOffset.y -= adjustStep;
            offsetChanged = true;
        }

        // Z axis (up/down in model space) - Numpad +/-
        if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
            cockpitOffset.z += adjustStep;
            offsetChanged = true;
        }
        if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
            cockpitOffset.z -= adjustStep;
            offsetChanged = true;
        }

        // Print updated offset when changed
        if (offsetChanged) {
            camera.setCockpitOffset(cockpitOffset);
            Log logger;
            logger.log("info", "Cockpit Offset: (" + std::to_string(cockpitOffset.x) + ", " +
                                   std::to_string(cockpitOffset.y) + ", " + std::to_string(cockpitOffset.z) + ")");
        }

        // Log camera and car position on L key press
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
            glm::vec3 camPos = camera.getPosition();
            Log       logger;
            logger.log("position", "Camera: (" + std::to_string(camPos.x) + ", " + std::to_string(camPos.y) + ", " +
                                       std::to_string(camPos.z) + ") | Car: (" + std::to_string(carPosition.x) + ", " +
                                       std::to_string(carPosition.y) + ", " + std::to_string(carPosition.z) +
                                       ") | Angle: " + std::to_string(carRotation));
            // Small delay to prevent multiple logs
            while (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                glfwPollEvents();
            }
        }

        // Update debug markers every frame (car position changes)

        glfwPollEvents();
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

    app->camera.processMouseMovement(xoffset, yoffset);
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
    // Create pipeline layout
    worldPipelineLayout = PipelineFactory::createPipelineLayout(vulkanContext.getDevice(), {descriptorSetLayout});

    // Create pipeline
    PipelineConfig config;
    config.vertShader = "world.vert.spv";
    config.fragShader = "world.frag.spv";
    config.layout     = worldPipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;

    worldPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config, swapChainManager.getRenderPass(),
                                                    swapChainManager.getExtent());
}

void Application::loadCarModel() {
    // NEW: Use ModelAdapter for data-driven loading
    carAdapter = new ModelAdapter();
    if (!carAdapter->load("assets/models/bmw/bmw.gltf", vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                          commandPool, vulkanContext.getGraphicsQueue())) {
        throw std::runtime_error("Failed to load car model via adapter");
    }
    carModelPtr = carAdapter->getModel();

    // Get hierarchy-aware dimensions for accurate scaling
    glm::vec3 hMin, hMax;
    carModelPtr->getHierarchyBounds(hMin, hMax);
    glm::vec3 hDimensions = hMax - hMin;

    // Use target length from adapter
    float targetLength = carAdapter->getTargetLength();
    if (targetLength <= 0.0f) {
        targetLength = 4.7f;  // Fallback
    }

    // Calculate scale factor based on length (Z dimension)
    carScaleFactor = targetLength / hDimensions.z;

    // Cache the bottom offset (lowest point in model space * scale)
    carBottomOffset = hMin.y * carScaleFactor;

    // Use cockpit offset from adapter metadata if available
    const auto& camCfg = carAdapter->getCameraConfig();
    if (camCfg.hasData) {
        cockpitOffset = camCfg.cockpit.position;
    } else {
        // Fallback to a better starting cockpit position if not in JSON
        float suggestedX = 0.0f;                            // Center
        float suggestedY = hMin.y + hDimensions.y * 0.4f;   // 40% from front
        float suggestedZ = hMin.z + hDimensions.z * 0.75f;  // 75% of height
        cockpitOffset    = glm::vec3(suggestedX, suggestedY, suggestedZ);
    }

    // Load GPU resources for car materials
    const auto& carMaterials = carModelPtr->getMaterials();
    for (size_t i = 0; i < carMaterials.size(); i++) {
        uint32_t gpuId    = materialManager->createMaterial(carMaterials[i]);
        carMaterialIds[i] = gpuId;
    }

    // NEW: Build scene from hierarchy
    Scene* drivingScene = sceneManager.createScene("driving");

    std::vector<NodeHandle> carRootNodes = SceneBuilder::buildFromModel(drivingScene, carModelPtr, carMaterialIds);

    // NEW: Create entity for player car with a WRAPPER ROOT
    // This ensures all glTF roots get the same transform when we move/scale the car
    playerCar = sceneManager.createEntity<CarEntity>("player_car", "driving");

    // Apply config from CarEntity
    auto& carConfig  = playerCar->getConfig();
    carConfig.length = targetLength;

    // Create a wrapper root node that will be the entity's root
    NodeHandle carWrapperRoot = drivingScene->createNode("car_wrapper_root");
    playerCar->addNode(carWrapperRoot);  // This becomes the entity root

    // Reparent all glTF root nodes under the wrapper
    for (size_t i = 0; i < carRootNodes.size(); i++) {
        drivingScene->setParent(carRootNodes[i], carWrapperRoot);
        playerCar->addNode(carRootNodes[i], "gltf_root_" + std::to_string(i));
    }

    // NEW: Find and tag specific parts for animation using ADAPTER ROLES

    auto tagRole = [&](const std::string& role) {
        std::string nodeName = carAdapter->getNodeNameForRole(role);
        if (!nodeName.empty()) {
            if (auto node = drivingScene->findNode(nodeName); node.isValid()) {
                playerCar->addNode(node, role);
                return true;
            }
        }
        return false;
    };

    tagRole(CarEntity::ROLE_WHEEL_FL);
    tagRole(CarEntity::ROLE_WHEEL_FR);
    tagRole(CarEntity::ROLE_WHEEL_RL);
    tagRole(CarEntity::ROLE_WHEEL_RR);

    if (tagRole(CarEntity::ROLE_STEERING_WHEEL_FRONT)) {
        carParts.hasSteeringWheelFront = true;
    }

    if (tagRole(CarEntity::ROLE_STEERING_WHEEL_BACK)) {
        carParts.hasSteeringWheelBack = true;  
    }

    if (tagRole(CarEntity::ROLE_WIPER_LEFT)) {
        carParts.hasWipers = true;
    }
    tagRole(CarEntity::ROLE_WIPER_RIGHT);

    // Tag other roles
    tagRole(CarEntity::ROLE_HOOD);
    tagRole(CarEntity::ROLE_DOOR_L);
    tagRole(CarEntity::ROLE_DOOR_R);
    tagRole(CarEntity::ROLE_HEADLIGHTS);
    tagRole(CarEntity::ROLE_TAILLIGHTS);

    // Apply physics configuration from adapter if available
    const auto& phys = carAdapter->getPhysicsConfig();
    if (phys.wheelBase > 0.0f) {
        auto& carConfig           = playerCar->getConfig();
        carConfig.wheelBase       = phys.wheelBase;
        carConfig.trackWidth      = phys.trackWidth;
        carConfig.wheelRadius     = phys.wheelRadius;
        carConfig.maxSteerAngle   = phys.maxSteerAngle;
        carConfig.maxAcceleration = phys.maxAcceleration;
        carConfig.maxBraking      = phys.maxBraking;

        carConfig.mass              = phys.mass;
        carConfig.dragCoefficient   = phys.dragCoefficient;
        carConfig.rollingResistance = phys.rollingResistance;
    }

    // Apply spawn configuration
    const auto& spawn = carAdapter->getSpawnConfig();
    if (spawn.hasData) {
        carPosition = glm::vec3(spawn.position.x, spawn.position.y, spawn.position.z);
        // Map Y rotation (assuming Y-up world)
        carRotation = spawn.rotation.y;
    }

    // Apply debug configuration
    const auto& dbg = carAdapter->getDebugConfig();
    if (dbg.hasData) {
        // Enable debug visualization if any specialized debug view is requested
        if (dbg.showColliders || dbg.showSkeleton || dbg.showVelocityVector) {
            debugVisualizationEnabled = true;
        }
        // Force simple camera target viz if requested
        if (dbg.showCameraTarget) {
            // This could be mapped to a specific flag if one existed,
            // for now we assume debugVisualizationEnabled covers it.
            debugVisualizationEnabled = true;
        }
    }

    // Create camera entity and attach to car
    cameraEntity = sceneManager.createEntity<CameraEntity>("cockpit_camera", "driving");

    // Configure camera from JSON metadata
    CameraEntity::CameraConfig camConfig;

    // Use camera configuration from JSON if available
    const auto& setupCamCfg = carAdapter->getCameraConfig();
    if (setupCamCfg.hasData) {
        const auto& cockpit   = setupCamCfg.cockpit;
        camConfig.localOffset = cockpit.position;
        if (cockpit.useQuaternion) {
            camConfig.localRotation = cockpit.rotation;
        } else {
            camConfig.localRotation = glm::quat(glm::radians(cockpit.eulerRotation));
        }
        camConfig.fov       = cockpit.fov;
        camConfig.nearPlane = cockpit.nearPlane;
        camConfig.farPlane  = cockpit.farPlane;
    } else {
        // Fallback to defaults
        camConfig.localOffset   = cockpitOffset;
        camConfig.localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        camConfig.fov           = 75.0f;
        camConfig.nearPlane     = 0.1f;
        camConfig.farPlane      = 10000.0f;
    }

    cameraEntity->setConfig(camConfig);

    // Attach camera to car
    cameraEntity->attachToParent(playerCar);

    sceneManager.setActiveScene("driving");
}

void Application::loadRoadModel() {
    roadAdapter = new ModelAdapter();
    if (!roadAdapter->load("assets/models/road.glb", vulkanContext.getDevice(), vulkanContext.getPhysicalDevice(),
                           commandPool, vulkanContext.getGraphicsQueue())) {
        throw std::runtime_error("Failed to load road model via adapter");
    }
    roadModelPtr = roadAdapter->getModel();

    // Log road model statistics

    glm::vec3 minBounds = roadModelPtr->getMinBounds();
    glm::vec3 maxBounds = roadModelPtr->getMaxBounds();

    // Position road at ground level (Y=0)
    glm::mat4 roadTransform = glm::mat4(1.0f);
    roadTransform           = glm::translate(roadTransform, glm::vec3(0.0f, 0.0f, 0.0f));

    // May need scaling depending on road.glb dimensions
    // For now, assume road.glb is already at correct scale
    roadModelPtr->setModelMatrix(roadTransform);

    // Load GPU resources for road materials
    const auto& roadMaterials = roadModelPtr->getMaterials();
    for (size_t i = 0; i < roadMaterials.size(); i++) {
        uint32_t gpuId     = materialManager->createMaterial(roadMaterials[i]);
        roadMaterialIds[i] = gpuId;
    }

    // Create RoadEntity
    // Note: We don't store a pointer in Application class yet, but it's managed by SceneManager
    // We assume 'driving' scene exists since loadCarModel usually runs first or around same time
    if (sceneManager.getScene("driving")) {
        RoadEntity* roadEntity = sceneManager.createEntity<RoadEntity>("road", "driving");
        // We could attach the road model nodes here if we had them as SceneNodes
        // For now, the road is drawn via legacy loop in drawFrame, but we have the Entity for logic
    }
}

void Application::createCarPipeline() {
    // Create descriptor set layout for texture
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding         = 0;
    samplerLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanContext.getDevice(), &layoutInfo, nullptr, &carDescriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create car descriptor set layout");
    }

    // Push constants for model matrix
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(glm::mat4);

    // Pipeline layout with both descriptor sets
    std::vector<VkDescriptorSetLayout> layouts = {descriptorSetLayout, carDescriptorSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(layouts.size());
    pipelineLayoutInfo.pSetLayouts            = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(vulkanContext.getDevice(), &pipelineLayoutInfo, nullptr, &carPipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create car pipeline layout");
    }

    PipelineConfig config;
    config.vertShader = "car.vert.spv";
    config.fragShader = "car.frag.spv";
    config.layout     = carPipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;

    carPipeline = PipelineFactory::createPipeline(vulkanContext.getDevice(), config, swapChainManager.getRenderPass(),
                                                  swapChainManager.getExtent());
}

void Application::createCarTransparentPipeline() {
    PipelineConfig config;
    config.vertShader       = "car.vert.spv";
    config.fragShader       = "car.frag.spv";
    config.layout           = carPipelineLayout;  // Reuse existing layout
    config.cullMode         = VK_CULL_MODE_NONE;
    config.enableBlending   = true;
    config.enableDepthWrite = false;

    carTransparentPipeline = PipelineFactory::createPipeline(
        vulkanContext.getDevice(), config, swapChainManager.getRenderPass(), swapChainManager.getExtent());
}

void Application::createCarDescriptorSets() {
    const auto& materials = carModelPtr->getMaterials();
    if (materials.empty()) {
        return;
    }

    // Create descriptor pool for ALL materials (road + car) * frames
    // We need to account for materials already created (road = 1) + car materials
    uint32_t totalMaterials = 1 + static_cast<uint32_t>(materials.size());  // road + car
    totalMaterials += 50;                                                   // Add buffer for safety

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = totalMaterials * MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = totalMaterials * MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(vulkanContext.getDevice(), &poolInfo, nullptr, &carDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create car descriptor pool");
    }

    // Initialize MaterialManager descriptor support
    // MaterialManager will use these to create descriptor sets for materials
    materialManager->initDescriptorSupport(carDescriptorSetLayout, carDescriptorPool, MAX_FRAMES_IN_FLIGHT);

    // Create descriptor sets for materials that were loaded before descriptor support was initialized
    materialManager->createDescriptorSetsForExistingMaterials();
}

void Application::updateCarPhysics(float deltaTime) {
    // Simple driving controls
    const float acceleration = 5.0f;   // m/s^2
    const float deceleration = 8.0f;   // m/s^2 (braking)
    const float maxSpeed     = 15.0f;  // m/s (about 54 km/h)
    const float friction     = 2.0f;   // m/s^2

    // Forward/backward controls
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        carVelocity += acceleration * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        carVelocity -= deceleration * deltaTime;
    }

    // Apply friction when no input
    if (glfwGetKey(window, GLFW_KEY_UP) != GLFW_PRESS && glfwGetKey(window, GLFW_KEY_W) != GLFW_PRESS &&
        glfwGetKey(window, GLFW_KEY_DOWN) != GLFW_PRESS && glfwGetKey(window, GLFW_KEY_S) != GLFW_PRESS) {
        if (carVelocity > 0) {
            carVelocity -= friction * deltaTime;
            if (carVelocity < 0)
                carVelocity = 0;
        } else if (carVelocity < 0) {
            carVelocity += friction * deltaTime;
            if (carVelocity > 0)
                carVelocity = 0;
        }
    }

    // Clamp velocity
    carVelocity = glm::clamp(carVelocity, -maxSpeed * 0.5f, maxSpeed);

    // Update car position (move along Z axis, which is forward)
    carPosition.z += carVelocity * deltaTime;

    // NEW: Update scene entity transform
    if (playerCar) {
        // Apply vertical offset relative to the rotation correction
        // The car model has an internal offset that we dynamically calculated
        glm::vec3 visualPosition = carPosition;
        visualPosition.y -= carBottomOffset;

        playerCar->setPosition(visualPosition);

        // Combine rotations: Model orientation fix + Y-axis (steering)
        glm::vec3 modelRot = carAdapter ? carAdapter->getModelRotation() : glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
        glm::quat fixRotation      = glm::quat(glm::vec3(modelRot.x, modelRot.y, modelRot.z));
        glm::quat yRotation        = glm::angleAxis(glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat combinedRotation = yRotation * fixRotation;  // Apply fix rotation first, then steering

        playerCar->setRotation(combinedRotation);
        playerCar->setScale(glm::vec3(1));

        // Animate wheels based on velocity
        static float wheelRotationAccum = 0.0f;
        wheelRotationAccum += carVelocity * deltaTime * 10.0f;  // Adjust multiplier for visual appearance
        glm::quat wheelRot = glm::angleAxis(wheelRotationAccum, glm::vec3(1.0f, 0.0f, 0.0f));

        playerCar->animateRotation("wheel_FL", wheelRot);
        playerCar->animateRotation("wheel_FR", wheelRot);
        playerCar->animateRotation("wheel_RL", wheelRot);
        playerCar->animateRotation("wheel_RR", wheelRot);
    }

    // Update steering wheel rotation based on A/D keys
    if (carParts.hasSteeringWheelFront && carParts.hasSteeringWheelBack) {
        const float maxSteeringAngle = 450.0f;  // degrees (1.25 full rotations)
        const float steeringSpeed    = 180.0f;  // degrees per second
        const float returnSpeed      = 360.0f;  // degrees per second

        bool turningLeft =
            (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
        bool turningRight =
            (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

        if (turningLeft && !turningRight) {
            steeringWheelRotation += steeringSpeed * deltaTime;
            steeringWheelRotation = glm::min(steeringWheelRotation, maxSteeringAngle);
        } else if (turningRight && !turningLeft) {
            steeringWheelRotation -= steeringSpeed * deltaTime;
            steeringWheelRotation = glm::max(steeringWheelRotation, -maxSteeringAngle);
        } else {
            // Return to center when no input
            if (steeringWheelRotation > 0.0f) {
                steeringWheelRotation -= returnSpeed * deltaTime;
                if (steeringWheelRotation < 0.0f)
                    steeringWheelRotation = 0.0f;
            } else if (steeringWheelRotation < 0.0f) {
                steeringWheelRotation += returnSpeed * deltaTime;
                if (steeringWheelRotation > 0.0f)
                    steeringWheelRotation = 0.0f;
            }
        }

        // NEW: Animate steering wheel in scene entity
        if (playerCar) {
            // Steering wheel rotates around Z axis
            glm::quat steeringRot = glm::angleAxis(glm::radians(steeringWheelRotation), glm::vec3(0.0f, 0.0f, 1.0f));
            playerCar->animateRotation("steering_wheel_front", steeringRot);
            playerCar->animateRotation("steering_wheel_back", steeringRot);
        }
    }

    // Update scene manager
    sceneManager.update(deltaTime);

    // DEBUG: Log car internal state
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        Log logger;
        logger.log("debug", "CarPos: (" + std::to_string(carPosition.x) + ", " + std::to_string(carPosition.y) + ", " +
                                std::to_string(carPosition.z) + ") | BottomOffset: " + std::to_string(carBottomOffset));
    }
}

void Application::updateCameraForCockpit() {
    // Use CameraEntity to get world-space position and rotation
    // The CameraEntity automatically follows the car through the scene graph
    if (!cameraEntity || !playerCar) {
        // Fallback to old system if camera entity not initialized
        glm::quat yRotation = glm::angleAxis(glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 finalOffset;
        glm::quat finalCamRot;

        const auto& camCfg = carAdapter->getCameraConfig();
        if (camCfg.hasData) {
            const auto& cockpit = camCfg.cockpit;
            finalOffset         = yRotation * cockpit.position;
            if (cockpit.useQuaternion) {
                finalCamRot = yRotation * cockpit.rotation;
            } else {
                finalCamRot = yRotation * glm::quat(glm::radians(cockpit.eulerRotation));
            }
            camera.setCameraTarget(carPosition + finalOffset, finalCamRot);
        } else {
            glm::mat4 rotationMatrix = glm::mat4(1.0f);
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

            glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(cockpitOffset, 0.0f));
            camera.setPosition(carPosition + rotatedOffset);
            camera.setYaw(0.0f);
            camera.setPitch(0.0f);
        }
        return;
    }

    // Use CameraEntity to get world-space position and rotation
    // This automatically accounts for car position, rotation, and the local offset
    glm::vec3 worldPos = cameraEntity->getWorldPosition();
    glm::quat worldRot = cameraEntity->getWorldRotation();

    // DEBUG: Log camera entity world position
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        Log logger;
        logger.log("debug", "CameraEntity WorldPos: (" + std::to_string(worldPos.x) + ", " +
                                std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) + ")");
    }

    // Ensure the Camera class's internal cockpitOffset doesn't double-offset
    // since we already have the transformed world position.
    camera.setCockpitOffset(glm::vec3(0.0f));
    camera.setCameraTarget(worldPos, worldRot);
}

void Application::createWindshieldPipeline() {
    // TODO: Implement windshield pipeline creation
    // This will be a simple quad rendering pipeline with the windshield shader
}

void Application::renderWindshield(VkCommandBuffer cmd, uint32_t frameIndex) {
    // TODO: Implement windshield rendering
    // This will render a full-screen quad with the windshield effect
    (void)cmd;
    (void)frameIndex;
}

}  // namespace DownPour
