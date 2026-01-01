#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include "Vertex.h"

namespace DownPour {

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
     * @brief Get texture image view (if loaded)
     */
    VkImageView getTextureImageView() const { return textureImageView; }

    /**
     * @brief Get texture sampler
     */
    VkSampler getTextureSampler() const { return textureSampler; }

    /**
     * @brief Check if model has a texture
     */
    bool hasTexture() const { return textureImageView != VK_NULL_HANDLE; }

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

    // Texture resources
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    // Transform
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Helper methods
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkCommandPool commandPool, VkQueue graphicsQueue);
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool, VkQueue graphicsQueue);
    void createTextureImage(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkCommandPool commandPool, VkQueue graphicsQueue,
                           const unsigned char* pixels, int width, int height, int channels);
    void createTextureImageView(VkDevice device);
    void createTextureSampler(VkDevice device);

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
