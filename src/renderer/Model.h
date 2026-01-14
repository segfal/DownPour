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
    // Base color texture
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    // PBR textures (optional)
    VkImage normalMap = VK_NULL_HANDLE;
    VkDeviceMemory normalMapMemory = VK_NULL_HANDLE;
    VkImageView normalMapView = VK_NULL_HANDLE;
    VkSampler normalMapSampler = VK_NULL_HANDLE;

    VkImage metallicRoughnessMap = VK_NULL_HANDLE;
    VkDeviceMemory metallicRoughnessMemory = VK_NULL_HANDLE;
    VkImageView metallicRoughnessView = VK_NULL_HANDLE;
    VkSampler metallicRoughnessSampler = VK_NULL_HANDLE;

    VkImage emissiveMap = VK_NULL_HANDLE;
    VkDeviceMemory emissiveMapMemory = VK_NULL_HANDLE;
    VkImageView emissiveMapView = VK_NULL_HANDLE;
    VkSampler emissiveMapSampler = VK_NULL_HANDLE;

    // Index range for this material
    uint32_t indexStart = 0;  // Starting index in the index buffer
    uint32_t indexCount = 0;  // Number of indices for this material

    // Flags to track which textures are present
    bool hasNormalMap = false;
    bool hasMetallicRoughness = false;
    bool hasEmissive = false;
};

/**
 * @brief Named mesh structure holding mesh name and index range
 *
 * A glTF mesh can contain multiple primitives. This struct stores
 * the index range for a specific primitive within a mesh.
 */
struct NamedMesh {
    std::string name;           // Mesh name from glTF
    uint32_t primitiveIndex;    // Which primitive within the mesh (0, 1, 2...)
    uint32_t indexStart;        // Starting index in the index buffer
    uint32_t indexCount;        // Number of indices for this primitive
    glm::mat4 transform;        // Optional transform (defaults to identity)

    NamedMesh()
        : primitiveIndex(0)
        , indexStart(0)
        , indexCount(0)
        , transform(glm::mat4(1.0f)) {}
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
     * @brief Get all mesh names loaded in the model
     * @return Vector of all mesh names
     */
    std::vector<std::string> getMeshNames() const;

    /**
     * @brief Get a mesh by exact name match
     * @param name The exact mesh name to search for
     * @param outMesh Output parameter filled with mesh data if found
     * @return true if mesh was found, false otherwise
     */
    bool getMeshByName(const std::string& name, NamedMesh& outMesh) const;

    /**
     * @brief Get all meshes whose names start with the given prefix
     * @param prefix The name prefix to search for
     * @return Vector of matching meshes (empty if none found)
     */
    std::vector<NamedMesh> getMeshesByPrefix(const std::string& prefix) const;

    /**
     * @brief Get the index range for a specific mesh
     * @param name The mesh name to search for
     * @param outStart Output parameter for index start
     * @param outCount Output parameter for index count
     * @return true if mesh was found, false otherwise
     */
    bool getMeshIndexRange(const std::string& name, uint32_t& outStart, uint32_t& outCount) const;

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

    // External texture loading
    void loadExternalTexture(const std::string& filepath,
                            VkDevice device,
                            VkPhysicalDevice physicalDevice,
                            VkCommandPool commandPool,
                            VkQueue graphicsQueue,
                            VkImage& outImage,
                            VkDeviceMemory& outMemory,
                            VkImageView& outView,
                            VkSampler& outSampler);

    std::string resolveTexturePath(const std::string& modelPath,
                                   const std::string& textureUri);

    // Named mesh storage
    std::vector<NamedMesh> namedMeshes;
};

} // namespace DownPour
