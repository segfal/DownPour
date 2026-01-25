#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../vulkan/VulkanTypes.h"

#include <vector>

namespace DownPour {

/**
 * @brief Manages Vulkan swap chain and related resources
 *
 * Encapsulates swap chain creation, framebuffers, image views, and render pass.
 * Separates presentation/display logic from the main Application class.
 */
class SwapChainManager {
public:
    SwapChainManager() = default;
    ~SwapChainManager() = default;

    /**
     * @brief Initialize swap chain and related resources
     *
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param surface Window surface
     * @param window GLFW window for extent queries
     * @param depthFormat Format for depth attachment
     */
    void initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                   GLFWwindow* window, VkFormat depthFormat);

    /**
     * @brief Clean up swap chain resources
     */
    void cleanup(VkDevice device);

    // Accessors
    VkSwapchainKHR getSwapChain() const { return swapchain; }
    VkFormat getImageFormat() const { return swapchainImageFormat; }
    VkExtent2D getExtent() const { return swapchainExtent; }
    VkRenderPass getRenderPass() const { return renderPass; }

    const std::vector<VkImage>& getImages() const { return swapchainImages; }
    const std::vector<VkImageView>& getImageViews() const { return swapchainImageViews; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapchainFramebuffers; }

    /**
     * @brief Update framebuffers after depth image is created
     */
    void createFramebuffers(VkDevice device, VkImageView depthImageView);

private:
    VkSwapchainKHR             swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>       swapchainImages;
    VkFormat                   swapchainImageFormat;
    VkExtent2D                 swapchainExtent;
    std::vector<VkImageView>   swapchainImageViews;
    VkRenderPass               renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    VkFormat depthFormat;

    void createSwapChain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, GLFWwindow* window);
    void createImageViews(VkDevice device);
    void createRenderPass(VkDevice device);

    Vulkan::SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
};

} // namespace DownPour
