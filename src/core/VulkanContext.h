#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../vulkan/VulkanTypes.h"

#include <vector>

namespace DownPour {

/**
 * @brief Manages Vulkan instance, device, and surface initialization
 *
 * Encapsulates all Vulkan core setup, separating initialization logic
 * from the main Application class. Handles instance, physical device
 * selection, logical device creation, and queue management.
 */
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    /**
     * @brief Initialize Vulkan instance, device, and surface
     *
     * @param window GLFW window for surface creation
     */
    void initialize(GLFWwindow* window);

    /**
     * @brief Clean up Vulkan resources
     */
    void cleanup();

    // Accessors
    VkInstance getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }
    VkSurfaceKHR getSurface() const { return surface; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }

    Vulkan::QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

private:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    GLFWwindow* window = nullptr;

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
};

} // namespace DownPour
