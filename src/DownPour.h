#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan/VulkanTypes.h"

#include <cstdint>
#include <vector>

namespace DownPour {

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
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    // GLFW and Vulkan handles
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    
    // Queue handles
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    
    // Swap chain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;

    // Initialization methods
    void initWindow();
    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();

    // Main loop and cleanup
    void mainLoop();
    void cleanup();
    
    // Helper methods
    Vulkan::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    Vulkan::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

} // namespace DownPour
