#pragma once

#include "monitor.h"

/**
 * @brief Detect whether Vulkan is using GPU or CPU backend
 *
 * Initializes a minimal Vulkan instance, queries physical devices,
 * and checks device type to determine hardware acceleration.
 *
 * @return VulkanBackend type (GPU, CPU, or UNKNOWN)
 */
VulkanBackend detect_vulkan_backend_impl(void);
