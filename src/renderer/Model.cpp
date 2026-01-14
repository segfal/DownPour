#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Model.h"
#include <tiny_gltf.h>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <filesystem>

namespace DownPour {

void Model::loadFromFile(const std::string& filepath,
                        VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool,
                        VkQueue graphicsQueue) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ret = false;
    if (filepath.substr(filepath.find_last_of(".") + 1) == "glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }

    if (!warn.empty()) {
        std::cout << "GLTF Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        throw std::runtime_error("GLTF Error: " + err);
    }

    if (!ret) {
        throw std::runtime_error("Failed to load GLTF file: " + filepath);
    }

    std::cout << "Successfully loaded GLTF: " << filepath << std::endl;

    // Process each mesh in the model
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            // Track index start for this primitive
            uint32_t primitiveIndexStart = static_cast<uint32_t>(indices.size());

            // Get accessors
            const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

            // Get normals if available
            const tinygltf::Accessor* normalAccessor = nullptr;
            const tinygltf::BufferView* normalView = nullptr;
            const tinygltf::Buffer* normalBuffer = nullptr;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
                normalView = &model.bufferViews[normalAccessor->bufferView];
                normalBuffer = &model.buffers[normalView->buffer];
            }

            // Get texture coordinates if available
            const tinygltf::Accessor* texCoordAccessor = nullptr;
            const tinygltf::BufferView* texCoordView = nullptr;
            const tinygltf::Buffer* texCoordBuffer = nullptr;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoordView = &model.bufferViews[texCoordAccessor->bufferView];
                texCoordBuffer = &model.buffers[texCoordView->buffer];
            }

            // Extract vertex data
            size_t vertexStart = vertices.size();
            for (size_t i = 0; i < posAccessor.count; ++i) {
                Vertex vertex{};

                // Position
                const float* pos = reinterpret_cast<const float*>(
                    &posBuffer.data[posView.byteOffset + posAccessor.byteOffset + i * 12]);
                vertex.position = glm::vec3(pos[0], pos[1], pos[2]);

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
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

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

            // Store named mesh information
            NamedMesh namedMesh;
            namedMesh.name = mesh.name;
            namedMesh.primitiveIndex = static_cast<uint32_t>(&primitive - &mesh.primitives[0]);
            namedMesh.indexStart = primitiveIndexStart;
            namedMesh.indexCount = primitiveIndexCount;
            namedMesh.transform = glm::mat4(1.0f);

            // If mesh has multiple primitives, append primitive index to name
            if (mesh.primitives.size() > 1) {
                namedMesh.name += "_primitive" + std::to_string(namedMesh.primitiveIndex);
            }

            // Only store if mesh has a name and valid geometry
            if (!mesh.name.empty() && primitiveIndexCount > 0) {
                namedMeshes.push_back(namedMesh);
            }

            // Load texture/material if available
            if (primitive.material >= 0) {
                const tinygltf::Material& gltfMaterial = model.materials[primitive.material];
                Material newMaterial;
                newMaterial.indexStart = primitiveIndexStart;
                newMaterial.indexCount = primitiveIndexCount;
                bool materialHasTexture = false;

                // Load base color texture
                if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                    int textureIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
                    const tinygltf::Texture& texture = model.textures[textureIndex];
                    const tinygltf::Image& image = model.images[texture.source];

                    // Check if texture is external (has URI) or embedded
                    if (!image.uri.empty()) {
                        // External texture - load from file
                        std::string texturePath = resolveTexturePath(filepath, image.uri);
                        loadExternalTexture(texturePath, device, physicalDevice, commandPool, graphicsQueue,
                                          newMaterial.textureImage, newMaterial.textureImageMemory,
                                          newMaterial.textureImageView, newMaterial.textureSampler);
                        if (newMaterial.textureImage != VK_NULL_HANDLE) {
                            materialHasTexture = true;
                        }
                    } else if (!image.image.empty() && image.width > 0 && image.height > 0) {
                        // Embedded texture - load from image data
                        createTextureImage(device, physicalDevice, commandPool, graphicsQueue,
                                         image.image.data(), image.width, image.height, image.component,
                                         newMaterial);
                        createTextureImageView(device, newMaterial);
                        createTextureSampler(device, newMaterial);
                        std::cout << "Loaded embedded base color texture: " << image.width << "x" << image.height
                                 << " (" << image.component << " components)" << std::endl;
                        materialHasTexture = true;
                    }
                }

                // Load normal map
                if (gltfMaterial.normalTexture.index >= 0) {
                    int texIndex = gltfMaterial.normalTexture.index;
                    const tinygltf::Texture& texture = model.textures[texIndex];
                    const tinygltf::Image& image = model.images[texture.source];

                    if (!image.uri.empty()) {
                        std::string texturePath = resolveTexturePath(filepath, image.uri);
                        loadExternalTexture(texturePath, device, physicalDevice, commandPool, graphicsQueue,
                                          newMaterial.normalMap, newMaterial.normalMapMemory,
                                          newMaterial.normalMapView, newMaterial.normalMapSampler);
                        if (newMaterial.normalMap != VK_NULL_HANDLE) {
                            newMaterial.hasNormalMap = true;
                            materialHasTexture = true;
                        }
                    } else if (!image.image.empty()) {
                        // Could load embedded normal map here if needed
                        std::cout << "Found embedded normal map (not implemented)" << std::endl;
                    }
                }

                // Load metallic/roughness texture
                if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
                    int texIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
                    const tinygltf::Texture& texture = model.textures[texIndex];
                    const tinygltf::Image& image = model.images[texture.source];

                    if (!image.uri.empty()) {
                        std::string texturePath = resolveTexturePath(filepath, image.uri);
                        loadExternalTexture(texturePath, device, physicalDevice, commandPool, graphicsQueue,
                                          newMaterial.metallicRoughnessMap, newMaterial.metallicRoughnessMemory,
                                          newMaterial.metallicRoughnessView, newMaterial.metallicRoughnessSampler);
                        if (newMaterial.metallicRoughnessMap != VK_NULL_HANDLE) {
                            newMaterial.hasMetallicRoughness = true;
                            materialHasTexture = true;
                        }
                    } else if (!image.image.empty()) {
                        std::cout << "Found embedded metallic/roughness map (not implemented)" << std::endl;
                    }
                }

                // Load emissive texture
                if (gltfMaterial.emissiveTexture.index >= 0) {
                    int texIndex = gltfMaterial.emissiveTexture.index;
                    const tinygltf::Texture& texture = model.textures[texIndex];
                    const tinygltf::Image& image = model.images[texture.source];

                    if (!image.uri.empty()) {
                        std::string texturePath = resolveTexturePath(filepath, image.uri);
                        loadExternalTexture(texturePath, device, physicalDevice, commandPool, graphicsQueue,
                                          newMaterial.emissiveMap, newMaterial.emissiveMapMemory,
                                          newMaterial.emissiveMapView, newMaterial.emissiveMapSampler);
                        if (newMaterial.emissiveMap != VK_NULL_HANDLE) {
                            newMaterial.hasEmissive = true;
                            materialHasTexture = true;
                        }
                    } else if (!image.image.empty()) {
                        std::cout << "Found embedded emissive map (not implemented)" << std::endl;
                    }
                }

                // Only add material if it has at least one texture
                if (materialHasTexture) {
                    materials.push_back(newMaterial);
                }
            }
        }
    }

    // Print loaded mesh names for debugging
    if (!namedMeshes.empty()) {
        std::cout << "\n=== LOADED MESH NAMES ===" << std::endl;
        std::cout << "Found " << namedMeshes.size() << " named meshes:" << std::endl;
        for (const auto& mesh : namedMeshes) {
            std::cout << "  - \"" << mesh.name << "\" (primitive " << mesh.primitiveIndex
                      << "): indices [" << mesh.indexStart << " to "
                      << (mesh.indexStart + mesh.indexCount - 1) << "], count = "
                      << mesh.indexCount << std::endl;
        }
        std::cout << "=========================\n" << std::endl;
    }

    indexCount = static_cast<uint32_t>(indices.size());

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
        glm::vec3 center = (minBounds + maxBounds) * 0.5f;
        
        std::cout << "\n=== MODEL BOUNDS INFO ===" << std::endl;
        std::cout << "Min bounds: (" << minBounds.x << ", " << minBounds.y << ", " << minBounds.z << ")" << std::endl;
        std::cout << "Max bounds: (" << maxBounds.x << ", " << maxBounds.y << ", " << maxBounds.z << ")" << std::endl;
        std::cout << "Dimensions (W x H x D): " << dimensions.x << " x " << dimensions.y << " x " << dimensions.z << std::endl;
        std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
        std::cout << "=========================\n" << std::endl;
    }

    std::cout << "Loaded " << vertices.size() << " vertices, " << indices.size() << " indices" << std::endl;
    std::cout << "Created " << materials.size() << " material entries" << std::endl;

    // Create Vulkan buffers
    std::cout << "Creating vertex buffer..." << std::endl;
    createVertexBuffer(device, physicalDevice, commandPool, graphicsQueue);
    std::cout << "Creating index buffer..." << std::endl;
    createIndexBuffer(device, physicalDevice, commandPool, graphicsQueue);
    std::cout << "Model loading complete" << std::endl;
}

void Model::createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                               VkCommandPool commandPool, VkQueue graphicsQueue) {
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);

    // Copy vertex data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create device local buffer
    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                vertexBuffer, vertexBufferMemory);

    // Copy from staging to device local
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkCommandPool commandPool, VkQueue graphicsQueue) {
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);

    // Copy index data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create device local buffer
    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                indexBuffer, indexBufferMemory);

    // Copy from staging to device local
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, indexBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::createTextureImage(VkDevice device, VkPhysicalDevice physicalDevice,
                               VkCommandPool commandPool, VkQueue graphicsQueue,
                               const unsigned char* pixels, int width, int height, int channels,
                               Material& material) {
    VkDeviceSize imageSize = width * height * 4; // Always use RGBA

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, imageSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);

    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    if (channels == 4) {
        memcpy(data, pixels, imageSize);
    } else if (channels == 3) {
        // Convert RGB to RGBA
        unsigned char* rgba = static_cast<unsigned char*>(data);
        for (int i = 0; i < width * height; ++i) {
            rgba[i * 4 + 0] = pixels[i * 3 + 0];
            rgba[i * 4 + 1] = pixels[i * 3 + 1];
            rgba[i * 4 + 2] = pixels[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
    }
    vkUnmapMemory(device, stagingBufferMemory);

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &material.textureImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image");
    }

    // Allocate memory for image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, material.textureImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &material.textureImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate texture image memory");
    }

    vkBindImageMemory(device, material.textureImage, material.textureImageMemory, 0);

    // Transition image layout and copy buffer to image
    transitionImageLayout(device, commandPool, graphicsQueue, material.textureImage,
                         VK_FORMAT_R8G8B8A8_SRGB,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer, material.textureImage, width, height);

    transitionImageLayout(device, commandPool, graphicsQueue, material.textureImage,
                         VK_FORMAT_R8G8B8A8_SRGB,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::createTextureImageView(VkDevice device, Material& material) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = material.textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &material.textureImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view");
    }
}

void Model::createTextureSampler(VkDevice device, Material& material) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &material.textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
}

void Model::createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkDeviceSize size, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties,
                        VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Model::copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                      VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
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
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t Model::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                               VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void Model::transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                                 VkImage image, VkFormat format,
                                 VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Model::copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                             VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Model::loadExternalTexture(const std::string& filepath,
                                VkDevice device,
                                VkPhysicalDevice physicalDevice,
                                VkCommandPool commandPool,
                                VkQueue graphicsQueue,
                                VkImage& outImage,
                                VkDeviceMemory& outMemory,
                                VkImageView& outView,
                                VkSampler& outSampler) {
    // Load image using stb_image
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        std::cerr << "Warning: Failed to load external texture: " << filepath << std::endl;
        return; // Non-fatal, just skip texture
    }

    std::cout << "Loading external texture: " << filepath
              << " (" << texWidth << "x" << texHeight << ", " << texChannels << " channels)" << std::endl;

    // Create a temporary material to use existing infrastructure
    Material tempMaterial;
    createTextureImage(device, physicalDevice, commandPool, graphicsQueue,
                      pixels, texWidth, texHeight, 4, tempMaterial);
    createTextureImageView(device, tempMaterial);
    createTextureSampler(device, tempMaterial);

    // Transfer handles to output parameters
    outImage = tempMaterial.textureImage;
    outMemory = tempMaterial.textureImageMemory;
    outView = tempMaterial.textureImageView;
    outSampler = tempMaterial.textureSampler;

    // Clear temp material handles so they don't get destroyed
    tempMaterial.textureImage = VK_NULL_HANDLE;
    tempMaterial.textureImageMemory = VK_NULL_HANDLE;
    tempMaterial.textureImageView = VK_NULL_HANDLE;
    tempMaterial.textureSampler = VK_NULL_HANDLE;

    stbi_image_free(pixels);
}

std::string Model::resolveTexturePath(const std::string& modelPath,
                                       const std::string& textureUri) {
    // Get directory of model file
    std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();

    // Resolve relative URI (try model directory first)
    std::filesystem::path texturePath = modelDir / textureUri;

    if (std::filesystem::exists(texturePath)) {
        std::cout << "Found texture at: " << texturePath.string() << std::endl;
        return texturePath.string();
    }

    // Try alternate location: assets/textures/[model_name]/[texture_file]
    std::string modelName = std::filesystem::path(modelPath).stem().string();
    std::filesystem::path textureFilename = std::filesystem::path(textureUri).filename();
    std::filesystem::path altPath = std::filesystem::path("assets/textures") / modelName / textureFilename;

    if (std::filesystem::exists(altPath)) {
        std::cout << "Found texture at alternate location: " << altPath.string() << std::endl;
        return altPath.string();
    }

    // Return original path and let loading fail gracefully
    std::cerr << "Warning: Could not resolve texture path for: " << textureUri << std::endl;
    std::cerr << "  Tried: " << texturePath.string() << std::endl;
    std::cerr << "  Tried: " << altPath.string() << std::endl;
    return texturePath.string();
}

void Model::cleanup(VkDevice device) {
    // Clean up all materials
    for (auto& material : materials) {
        // Clean up base color texture
        if (material.textureSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, material.textureSampler, nullptr);
            material.textureSampler = VK_NULL_HANDLE;
        }
        if (material.textureImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, material.textureImageView, nullptr);
            material.textureImageView = VK_NULL_HANDLE;
        }
        if (material.textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, material.textureImage, nullptr);
            material.textureImage = VK_NULL_HANDLE;
        }
        if (material.textureImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, material.textureImageMemory, nullptr);
            material.textureImageMemory = VK_NULL_HANDLE;
        }

        // Clean up normal map
        if (material.normalMapSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, material.normalMapSampler, nullptr);
            material.normalMapSampler = VK_NULL_HANDLE;
        }
        if (material.normalMapView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, material.normalMapView, nullptr);
            material.normalMapView = VK_NULL_HANDLE;
        }
        if (material.normalMap != VK_NULL_HANDLE) {
            vkDestroyImage(device, material.normalMap, nullptr);
            material.normalMap = VK_NULL_HANDLE;
        }
        if (material.normalMapMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, material.normalMapMemory, nullptr);
            material.normalMapMemory = VK_NULL_HANDLE;
        }

        // Clean up metallic/roughness map
        if (material.metallicRoughnessSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, material.metallicRoughnessSampler, nullptr);
            material.metallicRoughnessSampler = VK_NULL_HANDLE;
        }
        if (material.metallicRoughnessView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, material.metallicRoughnessView, nullptr);
            material.metallicRoughnessView = VK_NULL_HANDLE;
        }
        if (material.metallicRoughnessMap != VK_NULL_HANDLE) {
            vkDestroyImage(device, material.metallicRoughnessMap, nullptr);
            material.metallicRoughnessMap = VK_NULL_HANDLE;
        }
        if (material.metallicRoughnessMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, material.metallicRoughnessMemory, nullptr);
            material.metallicRoughnessMemory = VK_NULL_HANDLE;
        }

        // Clean up emissive map
        if (material.emissiveMapSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, material.emissiveMapSampler, nullptr);
            material.emissiveMapSampler = VK_NULL_HANDLE;
        }
        if (material.emissiveMapView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, material.emissiveMapView, nullptr);
            material.emissiveMapView = VK_NULL_HANDLE;
        }
        if (material.emissiveMap != VK_NULL_HANDLE) {
            vkDestroyImage(device, material.emissiveMap, nullptr);
            material.emissiveMap = VK_NULL_HANDLE;
        }
        if (material.emissiveMapMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, material.emissiveMapMemory, nullptr);
            material.emissiveMapMemory = VK_NULL_HANDLE;
        }
    }
    materials.clear();

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

std::vector<std::string> Model::getMeshNames() const {
    std::vector<std::string> names;
    names.reserve(namedMeshes.size());
    for (const auto& mesh : namedMeshes) {
        names.push_back(mesh.name);
    }
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

} // namespace DownPour
