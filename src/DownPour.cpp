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

namespace {
// Load a binary file into a byte buffer. Tries both local and parent shader directories.
std::vector<char> readFile(const std::string& filename) {
    std::vector<std::filesystem::path> candidates = {std::filesystem::path("shaders") / filename,
                                                     std::filesystem::path("..") / "shaders" / filename};

    for (const auto& path : candidates) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) continue;
        const size_t      fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }

    throw std::runtime_error("Failed to open shader file: " + filename);
}
}  // namespace

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
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createDepthResources();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createWorldPipeline();
    createFramebuffers();
    createCommandPool();

    // Initialize material manager
    materialManager = new MaterialManager(device, physicalDevice, commandPool, graphicsQueue);

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
    windshield.initialize(device, physicalDevice, commandPool, graphicsQueue);
    createWindshieldPipeline();


    createSyncObjects();

    float aspect = static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
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
    windshield.cleanup(device);
    safeDestroy(windshieldPipeline, vkDestroyPipeline);
    safeDestroy(windshieldPipelineLayout, vkDestroyPipelineLayout);
    safeDestroy(windshieldDescriptorLayout, vkDestroyDescriptorSetLayout);

    safeDestroy(depthImageView, vkDestroyImageView);
    safeDestroy(depthImage, vkDestroyImage);
    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    for (auto framebuffer : swapchainFramebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
    for (auto imageView : swapchainImageViews) vkDestroyImageView(device, imageView, nullptr);
    safeDestroy(renderPass, vkDestroyRenderPass);

    // Clean up sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
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
        carModelPtr->cleanup(device);
        delete carModelPtr;
        carModelPtr = nullptr;
    }
    safeDestroy(carPipeline, vkDestroyPipeline);
    safeDestroy(carTransparentPipeline, vkDestroyPipeline);
    safeDestroy(carPipelineLayout, vkDestroyPipelineLayout);
    safeDestroy(carDescriptorSetLayout, vkDestroyDescriptorSetLayout);
    safeDestroy(carDescriptorPool, vkDestroyDescriptorPool);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    safeDestroy(commandPool, vkDestroyCommandPool);
    safeDestroy(graphicsPipeline, vkDestroyPipeline);
    safeDestroy(pipelineLayout, vkDestroyPipelineLayout);

    // Clean up road model BEFORE destroying device
    if (roadModelPtr) {
        roadModelPtr->cleanup(device);
        delete roadModelPtr;
        roadModelPtr = nullptr;
    }
    safeDestroy(worldPipeline, vkDestroyPipeline);
    safeDestroy(worldPipelineLayout, vkDestroyPipelineLayout);

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(CameraUBO);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i],
                     uniformBuffersMemory[i]);

        // Persistent mapping
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
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

void Application::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = findDepthFormat();
    depthAttachment.format         = findDepthFormat();
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    renderPassInfo.attachmentCount                     = 2;
    renderPassInfo.pAttachments                        = attachments.data();
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) throw std::runtime_error("Failed to create render pass!");
}

VkShaderModule Application::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) throw std::runtime_error("Failed to create shader module");
    return shaderModule;
}

void Application::createGraphicsPipeline() {
    auto vertShaderCode = readFile("basic.vert.spv");
    auto fragShaderCode = readFile("basic.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapchainExtent.width);
    viewport.height   = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;  // keep triangle visible regardless of winding
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = pipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void Application::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("Failed to create window surface!");
}

void Application::createInstance() {
    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "DownPour";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    // Get required GLFW extensions
    uint32_t     glfwExtensionCount = 0;
    const char** glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Add portability extension for macOS compatibility
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    // Log enabled extensions

    // Create Vulkan instance
    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = 0;
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan instance!");
}

Vulkan::QueueFamilyIndices Application::findQueueFamilies(VkPhysicalDevice device) {
    Vulkan::QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) indices.presentFamily = i;

        if (indices.isComplete()) break;

        i++;
    }

    return indices;
}

void Application::createCommandPool() {
    auto indicies = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = indicies.graphicsFamily.value();
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &info, nullptr, &commandPool) != VK_SUCCESS) throw std::runtime_error("Failed to create command pool!");
}

void Application::createCommandBuffers() {
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) throw std::runtime_error("Failed to allocate command buffers!");
}

void Application::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo     fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
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
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) throw std::runtime_error("Failed to create descriptor pool");
}

void Application::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) throw std::runtime_error("Failed to allocate descriptor sets!");

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

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void Application::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) throw std::runtime_error("Failed to find GPUs with Vulkan support!");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        Vulkan::QueueFamilyIndices indices = findQueueFamilies(device);
        if (indices.isComplete()) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("Failed to find a suitable GPU!");
}

void Application::createLogicalDevice() {
    Vulkan::QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    // Enable required device extensions
    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Check if portability subset extension is available (required for MoltenVK on macOS)
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    bool portabilitySubsetAvailable = false;
    for (const auto& extension : availableExtensions) if (strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) { portabilitySubsetAvailable = true; break; }

    if (portabilitySubsetAvailable) deviceExtensions.push_back("VK_KHR_portability_subset");

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("Failed to create logical device!");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

Vulkan::SwapChainSupportDetails Application::querySwapChainSupport(VkPhysicalDevice device) {
    Vulkan::SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat;
    return availableFormats[0];
}

VkPresentModeKHR Application::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width  = std::max(capabilities.minImageExtent.width,
                                       std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height,
                                       std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Application::createSwapChain() {
    Vulkan::SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) imageCount = swapChainSupport.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Vulkan::QueueFamilyIndices indices              = findQueueFamilies(physicalDevice);
    uint32_t                   queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) throw std::runtime_error("Failed to create swap chain!");

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent      = extent;
}

void Application::createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {swapchainImageViews[i], depthImageView};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = renderPass;
        framebufferInfo.attachmentCount = 2;  // Changed from 1
        framebufferInfo.pAttachments    = attachments.data();
        framebufferInfo.width           = swapchainExtent.width;
        framebufferInfo.height          = swapchainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) throw std::runtime_error("Failed to create framebuffer!");
    }
}

void Application::createImageViews() {
    swapchainImageViews.resize(swapchainImages.size());
    VkImageViewCreateInfo view{};
    view.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    view.format     = swapchainImageFormat;
    view.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY};
    view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel   = 0;
    view.subresourceRange.levelCount     = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount     = 1;
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        view.image = swapchainImages[i];
        if (vkCreateImageView(device, &view, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) throw std::runtime_error("Failed to create image views!");
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

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) throw std::runtime_error("Failed to create descriptor set layout!");
}

void Application::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t frameIndex) {
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &begin);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.05f, 0.05f, 0.07f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass        = renderPass;
    rp.framebuffer       = swapchainFramebuffers[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = swapchainExtent;
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

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) throw std::runtime_error("Failed to record command buffer");

}
// ==== Buffer Helper Methods (TODO) ====

void Application::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                               VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;  // Structure type
    bufferInfo.size        = size;                                  // Size of the buffer in bytes
    bufferInfo.usage       = usage;                                 // Usage flags
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;             // Exclusive access

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;                             // Memory requirements for the buffer
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);  // Get memory requirements
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;                      // Structure type
    allocInfo.allocationSize  = memRequirements.size;                                        // Size of allocation
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);  // Memory type index
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

uint32_t Application::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

// end of Buffer Helper Methods ====

void Application::drawFrame() {
    // Wait for previous frame
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // Acquire image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                          &imageIndex);

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

    if (vkQueueSubmit(graphicsQueue, 1, &submit, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    // Present
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];
    present.swapchainCount     = 1;
    present.pSwapchains        = &swapchain;
    present.pImageIndices      = &imageIndex;

    vkQueuePresentKHR(presentQueue, &present);

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
        // IMPORTANT: Car model has a 90° X-axis rotation, we need to account for this
        glm::vec3 modelRot = carAdapter ? carAdapter->getModelRotation() : glm::vec3(90.0f, 0.0f, 0.0f);
        glm::quat fixRotation =
            glm::quat(glm::vec3(glm::radians(modelRot.x), glm::radians(modelRot.y), glm::radians(modelRot.z)));
        glm::quat yRotation = glm::angleAxis(glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat carQuat   = yRotation * fixRotation;  // Combine steering + model orientation

        camera.setCameraTarget(carPosition, carQuat);
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
            logger.log("info",
                "Cockpit Offset: (" + std::to_string(cockpitOffset.x) + ", " +
                std::to_string(cockpitOffset.y) + ", " +
                std::to_string(cockpitOffset.z) + ")");
        }

        // Log camera and car position on L key press
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
            glm::vec3 camPos = camera.getPosition();
            Log logger;
            logger.log("position",
                "Camera: (" + std::to_string(camPos.x) + ", " +
                std::to_string(camPos.y) + ", " +
                std::to_string(camPos.z) + ") | Car: (" +
                std::to_string(carPosition.x) + ", " +
                std::to_string(carPosition.y) + ", " +
                std::to_string(carPosition.z) + ") | Angle: " +
                std::to_string(carRotation));
            // Small delay to prevent multiple logs
            while (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                glfwPollEvents();
            }
        }

        // Update debug markers every frame (car position changes)

        glfwPollEvents();
        drawFrame();
    }
    vkDeviceWaitIdle(device);
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

VkFormat Application::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                          VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported format!");
}

VkFormat Application::findDepthFormat() {
    return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                               VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void Application::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                              VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                              VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void Application::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage,
                depthImageMemory);

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

    if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view!");
    }
}

void Application::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Application::createWorldPipeline() {
    auto vertShaderCode = readFile("world.vert.spv");
    auto fragShaderCode = readFile("world.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input - USE VERTEX STRUCTURE
    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    // Rest same as skybox pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapchainExtent.width);
    viewport.height   = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;  // Disable culling to debug visibility
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &worldPipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create world pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = worldPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &worldPipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, worldPipelineLayout, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        throw std::runtime_error("Failed to create world graphics pipeline");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void Application::loadCarModel() {
    // NEW: Use ModelAdapter for data-driven loading
    carAdapter = new ModelAdapter();
    if (!carAdapter->load("assets/models/bmw/bmw.gltf", device, physicalDevice, commandPool, graphicsQueue)) {
        throw std::runtime_error("Failed to load car model via adapter");
    }
    carModelPtr = carAdapter->getModel();

    // Get hierarchy-aware dimensions for accurate scaling
    glm::vec3 hMin, hMax;
    carModelPtr->getHierarchyBounds(hMin, hMax);
    glm::vec3 hDimensions = hMax - hMin;

              << std::endl;

    // Use target length from adapter
    float targetLength = carAdapter->getTargetLength();
    if (targetLength <= 0.0f) {
        targetLength = 4.7f;  // Fallback
    }

    // Calculate scale factor based on length (Z dimension)
    carScaleFactor = targetLength / hDimensions.z;

    // Cache the bottom offset (lowest point in model space * scale)
    carBottomOffset = hMin.y * carScaleFactor;


    // Use cockpit offset from adapter
    cockpitOffset = carAdapter->getCockpitOffset();
    if (cockpitOffset == glm::vec3(0.0f)) {
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

    if (tagRole(CarEntity::ROLE_STEERING_WHEEL)) {
        carParts.hasSteeringWheel = true;
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
    }

    sceneManager.setActiveScene("driving");
}

void Application::loadRoadModel() {

    roadAdapter = new ModelAdapter();
    if (!roadAdapter->load("assets/models/road.glb", device, physicalDevice, commandPool, graphicsQueue)) {
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

VkPipeline Application::createBasePipeline(const PipelineConfig& config) {
    auto vertShaderCode = readFile(config.vertShader);
    auto fragShaderCode = readFile(config.fragShader);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = config.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapchainExtent.width);
    viewport.height   = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = config.lineWidth;
    rasterizer.cullMode                = config.cullMode;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = config.enableDepthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    if (config.enableBlending) {
        colorBlendAttachment.blendEnable         = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    } else {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.logicOp         = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = config.layout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    return pipeline;
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

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &carDescriptorSetLayout) != VK_SUCCESS) {
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

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &carPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create car pipeline layout");
    }

    PipelineConfig config;
    config.vertShader = "car.vert.spv";
    config.fragShader = "car.frag.spv";
    config.layout     = carPipelineLayout;
    config.cullMode   = VK_CULL_MODE_NONE;

    carPipeline = createBasePipeline(config);
}

void Application::createCarTransparentPipeline() {
    PipelineConfig config;
    config.vertShader       = "car.vert.spv";
    config.fragShader       = "car.frag.spv";
    config.layout           = carPipelineLayout;  // Reuse existing layout
    config.cullMode         = VK_CULL_MODE_NONE;
    config.enableBlending   = true;
    config.enableDepthWrite = false;

    carTransparentPipeline = createBasePipeline(config);
}

}

void Application::createCarDescriptorSets() {
    const auto& materials = carModelPtr->getMaterials();
    if (materials.empty()) {
        return;
    }

    // Create descriptor pool for ALL materials (road + car) * frames
    // We need to account for materials already created (road = 1) + car materials
    uint32_t totalMaterials = 1 + static_cast<uint32_t>(materials.size());  // road + car
    totalMaterials += 50;  // Add buffer for safety

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = totalMaterials * MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = totalMaterials * MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &carDescriptorPool) != VK_SUCCESS) {
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

    // Update car position (move along X axis, which is forward)
    carPosition.z += carVelocity * deltaTime;

    // Update car position (move along X axis, which is forward)
    carPosition.z += carVelocity * deltaTime;

    // NEW: Update scene entity transform
    if (playerCar) {
        // Apply vertical offset relative to the rotation correction
        // The car model has an internal offset that we dynamically calculated
        glm::vec3 visualPosition = carPosition;
        visualPosition.y -= carBottomOffset;

        playerCar->setPosition(visualPosition);

        // Combine rotations: 270° X-axis (model orientation fix) + Y-axis (steering)
        // Combine rotations: Model orientation fix + Y-axis (steering)
        glm::quat fixRotation      = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
        glm::quat yRotation        = glm::angleAxis(glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat combinedRotation = yRotation * fixRotation;  // Apply fix rotation first, then steering

        playerCar->setRotation(combinedRotation);
        playerCar->setScale(glm::vec3(carScaleFactor));

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
    if (carParts.hasSteeringWheel) {
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
            playerCar->animateRotation("steering_wheel", steeringRot);
        }
    }

    // Update scene manager
    sceneManager.update(deltaTime);
}

void Application::updateCameraForCockpit() {
    // Use hard-coded cockpit offset defined in header
    // This provides a reasonable default driver's eye position

    // Apply the same rotations to the cockpit offset as we do to the car model
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    // First rotate by car's Y-axis rotation (steering)
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
    // Then apply the 90° X-axis rotation (model orientation fix)
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(cockpitOffset, 0.0f));

    // Chase Camera (Behind and Above)
    // Position camera 6 meters behind (-Z) and 2.5 meters above the car
    // This ensures we see the exterior and aren't clipped inside
    glm::vec3 chasePosition = carPosition + glm::vec3(0.0f, 2.5f, -6.0f);

    camera.setPosition(chasePosition);

    // Face the car (looking +Z)
    camera.setYaw(90.0f);
    camera.setPitch(-15.0f);  // Look down slightly at the car

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
