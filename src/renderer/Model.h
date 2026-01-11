#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include "Vertex.h"

namespace DownPour {

/**
 * @brief Material structure holding texture resources and index range
 */
struct Material {
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
    uint32_t indexStart = 0;  // Starting index in the index buffer
    uint32_t indexCount = 0;  // Number of indices for this material
};

/**
 * @brief Model class for loading and rendering GLTF models
 *
 * This class handles loading GLTF/GLB files using tinygltf,
 * extracting vertex and index data, loading textures, and
 * managing Vulkan resources for rendering.
 */
class Model {
public:
    Model() = default;
    ~Model() = default;

    /**
     * @brief Load a GLTF/GLB model from file
     *
     * @param filepath Path to the GLTF/GLB file
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for buffer operations
     * @param graphicsQueue Graphics queue for command submission
     */
    void loadFromFile(const std::string& filepath,
                     VkDevice device,
                     VkPhysicalDevice physicalDevice,
                     VkCommandPool commandPool,
                     VkQueue graphicsQueue);

    /**
     * @brief Clean up Vulkan resources
     */
    void cleanup(VkDevice device);

    /**
     * @brief Get the vertex buffer
     */
    VkBuffer getVertexBuffer() const { return vertexBuffer; }

    /**
     * @brief Get the index buffer
     */
    VkBuffer getIndexBuffer() const { return indexBuffer; }

    /**
     * @brief Get the number of indices
     */
    uint32_t getIndexCount() const { return indexCount; }

    /**
     * @brief Get the model matrix
     */
    glm::mat4 getModelMatrix() const { return modelMatrix; }

    /**
     * @brief Set the model matrix
     */
    void setModelMatrix(const glm::mat4& matrix) { modelMatrix = matrix; }

    /**
     * @brief Get all materials
     */
    const std::vector<Material>& getMaterials() const { return materials; }

    /**
     * @brief Check if model has any materials with textures
     */
    bool hasTexture() const { return !materials.empty(); }

    /**
     * @brief Get the minimum bounds of the model
     */
    glm::vec3 getMinBounds() const { return minBounds; }

    /**
     * @brief Get the maximum bounds of the model
     */
    glm::vec3 getMaxBounds() const { return maxBounds; }

    /**
     * @brief Get the dimensions (width, height, depth) of the model
     */
    glm::vec3 getDimensions() const { return maxBounds - minBounds; }

    /**
     * @brief Get the center of the model
     */
    glm::vec3 getCenter() const { return (minBounds + maxBounds) * 0.5f; }

private:
    // Vertex and index data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Vulkan resources
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;

    // Materials with texture resources and index ranges
    std::vector<Material> materials;

    // Transform
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Bounding box
    glm::vec3 minBounds = glm::vec3(0.0f);
    glm::vec3 maxBounds = glm::vec3(0.0f);

    // Helper methods
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkCommandPool commandPool, VkQueue graphicsQueue);
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool, VkQueue graphicsQueue);
    void createTextureImage(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkCommandPool commandPool, VkQueue graphicsQueue,
                           const unsigned char* pixels, int width, int height, int channels,
                           Material& material);
    void createTextureImageView(VkDevice device, Material& material);
    void createTextureSampler(VkDevice device, Material& material);

    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                   VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                           VkMemoryPropertyFlags properties);
    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                              VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                          VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};

} // namespace DownPour
