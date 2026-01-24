#include "Material.h"

#include <stb_image.h>

#include <iostream>
#include <stdexcept>

namespace DownPour {

MaterialManager::MaterialManager(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                 VkQueue graphicsQueue)
    : device(device),
      physicalDevice(physicalDevice),
      commandPool(commandPool),
      graphicsQueue(graphicsQueue),
      descriptorSetLayout(VK_NULL_HANDLE),
      descriptorPool(VK_NULL_HANDLE),
      maxFramesInFlight(0) {
    std::cout << "MaterialManager initialized\n";
}

MaterialManager::~MaterialManager() {
    // Cleanup is explicit via cleanup() call
}

uint32_t MaterialManager::createMaterial(const Material& material) {
    uint32_t id = nextMaterialId++;

    VulkanMaterialResources gpuResources;

    // Load base color texture if provided
    if (!material.baseColorTexture.empty())
        gpuResources.baseColor = loadTexture(material.baseColorTexture);

    // Load optional PBR textures
    if (!material.normalMapTexture.empty() && material.props.hasNormalMap)
        gpuResources.normalMap = loadTexture(material.normalMapTexture);

    if (!material.metallicRoughnessTexture.empty() && material.props.hasMetallicRoughness)
        gpuResources.metallicRoughness = loadTexture(material.metallicRoughnessTexture);

    if (!material.emissiveTexture.empty() && material.props.hasEmissive)
        gpuResources.emissive = loadTexture(material.emissiveTexture);

    // Create descriptor sets if descriptor support is initialized
    if (descriptorSetLayout != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE && maxFramesInFlight > 0) {
        std::vector<VkDescriptorSet> matDescriptorSets(maxFramesInFlight);

        // Allocate descriptor sets for all frames
        std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);
        VkDescriptorSetAllocateInfo        allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = descriptorPool;
        allocInfo.descriptorSetCount = maxFramesInFlight;
        allocInfo.pSetLayouts        = layouts.data();

        if (vkAllocateDescriptorSets(device, &allocInfo, matDescriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor sets for material");

        // Update all descriptor sets with texture bindings
        for (uint32_t frame = 0; frame < maxFramesInFlight; frame++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = gpuResources.baseColor.view;
            imageInfo.sampler     = gpuResources.baseColor.sampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet          = matDescriptorSets[frame];
            descriptorWrite.dstBinding      = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo      = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }

        gpuResources.descriptorSet = matDescriptorSets[0];
    }

    // Store resources and properties
    resources[id]  = gpuResources;
    properties[id] = material.props;

    return id;
}

void MaterialManager::initDescriptorSupport(VkDescriptorSetLayout layout, VkDescriptorPool pool,
                                            uint32_t maxFramesInFlight) {
    descriptorSetLayout     = layout;
    descriptorPool          = pool;
    this->maxFramesInFlight = maxFramesInFlight;
}

VkDescriptorSet MaterialManager::getDescriptorSet(uint32_t materialId, uint32_t frameIndex) const {
    auto it = resources.find(materialId);
    if (it == resources.end()) {
        throw std::runtime_error("Material ID " + std::to_string(materialId) + " not found");
    }
    return it->second.descriptorSet;
}

void MaterialManager::bindMaterial(uint32_t materialId, VkCommandBuffer cmd, VkPipelineLayout layout) {
    auto it = resources.find(materialId);
    if (it == resources.end())
        throw std::runtime_error("Material ID " + std::to_string(materialId) + " not found");

    const auto& res = it->second;
    if (res.descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &res.descriptorSet, 0, nullptr);
    }
}

const MaterialProperties& MaterialManager::getProperties(uint32_t materialId) const {
    auto it = properties.find(materialId);
    if (it == properties.end()) {
        throw std::runtime_error("Material ID " + std::to_string(materialId) + " not found");
    }
    return it->second;
}

void MaterialManager::cleanup() {
    for (auto& pair : resources) {
        auto& res = pair.second;
        destroyTextureHandle(res.baseColor);
        destroyTextureHandle(res.normalMap);
        destroyTextureHandle(res.metallicRoughness);
        destroyTextureHandle(res.emissive);
    }

    resources.clear();
    properties.clear();
}

// ============================================================================
// Private Helper Methods
// ============================================================================

TextureHandle MaterialManager::loadTexture(const std::string& path) {
    TextureHandle texture;

    int      texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        std::cerr << "Warning: Failed to load texture: " << path << "\n";
        return texture;  // Return invalid handle
    }

    try {
        createTextureImage(pixels, texWidth, texHeight, 4, texture);
        createTextureImageView(texture);
        createTextureSampler(texture);

        std::cout << "Loaded texture: " << path << " (" << texWidth << "x" << texHeight << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Error loading texture " << path << ": " << e.what() << "\n";
        destroyTextureHandle(texture);
    }

    stbi_image_free(pixels);
    return texture;
}

void MaterialManager::createTextureImage(const unsigned char* pixels, int width, int height, int channels,
                                         TextureHandle& outTexture) {
    VkDeviceSize imageSize = width * height * 4;  // Always RGBA

    // Create staging buffer
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = imageSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create staging buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    // Copy pixel data
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Create image
    createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                outTexture.image, outTexture.memory);

    // Transition and copy
    transitionImageLayout(outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(stagingBuffer, outTexture.image, width, height);

    transitionImageLayout(outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void MaterialManager::createTextureImageView(TextureHandle& texture) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = texture.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &texture.view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view");
}

void MaterialManager::createTextureSampler(TextureHandle& texture) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = 16.0f;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler");
}

void MaterialManager::destroyTextureHandle(TextureHandle& texture) {
    if (texture.sampler != VK_NULL_HANDLE)
        vkDestroySampler(device, texture.sampler, nullptr);
    if (texture.view != VK_NULL_HANDLE)
        vkDestroyImageView(device, texture.view, nullptr);
    if (texture.image != VK_NULL_HANDLE)
        vkDestroyImage(device, texture.image, nullptr);
    if (texture.memory != VK_NULL_HANDLE)
        vkFreeMemory(device, texture.memory, nullptr);
    texture.reset();
}

void MaterialManager::createImage(const uint32_t width, const uint32_t height, const VkFormat format,
                                  const VkImageTiling tiling, const VkImageUsageFlags usage,
                                  const VkMemoryPropertyFlags properties, const VkImage& image,
                                  const VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory");
    vkBindImageMemory(device, image, imageMemory, 0);
}

void MaterialManager::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                                            VkImageLayout newLayout) {
    // Create command buffer
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

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void MaterialManager::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
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

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t MaterialManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

}  // namespace DownPour
