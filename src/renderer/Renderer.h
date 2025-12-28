#pragma once
#include <vulkan/vulkan.h>


class Renderer {
public:
    Renderer(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent);
    ~Renderer();
    void drawFrame();
private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkExtent2D extent;
    // Additional Vulkan resources like pipelines, command buffers, etc.

};