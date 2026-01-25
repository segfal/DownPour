#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Model.h"

#include "logger/Logger.h"

#include <tiny_gltf.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace DownPour {

void Model::loadFromFile(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model    model;
    std::string        err, warn;

    bool ret = false;
    if (filepath.substr(filepath.find_last_of(".") + 1) == "glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }

    // Check for loading errors
    Log logger;

    if (!warn.empty()) {
        logger.log("warning", "glTF Warning: " + warn);
    }

    if (!err.empty()) {
        logger.log("error", "glTF Error: " + err);
    }

    if (!ret) {
        logger.log("fatal", "Failed to load glTF model: " + filepath);
        throw std::runtime_error("Failed to load glTF model: " + filepath);
    }

    logger.log("info", "Successfully loaded glTF model: " + filepath);
    logger.log("info", "  Nodes: " + std::to_string(model.nodes.size()));
    logger.log("info", "  Meshes: " + std::to_string(model.meshes.size()));
    logger.log("info", "  Materials: " + std::to_string(model.materials.size()));

    // Process each mesh in the model
    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
        const auto& mesh = model.meshes[meshIdx];
        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); primIdx++) {
            const auto& primitive = mesh.primitives[primIdx];
            // Track index start for this primitive
            uint32_t primitiveIndexStart = static_cast<uint32_t>(indices.size());

            // Set up bounds tracking for this primitive
            glm::vec3 primMin(0.0f), primMax(0.0f);
            NamedMesh namedMesh;

            // Get accessors
            const tinygltf::Accessor&   posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView     = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer&     posBuffer   = model.buffers[posView.buffer];

            // Get normals if available
            const tinygltf::Accessor*   normalAccessor = nullptr;
            const tinygltf::BufferView* normalView     = nullptr;
            const tinygltf::Buffer*     normalBuffer   = nullptr;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
                normalView     = &model.bufferViews[normalAccessor->bufferView];
                normalBuffer   = &model.buffers[normalView->buffer];
            }

            // Get texture coordinates if available
            const tinygltf::Accessor*   texCoordAccessor = nullptr;
            const tinygltf::BufferView* texCoordView     = nullptr;
            const tinygltf::Buffer*     texCoordBuffer   = nullptr;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoordView     = &model.bufferViews[texCoordAccessor->bufferView];
                texCoordBuffer   = &model.buffers[texCoordView->buffer];
            }

            // Extract vertex data
            size_t vertexStart = vertices.size();
            for (size_t i = 0; i < posAccessor.count; ++i) {
                Vertex vertex{};

                // Position
                const float* pos = reinterpret_cast<const float*>(
                    &posBuffer.data[posView.byteOffset + posAccessor.byteOffset + i * 12]);
                vertex.position = glm::vec3(pos[0], pos[1], pos[2]);

                // Track primitive bounds
                if (i == 0) {
                    primMin = primMax = vertex.position;
                } else {
                    primMin = glm::min(primMin, vertex.position);
                    primMax = glm::max(primMax, vertex.position);
                }

                // Normal
                if (normalAccessor) {
                    const float* norm = reinterpret_cast<const float*>(
                        &normalBuffer->data[normalView->byteOffset + normalAccessor->byteOffset + i * 12]);
                    vertex.normal = glm::vec3(norm[0], norm[1], norm[2]);
                } else {
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                // Texture coordinates
                if (texCoordAccessor) {
                    const float* texCoord = reinterpret_cast<const float*>(
                        &texCoordBuffer->data[texCoordView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
                    vertex.texCoord = glm::vec2(texCoord[0], texCoord[1]);
                } else {
                    vertex.texCoord = glm::vec2(0.0f, 0.0f);
                }

                vertices.push_back(vertex);
            }

            // Extract index data
            if (primitive.indices >= 0) {
                const tinygltf::Accessor&   indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView     = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer&     indexBuffer   = model.buffers[indexView.buffer];

                const void* dataPtr = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];

                for (size_t i = 0; i < indexAccessor.count; ++i) {
                    uint32_t index = 0;
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        index = static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(dataPtr)[i]);
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        index = reinterpret_cast<const uint32_t*>(dataPtr)[i];
                    }
                    indices.push_back(vertexStart + index);
                }
            }

            // Calculate index count for this primitive
            uint32_t primitiveIndexCount = static_cast<uint32_t>(indices.size()) - primitiveIndexStart;

            // Store mesh/primitive bounding box
            namedMesh.minBounds = primMin;
            namedMesh.maxBounds = primMax;

            // Store named mesh information
            namedMesh.name           = mesh.name;
            namedMesh.meshIndex      = static_cast<uint32_t>(meshIdx);
            namedMesh.primitiveIndex = static_cast<uint32_t>(primIdx);
            namedMesh.indexStart     = primitiveIndexStart;
            namedMesh.indexCount     = primitiveIndexCount;
            namedMesh.transform      = glm::mat4(1.0f);

            // If mesh has multiple primitives, append primitive index to name
            if (mesh.primitives.size() > 1) {
                namedMesh.name += "_primitive" + std::to_string(namedMesh.primitiveIndex);
            }

            // Store for hierarchy calculation even if unnamed
            namedMeshes.push_back(namedMesh);

            // Load texture/material if available
            if (primitive.material >= 0) {
                const tinygltf::Material& gltfMaterial = model.materials[primitive.material];
                Material                  newMaterial;
                bool                      dummyFlag = false;
                newMaterial.name           = gltfMaterial.name;
                newMaterial.meshIndex      = static_cast<int32_t>(meshIdx);
                newMaterial.primitiveIndex = static_cast<int32_t>(primIdx);
                newMaterial.indexStart     = primitiveIndexStart;
                newMaterial.indexCount     = primitiveIndexCount;

                // Name-based glass detection
                std::string nameLower = gltfMaterial.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                if (nameLower.find("glass") != std::string::npos || nameLower.find("window") != std::string::npos ||
                    nameLower.find("windshield") != std::string::npos ||
                    nameLower.find("transparent") != std::string::npos) {
                    newMaterial.props.isTransparent = true;
                    if (newMaterial.props.alphaValue >= 0.99f) {
                        newMaterial.props.alphaValue = 0.3f;
                    }
                }

                // Detect transparency from glTF alphaMode
                if (gltfMaterial.alphaMode == "BLEND")
                    newMaterial.props.isTransparent = true;

                // Get base color factor alpha
                bool hasCustomAlpha = (newMaterial.props.alphaValue < 0.99f);
                if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() >= 4 && !hasCustomAlpha) {
                    newMaterial.props.alphaValue =
                        static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]);
                    if (newMaterial.props.alphaValue < 0.99f) {
                        newMaterial.props.isTransparent = true;
                    }
                }

                // Extract texture paths or embedded data using helper
                processGLTFTexture(filepath, &model, gltfMaterial.pbrMetallicRoughness.baseColorTexture.index,
                                   newMaterial.baseColorTexture, newMaterial.embeddedBaseColor, dummyFlag);

                processGLTFTexture(filepath, &model, gltfMaterial.normalTexture.index, newMaterial.normalMapTexture,
                                   newMaterial.embeddedNormalMap, newMaterial.props.hasNormalMap);

                processGLTFTexture(filepath, &model, gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index,
                                   newMaterial.metallicRoughnessTexture, newMaterial.embeddedMetallicRoughness,
                                   newMaterial.props.hasMetallicRoughness);

                processGLTFTexture(filepath, &model, gltfMaterial.emissiveTexture.index, newMaterial.emissiveTexture,
                                   newMaterial.embeddedEmissive, newMaterial.props.hasEmissive);

                materials.push_back(newMaterial);
            }
        }
    }

    this->indexCount = static_cast<uint32_t>(indices.size());

    // Calculate bounding box
    if (!vertices.empty()) {
        minBounds = vertices[0].position;
        maxBounds = vertices[0].position;

        for (const auto& vertex : vertices) {
            minBounds.x = std::min(minBounds.x, vertex.position.x);
            minBounds.y = std::min(minBounds.y, vertex.position.y);
            minBounds.z = std::min(minBounds.z, vertex.position.z);
            maxBounds.x = std::max(maxBounds.x, vertex.position.x);
            maxBounds.y = std::max(maxBounds.y, vertex.position.y);
            maxBounds.z = std::max(maxBounds.z, vertex.position.z);
        }

        glm::vec3 dimensions = maxBounds - minBounds;
        glm::vec3 center     = (minBounds + maxBounds) * 0.5f;
    }

    // Extract glTF hierarchy (scene graph)
    nodes.reserve(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); i++) {
        const tinygltf::Node& gltfNode = model.nodes[i];
        glTFNode              node;

        node.name      = gltfNode.name;
        node.meshIndex = gltfNode.mesh;

        // Extract transform - glTF nodes can have either matrix or TRS
        if (!gltfNode.matrix.empty() && gltfNode.matrix.size() == 16) {
            // Node uses matrix representation (glTF stores matrices in column-major order)
            const auto& m = gltfNode.matrix;
            node.matrix   = glm::mat4(static_cast<float>(m[0]), static_cast<float>(m[1]), static_cast<float>(m[2]),
                                      static_cast<float>(m[3]), static_cast<float>(m[4]), static_cast<float>(m[5]),
                                      static_cast<float>(m[6]), static_cast<float>(m[7]), static_cast<float>(m[8]),
                                      static_cast<float>(m[9]), static_cast<float>(m[10]), static_cast<float>(m[11]),
                                      static_cast<float>(m[12]), static_cast<float>(m[13]), static_cast<float>(m[14]),
                                      static_cast<float>(m[15]));
        } else {
            // Node uses TRS representation
            if (!gltfNode.translation.empty() && gltfNode.translation.size() == 3) {
                node.translation = glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
            }
            if (!gltfNode.rotation.empty() && gltfNode.rotation.size() == 4) {
                // glTF quaternion order: [x, y, z, w]
                // glm quaternion order: [w, x, y, z]
                node.rotation =
                    glm::quat(static_cast<float>(gltfNode.rotation[3]), static_cast<float>(gltfNode.rotation[0]),
                              static_cast<float>(gltfNode.rotation[1]), static_cast<float>(gltfNode.rotation[2]));
            }
            if (!gltfNode.scale.empty() && gltfNode.scale.size() == 3) {
                node.scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
            }
        }

        // Extract children indices
        node.children.reserve(gltfNode.children.size());
        for (int childIdx : gltfNode.children) {
            node.children.push_back(childIdx);
        }

        nodes.push_back(node);
    }

    // Build parent links from children
    for (size_t i = 0; i < nodes.size(); i++) {
        for (int childIdx : nodes[i].children) {
            if (childIdx >= 0 && childIdx < static_cast<int>(nodes.size())) {
                nodes[childIdx].parent = static_cast<int>(i);
            }
        }
    }

    // Extract scenes
    scenes.reserve(model.scenes.size());
    for (const auto& gltfScene : model.scenes) {
        glTFScene scene;
        scene.name      = gltfScene.name;
        scene.rootNodes = gltfScene.nodes;
        scenes.push_back(scene);
    }
    defaultSceneIndex =
        (model.defaultScene >= 0 && model.defaultScene < static_cast<int>(scenes.size())) ? model.defaultScene : 0;

    // Create Vulkan buffers
    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue);
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue);
}

void Model::createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                               VkQueue graphicsQueue) {
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    // Create staging buffer
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                 stagingBufferMemory);

    // Copy vertex data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create device local buffer
    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    // Copy from staging to device local
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                              VkQueue graphicsQueue) {
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    // Create staging buffer
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                 stagingBufferMemory);

    // Copy index data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create device local buffer
    createBuffer(device, physicalDevice, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    // Copy from staging to device local
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, indexBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Model::copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer,
                       VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t Model::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

std::string Model::resolveTexturePath(const std::string& modelPath, const std::string& textureUri) {
    // Get directory of model file
    std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();

    // Resolve relative URI (try model directory first)
    std::filesystem::path texturePath = modelDir / textureUri;

    if (std::filesystem::exists(texturePath)) {
        return texturePath.string();
    }

    // Try alternate location: assets/textures/[model_name]/[texture_file]
    std::string           modelName       = std::filesystem::path(modelPath).stem().string();
    std::filesystem::path textureFilename = std::filesystem::path(textureUri).filename();
    std::filesystem::path altPath         = std::filesystem::path("assets/textures") / modelName / textureFilename;

    if (std::filesystem::exists(altPath)) {
        return altPath.string();
    }

    // Return original path and let loading fail gracefully
    std::cerr << "Warning: Could not resolve texture path for: " << textureUri << std::endl;
    std::cerr << "  Tried: " << texturePath.string() << std::endl;
    std::cerr << "  Tried: " << altPath.string() << std::endl;
    return texturePath.string();
}

void Model::processGLTFTexture(const std::string& filepath, const void* modelPtr, int textureIndex,
                               std::string& outPath, EmbeddedTexture& outEmbedded, bool& outHasFlag) {
    if (textureIndex < 0)
        return;

    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    if (textureIndex >= static_cast<int>(model.textures.size()))
        return;

    const auto& texture = model.textures[textureIndex];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
        return;

    const auto& image = model.images[texture.source];

    if (!image.uri.empty()) {
        outPath = resolveTexturePath(filepath, image.uri);
    } else if (!image.image.empty()) {
        outEmbedded.pixels = image.image;
        outEmbedded.width  = image.width;
        outEmbedded.height = image.height;
    }
    outHasFlag = true;
}

void Model::cleanup(VkDevice device) {
    materials.clear();

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    indexBuffer        = VK_NULL_HANDLE;
    indexBufferMemory  = VK_NULL_HANDLE;
    vertexBuffer       = VK_NULL_HANDLE;
    vertexBufferMemory = VK_NULL_HANDLE;
}

VkBuffer Model::getVertexBuffer() const {
    return this->vertexBuffer;
}

VkBuffer Model::getIndexBuffer() const {
    return this->indexBuffer;
}

uint32_t Model::getIndexCount() const {
    return this->indexCount;
}

glm::mat4 Model::getModelMatrix() const {
    return this->modelMatrix;
}

void Model::setModelMatrix(const glm::mat4& matrix) {
    this->modelMatrix = matrix;
}

const std::vector<Material>& Model::getMaterials() const {
    return this->materials;
}

size_t Model::getMaterialCount() const {
    return this->materials.size();
}

const Material& Model::getMaterial(size_t index) const {
    if (index >= this->materials.size())
        throw std::out_of_range("Material index out of range");
    return this->materials[index];
}

std::vector<std::pair<size_t, const Material*>> Model::getMaterialsForMesh(int32_t meshIndex) const {
    std::vector<std::pair<size_t, const Material*>> results;
    for (size_t i = 0; i < materials.size(); i++) {
        if (materials[i].meshIndex == meshIndex) {
            results.push_back({i, &materials[i]});
        }
    }
    return results;
}

glm::vec3 Model::getMinBounds() const {
    return minBounds;
}

glm::vec3 Model::getMaxBounds() const {
    return maxBounds;
}

glm::vec3 Model::getDimensions() const {
    return maxBounds - minBounds;
}

glm::vec3 Model::getCenter() const {
    return (minBounds + maxBounds) * 0.5f;
}

void Model::getHierarchyBounds(glm::vec3& minOut, glm::vec3& maxOut) const {
    if (nodes.empty() || scenes.empty()) {
        minOut = minBounds;
        maxOut = maxBounds;
        return;
    }

    bool      initialized = false;
    glm::vec3 finalMin(0.0f), finalMax(0.0f);

    const auto& defaultScene = scenes[defaultSceneIndex];

    // Recursive helper to traverse hierarchy
    auto traverse = [&](auto& self, int nodeIdx, const glm::mat4& parentMatrix) -> void {
        const auto& node = nodes[nodeIdx];

        // Compute local matrix
        glm::mat4 localMatrix;
        if (node.matrix != glm::mat4(1.0f)) {
            localMatrix = node.matrix;
        } else {
            localMatrix = glm::translate(glm::mat4(1.0f), node.translation) * glm::mat4_cast(node.rotation) *
                          glm::scale(glm::mat4(1.0f), node.scale);
        }

        glm::mat4 globalMatrix = parentMatrix * localMatrix;

        // If node has a mesh, transform its bounding boxes
        if (node.meshIndex >= 0) {
            for (const auto& mesh : namedMeshes) {
                if (mesh.meshIndex == static_cast<uint32_t>(node.meshIndex)) {
                    // Transform primitive bounding box corners
                    glm::vec3 corners[8] = {{mesh.minBounds.x, mesh.minBounds.y, mesh.minBounds.z},
                                            {mesh.minBounds.x, mesh.minBounds.y, mesh.maxBounds.z},
                                            {mesh.minBounds.x, mesh.maxBounds.y, mesh.minBounds.z},
                                            {mesh.minBounds.x, mesh.maxBounds.y, mesh.maxBounds.z},
                                            {mesh.maxBounds.x, mesh.minBounds.y, mesh.minBounds.z},
                                            {mesh.maxBounds.x, mesh.minBounds.y, mesh.maxBounds.z},
                                            {mesh.maxBounds.x, mesh.maxBounds.y, mesh.minBounds.z},
                                            {mesh.maxBounds.x, mesh.maxBounds.y, mesh.maxBounds.z}};

                    for (int i = 0; i < 8; ++i) {
                        glm::vec3 p = glm::vec3(globalMatrix * glm::vec4(corners[i], 1.0f));
                        if (!initialized) {
                            finalMin = finalMax = p;
                            initialized         = true;
                        } else {
                            finalMin = glm::min(finalMin, p);
                            finalMax = glm::max(finalMax, p);
                        }
                    }
                }
            }
        }

        // Recursively process children
        for (int childIdx : node.children) {
            self(self, childIdx, globalMatrix);
        }
    };

    for (int rootIdx : defaultScene.rootNodes) {
        traverse(traverse, rootIdx, glm::mat4(1.0f));
    }

    if (initialized) {
        minOut = finalMin;
        maxOut = finalMax;
    } else {
        minOut = minBounds;
        maxOut = maxBounds;
    }
}

std::vector<std::string> Model::getMeshNames() const {
    std::vector<std::string> names;
    names.reserve(namedMeshes.size());
    for (const auto& mesh : namedMeshes)
        names.push_back(mesh.name);
    return names;
}

bool Model::getMeshByName(const std::string& name, NamedMesh& outMesh) const {
    for (const auto& mesh : namedMeshes) {
        if (mesh.name == name) {
            outMesh = mesh;
            return true;
        }
    }
    return false;
}

std::vector<NamedMesh> Model::getMeshesByPrefix(const std::string& prefix) const {
    std::vector<NamedMesh> matches;
    for (const auto& mesh : namedMeshes) {
        if (mesh.name.find(prefix) == 0) {
            matches.push_back(mesh);
        }
    }
    return matches;
}

bool Model::getMeshIndexRange(const std::string& name, uint32_t& outStart, uint32_t& outCount) const {
    NamedMesh mesh;
    if (getMeshByName(name, mesh)) {
        outStart = mesh.indexStart;
        outCount = mesh.indexCount;
        return true;
    }
    return false;
}

// Hierarchy accessors
const std::vector<glTFNode>& Model::getNodes() const {
    return nodes;
}

const glTFScene& Model::getDefaultScene() const {
    static glTFScene emptyScene;
    if (scenes.empty()) {
        return emptyScene;
    }
    return scenes[defaultSceneIndex];
}

const std::vector<glTFScene>& Model::getScenes() const {
    return scenes;
}

bool Model::hasHierarchy() const {
    return !nodes.empty();
}

}  // namespace DownPour
