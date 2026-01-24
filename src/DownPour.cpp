#include "DownPour.h"

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
        if (!file.is_open()) {
            continue;
        }
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

    // Initialize debug visualization
    createDebugPipeline();
    createDebugMarkers();

    createSyncObjects();

    float aspect = static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
    // Initialize camera (will be updated by updateCamera based on camera mode)
    camera = Camera({0.0f, 0.0f, 0.0f}, aspect);
    // Set FOV (wider for cockpit, normal for external)
    camera.setFOV(75.0f);
    // Set far plane to 10km so we can see the entire 6.5km road
    camera.setFarPlane(10000.0f);
    lastFrameTime = glfwGetTime();

    // Set initial camera to cockpit view
    updateCameraForCockpit();


}

void Application::cleanup() {
    // Clean up debug visualization resources
    if (debugVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, debugVertexBuffer, nullptr);
    }
    if (debugVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, debugVertexBufferMemory, nullptr);
    }
    if (debugPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, debugPipeline, nullptr);
    }
    if (debugPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, debugPipelineLayout, nullptr);
    }

    // Clean up windshield resources
    windshield.cleanup(device);
    if (windshieldPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, windshieldPipeline, nullptr);
    }
    if (windshieldPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, windshieldPipelineLayout, nullptr);
    }
    if (windshieldDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, windshieldDescriptorLayout, nullptr);
    }

    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }

    // Clean up sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

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
    carModel.cleanup(device);
    if (carPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, carPipeline, nullptr);
    }
    if (carTransparentPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, carTransparentPipeline, nullptr);
    }
    if (carPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, carPipelineLayout, nullptr);
    }
    if (carDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, carDescriptorSetLayout, nullptr);
    }
    if (carDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, carDescriptorPool, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    // Clean up road model BEFORE destroying device
    roadModel.cleanup(device);
    if (worldPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, worldPipeline, nullptr);
    }
    if (worldPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, worldPipelineLayout, nullptr);
    }

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
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

VkShaderModule Application::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
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
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
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
    std::cout << "Enabled extensions:" << std::endl;
    for (const auto& extension : extensions) {
        std::cout << "  " << extension << std::endl;
    }

    // Create Vulkan instance
    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = 0;
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

Vulkan::QueueFamilyIndices Application::findQueueFamilies(VkPhysicalDevice device) {
    Vulkan::QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

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

    if (vkCreateCommandPool(device, &info, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void Application::createCommandBuffers() {
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
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
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void Application::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

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

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        Vulkan::QueueFamilyIndices indices = findQueueFamilies(device);
        if (indices.isComplete()) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
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
    for (const auto& extension : availableExtensions) {
        if (strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
            portabilitySubsetAvailable = true;
            break;
        }
    }

    if (portabilitySubsetAvailable) {
        deviceExtensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

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
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Application::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
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
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

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

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

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

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
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
        if (vkCreateImageView(device, &view, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
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

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
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

    // 2. Draw road (model-based)
    if (roadModel.getIndexCount() > 0) {
        const auto&  roadMaterials       = roadModel.getMaterials();
        VkBuffer     roadVertexBuffers[] = {roadModel.getVertexBuffer()};
        VkDeviceSize roadOffsets[]       = {0};

        if (!roadMaterials.empty()) {
            // Road has textures - use car pipeline with MaterialManager
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipeline);

            // Push road model matrix
            glm::mat4 roadMatrix = roadModel.getModelMatrix();
            vkCmdPushConstants(cmd, carPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &roadMatrix);

            // Bind vertex and index buffers
            vkCmdBindVertexBuffers(cmd, 0, 1, roadVertexBuffers, roadOffsets);
            vkCmdBindIndexBuffer(cmd, roadModel.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            // Render each material
            for (size_t i = 0; i < roadMaterials.size(); i++) {
                const auto& material = roadMaterials[i];

                // Get descriptor set from MaterialManager
                uint32_t        gpuId         = roadMaterialIds[i];
                VkDescriptorSet matDescriptor = materialManager->getDescriptorSet(gpuId, frameIndex);

                // Bind descriptor sets
                std::vector<VkDescriptorSet> sets = {descriptorSets[frameIndex], matDescriptor};
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipelineLayout, 0,
                                        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

                // Draw this material's index range
                vkCmdDrawIndexed(cmd, material.indexCount, 1, material.indexStart, 0, 0);
            }
        } else {
            // Road has no materials - use simple world pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, worldPipelineLayout, 0, 1,
                                    &descriptorSets[frameIndex], 0, nullptr);
            vkCmdBindVertexBuffers(cmd, 0, 1, roadVertexBuffers, roadOffsets);
            vkCmdBindIndexBuffer(cmd, roadModel.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, roadModel.getIndexCount(), 1, 0, 0, 0);
        }
    }

    // 3. Draw car - TWO PASSES: opaque first, then transparent
    if (carModel.getIndexCount() > 0) {
        const auto& materials = carModel.getMaterials();

        if (!materials.empty()) {
            // Bind vertex and index buffers ONCE (same for all materials)
            VkBuffer     carVertexBuffers[] = {carModel.getVertexBuffer()};
            VkDeviceSize carOffsets[]       = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, carVertexBuffers, carOffsets);
            vkCmdBindIndexBuffer(cmd, carModel.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            // Push model matrix ONCE (same for all materials)
            glm::mat4 modelMatrix = carModel.getModelMatrix();
            vkCmdPushConstants(cmd, carPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMatrix);

            // PASS 1: Draw OPAQUE materials
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipeline);

            for (size_t matIdx = 0; matIdx < materials.size(); matIdx++) {
                const auto& material = materials[matIdx];

                // Skip transparent materials in this pass
                if (material.props.isTransparent)
                    continue;

                // Get descriptor set from MaterialManager
                uint32_t        gpuId         = carMaterialIds[matIdx];
                VkDescriptorSet matDescriptor = materialManager->getDescriptorSet(gpuId, frameIndex);

                // Bind descriptor sets for this material
                std::vector<VkDescriptorSet> sets = {descriptorSets[frameIndex], matDescriptor};
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipelineLayout, 0,
                                        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

                // Draw this material's index range
                vkCmdDrawIndexed(cmd, material.indexCount, 1, material.indexStart, 0, 0);
            }

            // PASS 2: Draw TRANSPARENT materials
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carTransparentPipeline);

            for (size_t matIdx = 0; matIdx < materials.size(); matIdx++) {
                const auto& material = materials[matIdx];

                // Only draw transparent materials in this pass
                if (!material.props.isTransparent)
                    continue;

                // Get descriptor set from MaterialManager
                uint32_t        gpuId         = carMaterialIds[matIdx];
                VkDescriptorSet matDescriptor = materialManager->getDescriptorSet(gpuId, frameIndex);

                // Bind descriptor sets for this material
                std::vector<VkDescriptorSet> sets = {descriptorSets[frameIndex], matDescriptor};
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipelineLayout, 0,
                                        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

                // Draw this material's index range
                vkCmdDrawIndexed(cmd, material.indexCount, 1, material.indexStart, 0, 0);
            }
        }
    }

    // 4. Draw debug markers (if enabled)
    if (debugVisualizationEnabled && debugVertexCount > 0) {
        renderDebugMarkers(cmd, frameIndex);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
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

        // Update car physics and camera
        updateCarPhysics(deltaTime);
        updateCameraForCockpit();

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
            std::cout << "Debug visualization: " << (debugVisualizationEnabled ? "ON" : "OFF") << std::endl;
            // Small delay to prevent multiple toggles
            while (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
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
            std::cout << "Cockpit offset: (" << cockpitOffset.x << ", " << cockpitOffset.y << ", " << cockpitOffset.z
                      << ")" << std::endl;
        }

        // Update debug markers every frame (car position changes)
        if (debugVisualizationEnabled) {
            updateDebugMarkers();
        }

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
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;  // Enable backface culling
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
    carModel.loadFromFile("assets/models/bmw.glb", device, physicalDevice, commandPool, graphicsQueue);

    // Get UNSCALED model dimensions
    glm::vec3 dimensions = carModel.getDimensions();

    std::cout << "\n=== CAR SCALE CALCULATION ===" << std::endl;
    std::cout << "Original model dimensions: " << dimensions.x << " x " << dimensions.y << " x " << dimensions.z
              << std::endl;

    // BMW M3 target dimensions in meters (1 unit = 1 meter)
    const float targetLength = 4.7f;  // meters

    // Calculate scale factor based on length (Z dimension)
    carScaleFactor = targetLength / dimensions.z;

    // Print bounding box info for debugging
    glm::vec3 minB = carModel.getMinBounds();
    glm::vec3 maxB = carModel.getMaxBounds();

    // Calculate a better starting cockpit position
    // Driver position is typically:
    // - X: slightly left of center (but we'll start centered)
    // - Y: about 60% forward from rear (in model space before rotation)
    // - Z: about 80% of height (eye level)

    glm::vec3 dims = carModel.getDimensions();
    glm::vec3 min  = carModel.getMinBounds();

    // In model space (before 90° rotation):
    // Y axis is what becomes "forward" in world space after rotation
    // Z axis is what becomes "up" in world space after rotation

    float suggestedX = 0.0f;                    // Center
    float suggestedY = min.y + dims.y * 0.4f;   // 40% from front (60% from back)
    float suggestedZ = min.z + dims.z * 0.75f;  // 75% of height for eye level

    // Query and store car part meshes for animation
    std::cout << "\n=== SEARCHING FOR CAR PARTS ===" << std::endl;

    auto meshNames = carModel.getMeshNames();
    std::cout << "Available meshes in car model: " << meshNames.size() << std::endl;
    for (const auto& name : meshNames) {
        std::cout << "  - " << name << std::endl;
    }

    // Search for steering wheel parts (try exact match, then with primitive suffix)
    if (carModel.getMeshIndexRange("steering_wheel_front", carParts.steeringWheelFrontStart,
                                   carParts.steeringWheelFrontCount)) {
        std::cout << "Found steering_wheel_front" << std::endl;
        carParts.hasSteeringWheel = true;
    } else if (carModel.getMeshIndexRange("steering_wheel_front_primitive0", carParts.steeringWheelFrontStart,
                                          carParts.steeringWheelFrontCount)) {
        std::cout << "Found steering_wheel_front_primitive0" << std::endl;
        carParts.hasSteeringWheel = true;
    }

    if (carModel.getMeshIndexRange("steering_wheel_back", carParts.steeringWheelBackStart,
                                   carParts.steeringWheelBackCount)) {
        std::cout << "Found steering_wheel_back" << std::endl;
        carParts.hasSteeringWheel = true;
    } else if (carModel.getMeshIndexRange("steering_wheel_back_primitive0", carParts.steeringWheelBackStart,
                                          carParts.steeringWheelBackCount)) {
        std::cout << "Found steering_wheel_back_primitive0" << std::endl;
        carParts.hasSteeringWheel = true;
    }

    // Search for wipers
    if (carModel.getMeshIndexRange("left_wiper", carParts.leftWiperStart, carParts.leftWiperCount)) {
        std::cout << "Found left_wiper" << std::endl;
        carParts.hasWipers = true;
    }

    if (carModel.getMeshIndexRange("right_wiper", carParts.rightWiperStart, carParts.rightWiperCount)) {
        std::cout << "Found right_wiper" << std::endl;
        carParts.hasWipers = true;
    }

    std::cout << "Steering wheel found: " << (carParts.hasSteeringWheel ? "YES" : "NO") << std::endl;
    std::cout << "Wipers found: " << (carParts.hasWipers ? "YES" : "NO") << std::endl;
    std::cout << "================================\n" << std::endl;

    // Load GPU resources for car materials
    const auto& carMaterials = carModel.getMaterials();
    std::cout << "\n=== LOADING CAR MATERIALS ===" << std::endl;
    for (size_t i = 0; i < carMaterials.size(); i++) {
        uint32_t gpuId    = materialManager->createMaterial(carMaterials[i]);
        carMaterialIds[i] = gpuId;
    }
    std::cout << "Loaded " << carMaterials.size() << " car materials" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void Application::loadRoadModel() {
    roadModel.loadFromFile("assets/models/road.glb", device, physicalDevice, commandPool, graphicsQueue);

    // Position road at ground level (Y=0)
    glm::mat4 roadTransform = glm::mat4(1.0f);
    roadTransform           = glm::translate(roadTransform, glm::vec3(0.0f, 0.0f, 0.0f));

    // May need scaling depending on road.glb dimensions
    // For now, assume road.glb is already at correct scale
    roadModel.setModelMatrix(roadTransform);

    // Load GPU resources for road materials
    const auto& roadMaterials = roadModel.getMaterials();
    std::cout << "\n=== LOADING ROAD MATERIALS ===" << std::endl;
    for (size_t i = 0; i < roadMaterials.size(); i++) {
        uint32_t gpuId     = materialManager->createMaterial(roadMaterials[i]);
        roadMaterialIds[i] = gpuId;
    }
    std::cout << "Loaded " << roadMaterials.size() << " road materials" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void Application::createCarPipeline() {
    auto vertShaderCode = readFile("car.vert.spv");
    auto fragShaderCode = readFile("car.frag.spv");

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

    // Vertex input
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
    rasterizer.cullMode                = VK_CULL_MODE_NONE;  // Disable culling to fix rendering issues
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
    pipelineInfo.layout              = carPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &carPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create car graphics pipeline");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void Application::createCarTransparentPipeline() {
    auto vertShaderCode = readFile("car.vert.spv");
    auto fragShaderCode = readFile("car.frag.spv");

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
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // CRITICAL: Configure depth for transparent objects
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_FALSE;  // DON'T write to depth buffer
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    // CRITICAL: Enable alpha blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
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
    pipelineInfo.layout              = carPipelineLayout;  // Reuse existing layout
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &carTransparentPipeline) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create transparent car graphics pipeline");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    std::cout << "Created transparent car pipeline" << std::endl;
}

void Application::createCarDescriptorSets() {
    const auto& materials = carModel.getMaterials();
    if (materials.empty()) {
        std::cout << "No car materials to create descriptor sets for" << std::endl;
        return;
    }

    // Create descriptor pool for car textures (need sets for each material * frames)
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = static_cast<uint32_t>(materials.size() * MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(materials.size() * MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &carDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create car descriptor pool");
    }

    // Initialize MaterialManager descriptor support
    // MaterialManager will create and manage descriptor sets for materials
    materialManager->initDescriptorSupport(carDescriptorSetLayout, carDescriptorPool, MAX_FRAMES_IN_FLIGHT);

    std::cout << "Car descriptor pool and MaterialManager initialized" << std::endl;
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

    // Update car model matrix
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix           = glm::translate(modelMatrix, carPosition);
    modelMatrix           = glm::rotate(modelMatrix, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
    // Apply 90° X-axis rotation to fix model orientation (was facing up)
    modelMatrix = glm::rotate(modelMatrix, glm::radians(270.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // Apply calculated scale for realistic size
    modelMatrix = glm::scale(modelMatrix, glm::vec3(carScaleFactor, carScaleFactor, carScaleFactor));
    carModel.setModelMatrix(modelMatrix);

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
    }
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

    // Update camera position to be inside the car at cockpit location
    glm::vec3 cockpitPosition =
        carPosition + rotatedOffset +
        glm::vec3(0.0f, 0.4f, -0.15f);  // x is forward and backward, y is up and down, z is left and right
    camera.setPosition(cockpitPosition);

    // Debug: Print camera AND car position (only once at startup, not every frame)
    static bool printedOnce = false;
    if (!printedOnce) {
        std::cout << "\n=== CAMERA & CAR DEBUG ===" << std::endl;
        std::cout << "Car world position: (" << carPosition.x << ", " << carPosition.y << ", " << carPosition.z << ")"
                  << std::endl;
        std::cout << "Car rotation (Y-axis): " << carRotation << " degrees" << std::endl;
        std::cout << "Cockpit offset (before rotation): (" << cockpitOffset.x << ", " << cockpitOffset.y << ", "
                  << cockpitOffset.z << ")" << std::endl;
        std::cout << "Rotated offset: (" << rotatedOffset.x << ", " << rotatedOffset.y << ", " << rotatedOffset.z << ")"
                  << std::endl;
        std::cout << "Final camera position: (" << cockpitPosition.x << ", " << cockpitPosition.y << ", "
                  << cockpitPosition.z << ")" << std::endl;
        std::cout << "Camera is " << (cockpitPosition.y >= 0 ? "ABOVE" : "BELOW") << " ground (Y=" << cockpitPosition.y
                  << ")" << std::endl;
        std::cout << "===========================\n" << std::endl;
        printedOnce = true;
    }

    // Set camera to look forward (pitch = 0 is looking straight ahead)
    // The 90° model rotation is already accounted for in the cockpit offset transformation
    camera.setPitch(0.0f);  // Look straight forward
}

void Application::createWindshieldPipeline() {
    // TODO: Implement windshield pipeline creation
    // This will be a simple quad rendering pipeline with the windshield shader
    std::cout << "Windshield pipeline creation placeholder" << std::endl;
}

void Application::renderWindshield(VkCommandBuffer cmd, uint32_t frameIndex) {
    // TODO: Implement windshield rendering
    // This will render a full-screen quad with the windshield effect
    (void)cmd;
    (void)frameIndex;
}

// ===== Debug Visualization Methods =====

void Application::createDebugPipeline() {
    // Load debug shaders
    auto vertShaderCode = readFile("debug.vert.spv");
    auto fragShaderCode = readFile("debug.frag.spv");

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

    // Vertex input: position (vec3) + color (vec3)
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(float) * 6;  // 3 floats for position + 3 for color
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    // Position
    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = 0;
    // Color
    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset   = sizeof(float) * 3;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;  // Draw lines
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
    rasterizer.lineWidth               = 2.0f;  // Thick lines for visibility
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_FALSE;                     // Don't write to depth buffer
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;  // Draw even if equal
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // Use the same descriptor set layout as other pipelines (camera UBO)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &debugPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug pipeline layout!");
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
    pipelineInfo.layout              = debugPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &debugPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    std::cout << "Debug visualization pipeline created" << std::endl;
}

void Application::createDebugMarkers() {
    // Initial buffer creation with placeholder data
    // Will be updated in updateDebugMarkers()
    const size_t maxVertices = 1000;                             // Plenty for our debug markers
    VkDeviceSize bufferSize  = sizeof(float) * 6 * maxVertices;  // 6 floats per vertex (pos + color)

    createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, debugVertexBuffer,
                 debugVertexBufferMemory);

    std::cout << "Debug marker buffer created" << std::endl;
}

void Application::updateDebugMarkers() {
    // Build debug visualization data
    struct DebugVertex {
        float x, y, z;  // Position
        float r, g, b;  // Color
    };

    std::vector<DebugVertex> vertices;

    // Get car dimensions in world space
    glm::vec3 carDimensions = carModel.getDimensions() * carScaleFactor;

    // Calculate rotation matrix (same as car model)
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix           = glm::rotate(rotationMatrix, glm::radians(carRotation), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix           = glm::rotate(rotationMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // 1. Car origin (0,0,0 in model space) - WHITE cross
    auto addLine = [&](glm::vec3 start, glm::vec3 end, glm::vec3 color) {
        vertices.push_back({start.x, start.y, start.z, color.r, color.g, color.b});
        vertices.push_back({end.x, end.y, end.z, color.r, color.g, color.b});
    };

    // Car origin axes (small, 0.5m each direction)
    glm::vec3 origin = carPosition;
    glm::vec3 xAxis  = glm::vec3(rotationMatrix * glm::vec4(0.5f, 0.0f, 0.0f, 0.0f));
    glm::vec3 yAxis  = glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.5f, 0.0f, 0.0f));
    glm::vec3 zAxis  = glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, 0.5f, 0.0f));

    addLine(origin, origin + xAxis, glm::vec3(1.0f, 0.0f, 0.0f));  // X = RED
    addLine(origin, origin + yAxis, glm::vec3(0.0f, 1.0f, 0.0f));  // Y = GREEN
    addLine(origin, origin + zAxis, glm::vec3(0.0f, 0.0f, 1.0f));  // Z = BLUE

    // 2. Bounding box corners - YELLOW
    glm::vec3 bbMin = carModel.getMinBounds() * carScaleFactor;
    glm::vec3 bbMax = carModel.getMaxBounds() * carScaleFactor;

    // Transform bounding box corners to world space
    auto transformPoint = [&](glm::vec3 localPoint) -> glm::vec3 {
        glm::vec3 rotated = glm::vec3(rotationMatrix * glm::vec4(localPoint, 0.0f));
        return carPosition + rotated;
    };

    glm::vec3 corners[8] = {
        transformPoint(glm::vec3(bbMin.x, bbMin.y, bbMin.z)), transformPoint(glm::vec3(bbMax.x, bbMin.y, bbMin.z)),
        transformPoint(glm::vec3(bbMax.x, bbMax.y, bbMin.z)), transformPoint(glm::vec3(bbMin.x, bbMax.y, bbMin.z)),
        transformPoint(glm::vec3(bbMin.x, bbMin.y, bbMax.z)), transformPoint(glm::vec3(bbMax.x, bbMin.y, bbMax.z)),
        transformPoint(glm::vec3(bbMax.x, bbMax.y, bbMax.z)), transformPoint(glm::vec3(bbMin.x, bbMax.y, bbMax.z))};

    glm::vec3 yellow(1.0f, 1.0f, 0.0f);
    // Bottom face
    addLine(corners[0], corners[1], yellow);
    addLine(corners[1], corners[2], yellow);
    addLine(corners[2], corners[3], yellow);
    addLine(corners[3], corners[0], yellow);
    // Top face
    addLine(corners[4], corners[5], yellow);
    addLine(corners[5], corners[6], yellow);
    addLine(corners[6], corners[7], yellow);
    addLine(corners[7], corners[4], yellow);
    // Vertical edges
    addLine(corners[0], corners[4], yellow);
    addLine(corners[1], corners[5], yellow);
    addLine(corners[2], corners[6], yellow);
    addLine(corners[3], corners[7], yellow);

    // 3. Cockpit offset position - CYAN
    glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(cockpitOffset, 0.0f));
    glm::vec3 cockpitPos    = carPosition + rotatedOffset + glm::vec3(0.0f, 0.4f, 0.0f);  // Same as camera calc

    // Draw a cross at cockpit position
    glm::vec3 cyan(0.0f, 1.0f, 1.0f);
    addLine(cockpitPos + glm::vec3(-0.2f, 0.0f, 0.0f), cockpitPos + glm::vec3(0.2f, 0.0f, 0.0f), cyan);
    addLine(cockpitPos + glm::vec3(0.0f, -0.2f, 0.0f), cockpitPos + glm::vec3(0.0f, 0.2f, 0.0f), cyan);
    addLine(cockpitPos + glm::vec3(0.0f, 0.0f, -0.2f), cockpitPos + glm::vec3(0.0f, 0.0f, 0.2f), cyan);

    // 4. Line from car origin to cockpit - MAGENTA
    glm::vec3 magenta(1.0f, 0.0f, 1.0f);
    addLine(origin, cockpitPos, magenta);

    // 5. Car world position marker at ground level - BRIGHT GREEN (shows where car is on ground)
    glm::vec3 carGroundPos(carPosition.x, 0.0f, carPosition.z);  // Project to ground
    glm::vec3 brightGreen(0.0f, 1.0f, 0.0f);
    // Draw a bright cross at the car's ground position
    addLine(carGroundPos + glm::vec3(-0.5f, 0.0f, 0.0f), carGroundPos + glm::vec3(0.5f, 0.0f, 0.0f), brightGreen);
    addLine(carGroundPos + glm::vec3(0.0f, 0.0f, -0.5f), carGroundPos + glm::vec3(0.0f, 0.0f, 0.5f), brightGreen);
    // Vertical line from ground to car origin
    addLine(carGroundPos, carPosition, brightGreen);

    // 6. Ground reference grid at car position (helps see elevation) - DARK GREY
    glm::vec3 darkGrey(0.4f, 0.4f, 0.4f);
    for (int i = -2; i <= 2; i++) {
        glm::vec3 start(carPosition.x + i * 1.0f, 0.0f, carPosition.z - 2.0f);
        glm::vec3 end(carPosition.x + i * 1.0f, 0.0f, carPosition.z + 2.0f);
        addLine(start, end, darkGrey);

        start = glm::vec3(carPosition.x - 2.0f, 0.0f, carPosition.z + i * 1.0f);
        end   = glm::vec3(carPosition.x + 2.0f, 0.0f, carPosition.z + i * 1.0f);
        addLine(start, end, darkGrey);
    }

    // Update vertex count
    debugVertexCount = static_cast<uint32_t>(vertices.size());

    // Copy to GPU
    void* data;
    vkMapMemory(device, debugVertexBufferMemory, 0, sizeof(DebugVertex) * vertices.size(), 0, &data);
    memcpy(data, vertices.data(), sizeof(DebugVertex) * vertices.size());
    vkUnmapMemory(device, debugVertexBufferMemory);
}

void Application::renderDebugMarkers(VkCommandBuffer cmd, uint32_t frameIndex) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipelineLayout, 0, 1,
                            &descriptorSets[frameIndex], 0, nullptr);

    VkBuffer     vertexBuffers[] = {debugVertexBuffer};
    VkDeviceSize offsets[]       = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    vkCmdDraw(cmd, debugVertexCount, 1, 0, 0);
}

}  // namespace DownPour
