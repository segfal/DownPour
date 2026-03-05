#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

namespace DownPour {

/**
 * @brief Manages Vulkan resource creation and memory management
 *
 * Handles buffer creation, image creation, memory allocation, and descriptor sets.
 */
class ResourceManager {
public:
    ResourceManager()  = default;
    ~ResourceManager() = default;

    /**
     * @brief Create a Vulkan buffer with memory
     *
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param size Buffer size in bytes
     * @param usage Buffer usage flags
     * @param properties Memory property flags
     * @param buffer Output buffer handle
     * @param bufferMemory Output memory handle
     */
    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
                             VkDeviceMemory& bufferMemory);

    /**
     * @brief Copy data between buffers using a command buffer
     *
     * @param device Vulkan logical device
     * @param commandPool Command pool for temporary command buffer
     * @param graphicsQueue Queue to submit copy command
     * @param srcBuffer Source buffer
     * @param dstBuffer Destination buffer
     * @param size Number of bytes to copy
     */
    static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer,
                           VkBuffer dstBuffer, VkDeviceSize size);

    /**
     * @brief Create a Vulkan image with memory
     *
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param width Image width
     * @param height Image height
     * @param format Image format
     * @param tiling Image tiling mode
     * @param usage Image usage flags
     * @param properties Memory property flags
     * @param image Output image handle
     * @param imageMemory Output memory handle
     */
    static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                            VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                            VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    /**
     * @brief Find suitable memory type from requirements
     *
     * @param physicalDevice Vulkan physical device
     * @param typeFilter Bitfield of suitable memory types
     * @param properties Required memory properties
     * @return Memory type index
     */
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);

    /**
     * @brief Find supported image format from candidates
     *
     * @param physicalDevice Vulkan physical device
     * @param candidates List of candidate formats
     * @param tiling Image tiling mode
     * @param features Required format features
     * @return Supported format
     */
    static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates,
                                        VkImageTiling tiling, VkFormatFeatureFlags features);

    /**
     * @brief Find suitable depth format
     *
     * @param physicalDevice Vulkan physical device
     * @return Depth format
     */
    static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
};

}  // namespace DownPour
