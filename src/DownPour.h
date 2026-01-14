#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan/VulkanTypes.h"
#include "renderer/Camera.h"
#include "renderer/Vertex.h"
#include "renderer/Model.h"
#include "simulation/WeatherSystem.h"
#include "simulation/WindshieldSurface.h"

#include <cstdint>
#include <vector>



namespace DownPour {

/**
 * @brief Uniform Buffer Object structure for camera matrices
 * 
 * This structure holds the view, projection, and combined
 * view-projection matrices for use in shaders.
 */

struct CameraUBO {
     alignas(16) glm::mat4   view;
     alignas(16) glm::mat4   proj;
     alignas(16)  glm::mat4  viewProj;
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
    static constexpr uint32_t WIDTH =  800;
    static constexpr uint32_t HEIGHT = 600;

    // Camera and input tracking
    Camera camera;
    float lastFrameTime;
    float lastX = WIDTH / 2.0f;
    float lastY = HEIGHT / 2.0f;
    bool firstMouse = true;
    bool cursorCaptured = true;

    // Car driving state
    glm::vec3 carPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    float carVelocity = 0.0f;
    float carRotation = 0.0f;
    float carScaleFactor = 1.0f;  // Calculated scale to achieve realistic size

    /**
     * @brief Index ranges for specific car parts that need animation
     */
    struct CarParts {
        uint32_t steeringWheelFrontStart = 0;
        uint32_t steeringWheelFrontCount = 0;
        uint32_t steeringWheelBackStart = 0;
        uint32_t steeringWheelBackCount = 0;

        uint32_t leftWiperStart = 0;
        uint32_t leftWiperCount = 0;
        uint32_t rightWiperStart = 0;
        uint32_t rightWiperCount = 0;

        bool hasSteeringWheel = false;
        bool hasWipers = false;
    };

    CarParts carParts;
    float steeringWheelRotation = 0.0f;  // Current steering wheel rotation in degrees

    // Simplified cockpit camera - hard-coded offset for initial implementation
    // Can be adjusted based on different car models later
    // Note: These values are in the MODEL'S local space BEFORE the 90° X rotation
    // Calculated based on car bounding box: 75% height, 40% from front
    glm::vec3 cockpitOffset = glm::vec3(0.0f, -0.21f, -0.18f);  // X=0(center), Y=forward(neg), Z=up

    // Debug visualization
    bool debugVisualizationEnabled = true;  // Toggle with 'V' key
    VkBuffer debugVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory debugVertexBufferMemory = VK_NULL_HANDLE;
    uint32_t debugVertexCount = 0;
    VkPipeline debugPipeline = VK_NULL_HANDLE;
    VkPipelineLayout debugPipelineLayout = VK_NULL_HANDLE;
    
    // Weather simulation system
    Simulation::WeatherSystem weatherSystem;
    Simulation::WindshieldSurface windshield;

    // GLFW and Vulkan handles
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    // Queue handles
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    
    // Swap chain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImageView> swapchainImageViews;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    // Depth resources
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // OLD: Procedural road buffers - no longer needed (now using Model)
    // VkBuffer roadVertexBuffer = VK_NULL_HANDLE;
    // VkDeviceMemory roadVertexBufferMemory = VK_NULL_HANDLE;
    // VkBuffer roadIndexBuffer = VK_NULL_HANDLE;
    // VkDeviceMemory roadIndexBufferMemory = VK_NULL_HANDLE;
    // uint32_t roadIndexCount = 0;
    // World pipeline
    VkPipeline worldPipeline = VK_NULL_HANDLE;
    VkPipelineLayout worldPipelineLayout = VK_NULL_HANDLE;

    // Car model
    Model carModel;
    // Road model
    Model roadModel;
    VkPipeline carPipeline = VK_NULL_HANDLE;
    VkPipelineLayout carPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout carDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool carDescriptorPool = VK_NULL_HANDLE;
    // 2D vector: [materialIndex][frameIndex]
    std::vector<std::vector<VkDescriptorSet>> carDescriptorSets;

    // Windshield rendering
    VkPipeline windshieldPipeline = VK_NULL_HANDLE;
    VkPipelineLayout windshieldPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout windshieldDescriptorLayout = VK_NULL_HANDLE;

    // Initialization methods
    void initWindow();
    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t frameIndex);
    void createCommandBuffers(); // DONE 
    void createImageViews(); // DONE
    void createCommandPool(); // DONE
    void createSyncObjects(); // DONE
    void drawFrame(); // DONE
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);

    // Main loop and cleanup
    void mainLoop();
    void cleanup();
    
    // Helper methods
    Vulkan::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    Vulkan::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);


    // Buffer Helper methods
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
         VkMemoryPropertyFlags properties, VkBuffer& buffer,
          VkDeviceMemory& bufferMemory); 
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties); 

    // frame management
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    size_t currentFrame = 0;
    std::vector<VkFence> inFlightFences;  // Update from single fence
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    // Uniform buffers
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;


    // Descriptor sets
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    void createDescriptorPool();
    void createDescriptorSets(); 

    // Depth resources
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    void createImage(uint32_t width, uint32_t height, VkFormat format,
                    VkImageTiling tiling, VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkImage& image,
                    VkDeviceMemory& imageMemory);
    void createDepthResources();

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createRoadBuffers();

    void createWorldPipeline();

    // Car rendering methods
    void loadCarModel();
    void createCarPipeline();
    void createCarDescriptorSets();

    // Road rendering methods
    void loadRoadModel();

    // Windshield rendering methods
    void createWindshieldPipeline();
    void renderWindshield(VkCommandBuffer cmd, uint32_t frameIndex);

    // Car simulation methods
    void updateCarPhysics(float deltaTime);
    void updateCameraForCockpit();

    // Debug visualization methods
    void createDebugPipeline();
    void createDebugMarkers();
    void updateDebugMarkers();
    void renderDebugMarkers(VkCommandBuffer cmd, uint32_t frameIndex);

};

} // namespace DownPour
