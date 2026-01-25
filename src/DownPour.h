#pragma once

#define GLFW_INCLUDE_VULKAN
#include "core/VulkanContext.h"
#include "renderer/Camera.h"
#include "renderer/Material.h"
#include "renderer/ModelAdapter.h"
#include "renderer/Vertex.h"
#include "scene/CarEntity.h"
#include "scene/Entity.h"
#include "scene/RoadEntity.h"
#include "scene/Scene.h"
#include "scene/SceneBuilder.h"
#include "scene/SceneManager.h"
#include "simulation/WeatherSystem.h"
#include "simulation/WindshieldSurface.h"
#include "vulkan/VulkanTypes.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace DownPour {

/**
 * @brief Uniform Buffer Object structure for camera matrices
 *
 * This structure holds the view, projection, and combined
 * view-projection matrices for use in shaders.
 */

struct CameraUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 viewProj;
};

/**
 * @brief Main application class for the DownPour rain simulator
 *
 * This class manages the lifecycle of the application including:
 * - Window creation and management
 * - Vulkan initialization and setup
 * - Main rendering loop
 * - Cleanup and resource deallocation
 */
class Application {
public:
    /**
     * @brief Run the application
     *
     * Initializes the window and Vulkan, runs the main loop,
     * and performs cleanup on exit.
     */
    void run();

private:
    // Window properties
    static constexpr uint32_t WIDTH  = 800;
    static constexpr uint32_t HEIGHT = 600;

    // Camera and input tracking
    Camera camera;
    float  lastFrameTime;
    float  lastX          = WIDTH / 2.0f;
    float  lastY          = HEIGHT / 2.0f;
    bool   firstMouse     = true;
    bool   cursorCaptured = true;

    // Car driving state
    glm::vec3 carPosition     = glm::vec3(0.0f, 2.0f, 2.0f);
    float     carVelocity     = 0.0f;
    float     carRotation     = -90.0f;
    float     carScaleFactor  = 1.0f;
    float     carBottomOffset = 0.0f;  // Dynamically calculated vertical offset

    /**
     * @brief Index ranges for specific car parts that need animation
     */
    /**
     * @brief Essential car parts tracking for animation
     */
    struct CarParts {
        bool hasSteeringWheel = false;
        bool hasWipers        = false;
    };

    CarParts carParts;
    float    steeringWheelRotation = 0.0f;  // Current steering wheel rotation in degrees

    // Simplified cockpit camera - hard-coded offset for initial implementation
    // Note: These values are in the MODEL'S local space BEFORE the 90° X rotation
    glm::vec3 cockpitOffset = glm::vec3(0.0f, -0.21f, -0.18f);  // X=0(center), Y=forward(neg), Z=up

    // Debug visualization (simplified)
    bool           debugVisualizationEnabled = true;  // Toggle with 'V' key
    VkBuffer       debugVertexBuffer         = VK_NULL_HANDLE;
    VkDeviceMemory debugVertexBufferMemory   = VK_NULL_HANDLE;
    uint32_t       debugVertexCount          = 0;

    // Weather simulation system
    Simulation::WeatherSystem     weatherSystem;
    Simulation::WindshieldSurface windshield;

    // Vulkan context (manages instance, device, surface, queues)
    VulkanContext vulkanContext;

    // GLFW window
    GLFWwindow* window = nullptr;

    // Vulkan handles still managed by Application
    VkCommandPool                commandPool    = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkPipelineLayout             pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline                   graphicsPipeline    = VK_NULL_HANDLE;
    VkDescriptorSetLayout        descriptorSetLayout = VK_NULL_HANDLE;

    // Swap chain
    VkSwapchainKHR             swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>       swapchainImages;
    VkFormat                   swapchainImageFormat;
    VkExtent2D                 swapchainExtent;
    std::vector<VkImageView>   swapchainImageViews;
    VkRenderPass               renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    // Depth resources
    VkImage        depthImage       = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView    depthImageView   = VK_NULL_HANDLE;

    // Pipelines
    VkPipeline       worldPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout worldPipelineLayout = VK_NULL_HANDLE;

    // Scene system (NEW)
    SceneManager sceneManager;
    CarEntity*   playerCar = nullptr;

    // Models (Managed by Adapters)
    ModelAdapter* carAdapter  = nullptr;
    ModelAdapter* roadAdapter = nullptr;

    // Legacy pointers for systems still expecting raw Model*
    Model* carModelPtr  = nullptr;
    Model* roadModelPtr = nullptr;

    // Material management
    MaterialManager*                     materialManager = nullptr;
    std::unordered_map<size_t, uint32_t> carMaterialIds;
    std::unordered_map<size_t, uint32_t> roadMaterialIds;

    VkPipeline            carPipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      carPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout carDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      carDescriptorPool      = VK_NULL_HANDLE;

    // Transparent car pipeline (same layout as carPipeline)
    VkPipeline carTransparentPipeline = VK_NULL_HANDLE;

    // Windshield rendering
    VkPipeline            windshieldPipeline         = VK_NULL_HANDLE;
    VkPipelineLayout      windshieldPipelineLayout   = VK_NULL_HANDLE;
    VkDescriptorSetLayout windshieldDescriptorLayout = VK_NULL_HANDLE;

    // Initialization methods
    void           initWindow();
    void           initVulkan();
    void           createSwapChain();
    void           createRenderPass();
    void           createGraphicsPipeline();
    void           createFramebuffers();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void           recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t frameIndex);
    void           createCommandBuffers();
    void           createImageViews();
    void           createCommandPool();
    void           createSyncObjects();
    void           drawFrame();
    void           createDescriptorSetLayout();
    void           createUniformBuffers();
    void           updateUniformBuffer(uint32_t currentImage);

    // Main loop and cleanup
    void mainLoop();
    void cleanup();

    // Helper methods
    Vulkan::QueueFamilyIndices      findQueueFamilies(VkPhysicalDevice device);
    Vulkan::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR              chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR                chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D                      chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    static void                     mouseCallback(GLFWwindow* window, double xpos, double ypos);

    // Buffer Helper methods
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
                      VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // frame management
    static constexpr int     MAX_FRAMES_IN_FLIGHT = 2;
    size_t                   currentFrame         = 0;
    std::vector<VkFence>     inFlightFences;  // Update from single fence
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    // Uniform buffers
    std::vector<VkBuffer>       uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*>          uniformBuffersMapped;

    // Descriptor sets
    VkDescriptorPool             descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    void createDescriptorPool();
    void createDescriptorSets();

    // Depth resources
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                 VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void createDepthResources();

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createRoadBuffers();

    void createWorldPipeline();

    // Car rendering methods
    void loadCarModel();
    void createCarPipeline();
    void createCarTransparentPipeline();
    void createCarDescriptorSets();

    // Road rendering methods
    void loadRoadModel();

    // Windshield rendering methods
    void createWindshieldPipeline();
    void renderWindshield(VkCommandBuffer cmd, uint32_t frameIndex);

    // Car simulation methods
    void updateCarPhysics(float deltaTime);
    void updateCameraForCockpit();

    /**
     * @brief Configuration for Vulkan pipeline creation
     */
    struct PipelineConfig {
        std::string                        vertShader;
        std::string                        fragShader;
        VkPipelineLayout                   layout           = VK_NULL_HANDLE;
        bool                               enableBlending   = false;
        bool                               enableDepthWrite = true;
        VkCullModeFlags                    cullMode         = VK_CULL_MODE_BACK_BIT;
        VkPrimitiveTopology                topology         = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        float                              lineWidth        = 1.0f;
        std::vector<VkDescriptorSetLayout> descriptorLayouts;
    };

    /**
     * @brief Centralized helper to create a Vulkan graphics pipeline
     */
    VkPipeline createBasePipeline(const PipelineConfig& config);

    /**
     * @brief Template helper to safely destroy Vulkan objects with null checks
     */
    template <typename T, typename D>
    void safeDestroy(T& handle, D destroyFunc) {
        if (handle != VK_NULL_HANDLE) {
            destroyFunc(vulkanContext.getDevice(), handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }
};

}  // namespace DownPour
