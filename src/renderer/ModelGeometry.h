#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace DownPour {

struct Vertex;

/**
 * @brief Manages Vulkan vertex and index buffers for a model
 *
 * Encapsulates all Vulkan buffer management logic, including buffer creation,
 * memory allocation, and data transfer. Separates GPU resource management
 * from model data storage.
 */
class ModelGeometry {
public:
    ModelGeometry()  = default;
    ~ModelGeometry() = default;

    /**
     * @brief Create Vulkan vertex and index buffers from CPU data
     *
     * @param vertices CPU vertex data
     * @param indices CPU index data
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device for memory queries
     * @param commandPool Command pool for buffer transfer operations
     * @param graphicsQueue Graphics queue for command submission
     */
    void createBuffers(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, VkDevice device,
                       VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Clean up Vulkan buffer resources
     *
     * @param device Vulkan logical device
     */
    void cleanup(VkDevice device);

    VkBuffer getVertexBuffer() const { return vertexBuffer; }
    VkBuffer getIndexBuffer() const { return indexBuffer; }
    uint32_t getIndexCount() const { return indexCount; }

private:
    VkBuffer       vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       indexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory  = VK_NULL_HANDLE;
    uint32_t       indexCount         = 0;

    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer,
                    VkBuffer dstBuffer, VkDeviceSize size);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

}  // namespace DownPour
