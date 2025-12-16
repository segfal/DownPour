#include "vulkan_detector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <vulkan/vulkan.h>
#else
#include <vulkan/vulkan.h>
#endif

VulkanBackend detect_vulkan_backend_impl(void) {
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result;

    // Create minimal Vulkan instance
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "SystemMonitor";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

#ifdef __APPLE__
    // MoltenVK portability extension
    const char* extensions[] = {"VK_KHR_portability_enumeration"};
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    result = vkCreateInstance(&create_info, NULL, &instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[Vulkan Detector] Failed to create Vulkan instance: %d\n", result);
        return BACKEND_UNKNOWN;
    }

    // Query physical devices
    uint32_t device_count = 0;
    result = vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    if (result != VK_SUCCESS || device_count == 0) {
        fprintf(stderr, "[Vulkan Detector] No Vulkan devices found\n");
        vkDestroyInstance(instance, NULL);
        return BACKEND_UNKNOWN;
    }

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    if (!devices) {
        vkDestroyInstance(instance, NULL);
        return BACKEND_UNKNOWN;
    }

    result = vkEnumeratePhysicalDevices(instance, &device_count, devices);
    if (result != VK_SUCCESS) {
        free(devices);
        vkDestroyInstance(instance, NULL);
        return BACKEND_UNKNOWN;
    }

    // Check first device properties
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(devices[0], &props);

    VulkanBackend backend = BACKEND_UNKNOWN;

    // Determine backend type from device type
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            backend = BACKEND_GPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            backend = BACKEND_CPU;
            break;
        default:
            backend = BACKEND_UNKNOWN;
            break;
    }

    // Cleanup
    free(devices);
    vkDestroyInstance(instance, NULL);

    return backend;
}
