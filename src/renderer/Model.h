#pragma once

#include "../core/Types.h"
#include "Material.h"
#include "Mesh.h"
#include "Vertex.h"

#include <string>
#include <vector>

// Use centralized type aliases
using namespace DownPour::Types;

namespace DownPour {

/**
 * @brief glTF node representation
 *
 * Stores transform and hierarchy information from a glTF node.
 * Used to preserve the scene graph structure from the model file.
 */
struct glTFNode {
    std::string      name;
    int              meshIndex      = -1;  // Index into model.meshes
    int              primitiveIndex = -1;  // Which primitive within the mesh
    Vec3             translation    = Vec3(0.0f);
    Quat             rotation       = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    Vec3             scale          = Vec3(1.0f);
    Mat4             matrix         = Mat4(1.0f);  // If node uses matrix instead of TRS
    std::vector<int> children;
    int              parent = -1;
};

/**
 * @brief glTF scene representation
 *
 * A glTF file can contain multiple scenes. Each scene has root nodes.
 */
struct glTFScene {
    std::string      name;
    std::vector<int> rootNodes;
};

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
    enum class CameraMode { ORBIT, FOLLOW, COCKPIT };
    CameraMode cameraMode    = CameraMode::COCKPIT;
    float      orbitYaw      = 0.0f;
    float      orbitPitch    = 0.0f;
    float      orbitDistance = 10.0f;
    bool       cKeyPressed   = false;

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
    Mat4 getModelMatrix() const;
    void setModelMatrix(const Mat4& matrix);

    // Materials (NEW: returns Material, not LegacyMaterial)
    const std::vector<Material>& getMaterials() const;
    size_t                       getMaterialCount() const;
    const Material&              getMaterial(size_t index) const;

    /**
     * @brief Find materials associated with a specific mesh
     * @param meshIndex glTF mesh index
     * @return Vector of (material index, Material) pairs for this mesh
     */
    std::vector<std::pair<size_t, const Material*>> getMaterialsForMesh(int32_t meshIndex) const;

    // Bounding box
    Vec3 getMinBounds() const;
    Vec3 getMaxBounds() const;
    Vec3 getDimensions() const;
    Vec3 getCenter() const;

    /**
     * @brief Calculate the bounding box of the entire model hierarchy
     *
     * Unlike getMin/MaxBounds which return raw vertex bounds, this
     * accounts for all node transforms in the default scene.
     */
    void getHierarchyBounds(Vec3& minOut, Vec3& maxOut) const;

    // Hierarchy (NEW: glTF scene graph)
    const std::vector<glTFNode>&  getNodes() const;
    const glTFScene&              getDefaultScene() const;
    const std::vector<glTFScene>& getScenes() const;
    bool                          hasHierarchy() const;

    void updateCamera(float deltaTime);
    // setters
    void setCameraMode(CameraMode mode);
    void setOrbitYaw(float yaw);
    void setOrbitPitch(float pitch);
    void setOrbitDistance(float distance);
    void setCKeyPressed(bool cPressed);
    // getters
    CameraMode getCameraMode() const;
    float      getOrbitYaw() const;
    float      getOrbitPitch() const;
    float      getOrbitDistance() const;
    bool       getCKeyPressed() const;

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
    Mat4 modelMatrix = Mat4(1.0f);

    // Bounding box
    Vec3 minBounds = Vec3(0.0f);
    Vec3 maxBounds = Vec3(0.0f);

    // Named meshes
    std::vector<NamedMesh> namedMeshes;

    // Hierarchy data (NEW: extracted from glTF)
    std::vector<glTFNode>  nodes;
    std::vector<glTFScene> scenes;
    int                    defaultSceneIndex = 0;

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

    /**
     * @brief Helper to process glTF textures (handles external paths and embedded data)
     */
    void processGLTFTexture(const std::string& filepath, const void* modelPtr, int textureIndex,
                            std::string& outPath, EmbeddedTexture& outEmbedded, bool& outHasFlag);
};

}  // namespace DownPour