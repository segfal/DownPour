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

    // 2. Draw road
    if (roadModelPtr && roadModelPtr->getIndexCount() > 0) {
        VkBuffer     roadVertexBuffers[] = {roadModelPtr->getVertexBuffer()};
        VkDeviceSize roadOffsets[]       = {0};

        // Use simple world pipeline for road rendering
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 0, 1,
                                &descriptorSets[frameIndex], 0, nullptr);
        vkCmdBindVertexBuffers(cmd, 0, 1, roadVertexBuffers, roadOffsets);
        vkCmdBindIndexBuffer(cmd, roadModelPtr->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, roadModelPtr->getIndexCount(), 1, 0, 0, 0);
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

        // Camera input (WASD movement)
        if (cameraEntity && cursorCaptured) {
            cameraEntity->processInput(window, deltaTime);
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

    // Create RoadEntity in main scene
    if (sceneManager.getScene("main")) {
        RoadEntity* roadEntity = sceneManager.createEntity<RoadEntity>("road", "main");
        // Road is drawn via legacy loop in recordCommandBuffer
    }
}

void Application::initCamera() {
    // Get or create main scene
    Scene* scene = sceneManager.getScene("main");
    if (!scene) {
        scene = sceneManager.createScene("main");
    }

    // Create standalone camera entity (no parent, no ModelAdapter)
    cameraEntity = sceneManager.createEntity<CameraEntity>("free_camera", "main", nullptr);

    // Set aspect ratio
    float aspect = static_cast<float>(swapChainManager.getExtent().width) /
                   static_cast<float>(swapChainManager.getExtent().height);
    cameraEntity->setAspectRatio(aspect);

    // Create root node for camera (no parent)
    NodeHandle cameraNode = scene->createNode("camera_root");
    cameraEntity->addNode(cameraNode, "camera_root");

    // Set initial position (elevated to see road)
    SceneNode* node = scene->getNode(cameraNode);
    if (node) {
        node->setLocalPosition(glm::vec3(0.0f, 2.0f, 5.0f)); // 2m up, 5m forward
        scene->markSubtreeDirty(cameraNode);
    }

    // Set camera config for free-fly
    CameraEntity::CameraConfig config;
    config.fov = 75.0f;
    config.nearPlane = 0.1f;
    config.farPlane = 10000.0f;
    cameraEntity->setConfig(config);

    // Set active scene
    sceneManager.setActiveScene("main");

    Log logger;
    logger.log("info", "Free-flying camera initialized at (0, 2, 5)");
}

}  // namespace DownPour
