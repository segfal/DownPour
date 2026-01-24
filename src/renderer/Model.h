#pragma once

#include "Material.h"
#include "Mesh.h"
#include "Vertex.h"

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace DownPour {

/**
 * @brief Model class for loading and rendering GLTF models
 *
 * This class handles loading GLTF/GLB files using tinygltf,
 * extracting vertex and index data, and material definitions.
 *
 * GPU resources (textures, descriptor sets) are managed by MaterialManager.
 * Model just holds geometry (vertices, indices) and material data.
 */
class Model {
public:
    Model()  = default;
    ~Model() = default;

    /**
     * @brief Load a GLTF/GLB model from file
     *
     * Extracts geometry and material definitions from the model file.
     * Does NOT create GPU resources - use MaterialManager for that.
     *
     * @param filepath Path to the GLTF/GLB file
     * @param device Vulkan logical device (for geometry buffers)
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for buffer operations
     * @param graphicsQueue Graphics queue for command submission
     */
    void loadFromFile(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue);

    /**
     * @brief Clean up Vulkan geometry resources
     *
     * Note: Material GPU resources are managed by MaterialManager
     */
    void cleanup(VkDevice device);

    // Geometry accessors
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    uint32_t getIndexCount() const;

    // Mesh queries
    std::vector<std::string> getMeshNames() const;
    bool                     getMeshByName(const std::string& name, NamedMesh& outMesh) const;
    std::vector<NamedMesh>   getMeshesByPrefix(const std::string& prefix) const;
    bool                     getMeshIndexRange(const std::string& name, uint32_t& outStart, uint32_t& outCount) const;

    // Transform
    glm::mat4 getModelMatrix() const;
    void      setModelMatrix(const glm::mat4& matrix);

    // Materials (NEW: returns Material, not LegacyMaterial)
    const std::vector<Material>& getMaterials() const;
    size_t                       getMaterialCount() const;
    const Material&              getMaterial(size_t index) const;

    // Bounding box
    glm::vec3 getMinBounds() const;
    glm::vec3 getMaxBounds() const;
    glm::vec3 getDimensions() const;
    glm::vec3 getCenter() const;

private:
    // Geometry data
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    // Vulkan geometry resources
    VkBuffer       vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       indexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory  = VK_NULL_HANDLE;
    uint32_t       indexCount         = 0;

    // Material definitions (NEW: pure data, no GPU resources)
    std::vector<Material> materials;

    // Transform
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Bounding box
    glm::vec3 minBounds = glm::vec3(0.0f);
    glm::vec3 maxBounds = glm::vec3(0.0f);

    // Named meshes
    std::vector<NamedMesh> namedMeshes;

    // Helper methods for geometry
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                            VkQueue graphicsQueue);

    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                           VkQueue graphicsQueue);

    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer,
                    VkBuffer dstBuffer, VkDeviceSize size);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    std::string resolveTexturePath(const std::string& modelPath, const std::string& textureUri);
};

}  // namespace DownPour
