#pragma once

#include "Vertex.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace DownPour {

/**
 * @brief Procedural terrain mesh generator for grass strips alongside the road
 *
 * Generates two rectangular grass strips on either side of the road with
 * proper UV tiling for seamless grass textures.
 */
class TerrainGeometry {
public:
    TerrainGeometry() = default;
    ~TerrainGeometry() = default;

    /**
     * @brief Generate terrain geometry
     *
     * Creates two grass strips flanking the road with subdivided grid
     * for proper UV tiling and lighting.
     *
     * @param roadWidth Width of the road (grass starts outside this)
     * @param terrainWidth Width of each grass strip
     * @param terrainLength Length of terrain along Z axis
     * @param texTileSize Size of each UV tile in world units
     */
    void generate(float roadWidth, float terrainWidth, float terrainLength, float texTileSize);

    /**
     * @brief Generate terrain centered on Z=0
     *
     * Same as generate() but centers the terrain on the origin along Z,
     * so it extends from -terrainLength/2 to +terrainLength/2.
     */
    void generateCentered(float roadWidth, float terrainWidth, float terrainLength, float texTileSize);

    /**
     * @brief Create Vulkan vertex and index buffers
     *
     * @param device Vulkan logical device
     * @param physicalDevice Vulkan physical device
     * @param commandPool Command pool for staging buffer transfers
     * @param graphicsQueue Queue for transfer commands
     */
    void createBuffers(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                       VkQueue graphicsQueue);

    /**
     * @brief Clean up Vulkan resources
     *
     * @param device Vulkan logical device
     */
    void cleanup(VkDevice device);

    // Getters
    VkBuffer     getVertexBuffer() const { return vertexBuffer; }
    VkBuffer     getIndexBuffer() const { return indexBuffer; }
    uint32_t     getIndexCount() const { return static_cast<uint32_t>(indices.size()); }
    const auto&  getVertices() const { return vertices; }
    const auto&  getIndices() const { return indices; }

private:
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    VkBuffer       vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       indexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory  = VK_NULL_HANDLE;

    /**
     * @brief Generate a single grass strip with subdivided grid
     *
     * @param startX X position where strip starts
     * @param endX X position where strip ends
     * @param startZ Z position where strip starts
     * @param endZ Z position where strip ends
     * @param texTileSize Size of each UV tile in world units
     * @param vertexOffset Offset for indexing (for second strip)
     */
    void generateStrip(float startX, float endX, float startZ, float endZ, float texTileSize, uint32_t vertexOffset);
};

}  // namespace DownPour
