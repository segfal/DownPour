#include "TerrainGeometry.h"

#include "../core/ResourceManager.h"

#include <cstring>

namespace DownPour {

void TerrainGeometry::generate(float roadWidth, float terrainWidth, float terrainLength, float texTileSize) {
    vertices.clear();
    indices.clear();

    // Generate left strip (negative X)
    float leftStart = -(roadWidth / 2.0f + terrainWidth);
    float leftEnd   = -roadWidth / 2.0f;
    generateStrip(leftStart, leftEnd, 0.0f, terrainLength, texTileSize, 0);

    uint32_t leftVertexCount = static_cast<uint32_t>(vertices.size());

    // Generate right strip (positive X)
    float rightStart = roadWidth / 2.0f;
    float rightEnd   = roadWidth / 2.0f + terrainWidth;
    generateStrip(rightStart, rightEnd, 0.0f, terrainLength, texTileSize, leftVertexCount);
}

void TerrainGeometry::generateCentered(float roadWidth, float terrainWidth, float terrainLength, float texTileSize) {
    vertices.clear();
    indices.clear();

    float halfLength = terrainLength / 2.0f;

    // Generate left strip (negative X), centered on Z=0
    float leftStart = -(roadWidth / 2.0f + terrainWidth);
    float leftEnd   = -roadWidth / 2.0f;
    generateStrip(leftStart, leftEnd, -halfLength, halfLength, texTileSize, 0);

    uint32_t leftVertexCount = static_cast<uint32_t>(vertices.size());

    // Generate right strip (positive X), centered on Z=0
    float rightStart = roadWidth / 2.0f;
    float rightEnd   = roadWidth / 2.0f + terrainWidth;
    generateStrip(rightStart, rightEnd, -halfLength, halfLength, texTileSize, leftVertexCount);
}

void TerrainGeometry::generateStrip(float startX, float endX, float startZ, float endZ, float texTileSize,
                                    uint32_t vertexOffset) {
    // Subdivide every texTileSize meters for proper UV tiling
    float stripWidth  = endX - startX;
    float stripLength = endZ - startZ;

    int segmentsX = static_cast<int>(stripWidth / texTileSize) + 1;
    int segmentsZ = static_cast<int>(stripLength / texTileSize) + 1;

    // Ensure minimum subdivision
    segmentsX = std::max(segmentsX, 2);
    segmentsZ = std::max(segmentsZ, 2);

    // Generate vertices
    uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

    for (int iz = 0; iz <= segmentsZ; iz++) {
        for (int ix = 0; ix <= segmentsX; ix++) {
            float t_x = static_cast<float>(ix) / static_cast<float>(segmentsX);
            float t_z = static_cast<float>(iz) / static_cast<float>(segmentsZ);

            Vertex v;
            v.position = glm::vec3(startX + t_x * stripWidth, 0.0f,  // Y = 0 (ground level)
                                   startZ + t_z * stripLength);

            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);  // Up normal

            // UV tiling based on world position
            v.texCoord = glm::vec2(v.position.x / texTileSize, v.position.z / texTileSize);

            vertices.push_back(v);
        }
    }

    // Generate indices (two triangles per quad)
    for (int iz = 0; iz < segmentsZ; iz++) {
        for (int ix = 0; ix < segmentsX; ix++) {
            uint32_t i0 = baseVertex + iz * (segmentsX + 1) + ix;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + (segmentsX + 1);
            uint32_t i3 = i2 + 1;

            // Triangle 1
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            // Triangle 2
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
}

void TerrainGeometry::createBuffers(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                    VkQueue graphicsQueue) {
    if (vertices.empty() || indices.empty()) {
        return;
    }

    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize indexBufferSize  = sizeof(uint32_t) * indices.size();

    // Create vertex buffer
    VkBuffer       stagingVertexBuffer;
    VkDeviceMemory stagingVertexMemory;

    ResourceManager::createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingVertexBuffer, stagingVertexMemory);

    void* data;
    vkMapMemory(device, stagingVertexMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(device, stagingVertexMemory);

    ResourceManager::createBuffer(device, physicalDevice, vertexBufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    ResourceManager::copyBuffer(device, commandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer,
                                vertexBufferSize);

    vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
    vkFreeMemory(device, stagingVertexMemory, nullptr);

    // Create index buffer
    VkBuffer       stagingIndexBuffer;
    VkDeviceMemory stagingIndexMemory;

    ResourceManager::createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  stagingIndexBuffer, stagingIndexMemory);

    vkMapMemory(device, stagingIndexMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(device, stagingIndexMemory);

    ResourceManager::createBuffer(device, physicalDevice, indexBufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    ResourceManager::copyBuffer(device, commandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);

    vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
    vkFreeMemory(device, stagingIndexMemory, nullptr);
}

void TerrainGeometry::cleanup(VkDevice device) {
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
}

}  // namespace DownPour
