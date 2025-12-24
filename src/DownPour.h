#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>

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
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    // Initialization methods
    void initWindow();
    void initVulkan();
    void createInstance();
    void createSurface();

    // Main loop and cleanup
    void mainLoop();
    void cleanup();
};

} // namespace DownPour
