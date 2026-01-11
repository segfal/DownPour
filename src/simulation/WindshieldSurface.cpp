// SPDX-License-Identifier: MIT
#include "WindshieldSurface.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace DownPour {
namespace Simulation {

void WindshieldSurface::initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                   VkCommandPool commandPool, VkQueue graphicsQueue) {
    std::cout << "Initializing windshield surface..." << std::endl;
    
    createWetnessMap(device, physicalDevice, commandPool, graphicsQueue);
    createFlowMap(device, physicalDevice, commandPool, graphicsQueue);
    
    std::cout << "Windshield surface initialized" << std::endl;
}

void WindshieldSurface::cleanup(VkDevice device) {
    if (wetnessMapView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, wetnessMapView, nullptr);
        wetnessMapView = VK_NULL_HANDLE;
    }
    if (wetnessMap != VK_NULL_HANDLE) {
        vkDestroyImage(device, wetnessMap, nullptr);
        wetnessMap = VK_NULL_HANDLE;
    }
    if (wetnessMapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, wetnessMapMemory, nullptr);
        wetnessMapMemory = VK_NULL_HANDLE;
    }
    
    if (flowMapView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, flowMapView, nullptr);
        flowMapView = VK_NULL_HANDLE;
    }
    if (flowMap != VK_NULL_HANDLE) {
        vkDestroyImage(device, flowMap, nullptr);
        flowMap = VK_NULL_HANDLE;
    }
    if (flowMapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, flowMapMemory, nullptr);
        flowMapMemory = VK_NULL_HANDLE;
    }
}

void WindshieldSurface::update(float deltaTime, const std::vector<Raindrop>& raindrops) {
    updateWiper(deltaTime);
    updateWetness(raindrops);
}

void WindshieldSurface::updateWiper(float deltaTime) {
    if (!wiperActive) return;
    
    float movement = wiperSpeed * deltaTime;
    if (wiperDirection) {
        wiperAngle += movement;
        if (wiperAngle >= 45.0f) {
            wiperAngle = 45.0f;
            wiperDirection = false;
        }
    } else {
        wiperAngle -= movement;
        if (wiperAngle <= -45.0f) {
            wiperAngle = -45.0f;
            wiperDirection = true;
        }
    }
}

void WindshieldSurface::updateWetness(const std::vector<Raindrop>& raindrops) {
    // TODO: Update wetness map based on raindrop impacts
    // This would involve CPU-side updates or compute shader updates
    // For now, this is a placeholder
    (void)raindrops;
}

void WindshieldSurface::createWetnessMap(VkDevice device, VkPhysicalDevice physicalDevice,
                                         VkCommandPool commandPool, VkQueue graphicsQueue) {
    // Create a simple 256x256 R8 texture for wetness
    const uint32_t width = 256;
    const uint32_t height = 256;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device, &imageInfo, nullptr, &wetnessMap) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create wetness map image");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, wetnessMap, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &wetnessMapMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate wetness map memory");
    }
    
    vkBindImageMemory(device, wetnessMap, wetnessMapMemory, 0);
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = wetnessMap;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device, &viewInfo, nullptr, &wetnessMapView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create wetness map image view");
    }
    
    // TODO: Initialize with zero wetness (dry windshield)
    // This requires transitioning the image layout from UNDEFINED to TRANSFER_DST_OPTIMAL,
    // clearing the image, then transitioning to SHADER_READ_ONLY_OPTIMAL.
    // Implementation deferred until build environment is available for testing.
    (void)commandPool;
    (void)graphicsQueue;
}

void WindshieldSurface::createFlowMap(VkDevice device, VkPhysicalDevice physicalDevice,
                                      VkCommandPool commandPool, VkQueue graphicsQueue) {
    // Create a simple 256x256 RG8 texture for flow directions
    const uint32_t width = 256;
    const uint32_t height = 256;
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device, &imageInfo, nullptr, &flowMap) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create flow map image");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, flowMap, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &flowMapMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate flow map memory");
    }
    
    vkBindImageMemory(device, flowMap, flowMapMemory, 0);
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = flowMap;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device, &viewInfo, nullptr, &flowMapView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create flow map image view");
    }
    
    // TODO: Initialize with downward flow (gravity)
    // This requires transitioning the image layout from UNDEFINED to TRANSFER_DST_OPTIMAL,
    // filling with flow data, then transitioning to SHADER_READ_ONLY_OPTIMAL.
    // Implementation deferred until build environment is available for testing.
    (void)commandPool;
    (void)graphicsQueue;
}

uint32_t WindshieldSurface::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
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

} // namespace Simulation
} // namespace DownPour
