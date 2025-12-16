// SPDX-License-Identifier: MIT
#pragma once

/**
 * @file Types.h
 * @brief Centralized type definitions for DownPour
 *
 * This file provides readable type aliases for commonly used GLM and Vulkan types.
 * Using these aliases improves code readability and makes type usage consistent
 * across the codebase.
 *
 * Usage:
 *   #include "core/Types.h"
 *   using namespace DownPour::Types;
 *
 *   Vec3 position(0.0f, 1.0f, 0.0f);
 *   Mat4 transform = glm::translate(Mat4(1.0f), position);
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace DownPour {
namespace Types {

// ============================================================================
// GLM Math Types
// ============================================================================

// --- Floating Point Vectors ---
using Vec2 = glm::vec2;  // 2D vector (x, y)
using Vec3 = glm::vec3;  // 3D vector (x, y, z)
using Vec4 = glm::vec4;  // 4D vector (x, y, z, w)

// --- Integer Vectors ---
using IVec2 = glm::ivec2;  // 2D integer vector
using IVec3 = glm::ivec3;  // 3D integer vector
using IVec4 = glm::ivec4;  // 4D integer vector

// --- Unsigned Integer Vectors ---
using UVec2 = glm::uvec2;  // 2D unsigned integer vector
using UVec3 = glm::uvec3;  // 3D unsigned integer vector
using UVec4 = glm::uvec4;  // 4D unsigned integer vector

// --- Matrices ---
using Mat2 = glm::mat2;  // 2x2 matrix
using Mat3 = glm::mat3;  // 3x3 matrix
using Mat4 = glm::mat4;  // 4x4 matrix (most common for transforms)

// --- Quaternions ---
using Quat = glm::quat;  // Quaternion for rotations

// ============================================================================
// Vulkan Handle Types
// ============================================================================

// --- Core Vulkan Objects ---
using Instance       = VkInstance;        // Vulkan instance
using PhysicalDevice = VkPhysicalDevice;  // GPU handle
using Device         = VkDevice;          // Logical device
using Queue          = VkQueue;           // Command queue
using Surface        = VkSurfaceKHR;      // Window surface

// --- Memory & Resources ---
using Buffer       = VkBuffer;        // GPU buffer (vertex, index, uniform)
using DeviceMemory = VkDeviceMemory;  // GPU memory allocation
using Image        = VkImage;         // GPU image (texture, framebuffer)
using ImageView    = VkImageView;     // Image view for shader access
using Sampler      = VkSampler;       // Texture sampler

// --- Pipeline & Rendering ---
using Pipeline       = VkPipeline;        // Graphics or compute pipeline
using PipelineLayout = VkPipelineLayout;  // Pipeline resource layout
using PipelineCache  = VkPipelineCache;   // Pipeline compilation cache
using RenderPass     = VkRenderPass;      // Render pass definition
using Framebuffer    = VkFramebuffer;     // Framebuffer for rendering
using ShaderModule   = VkShaderModule;    // Compiled shader
using Swapchain      = VkSwapchainKHR;    // Presentation swapchain
using SwapchainKHR   = VkSwapchainKHR;    // Explicit KHR naming

// --- Descriptors ---
using DescriptorSet       = VkDescriptorSet;        // Descriptor set instance
using DescriptorSetLayout = VkDescriptorSetLayout;  // Descriptor set layout
using DescriptorPool      = VkDescriptorPool;       // Descriptor pool

// --- Commands ---
using CommandBuffer = VkCommandBuffer;  // Command buffer for recording
using CommandPool   = VkCommandPool;    // Command buffer allocator

// --- Synchronization ---
using Semaphore = VkSemaphore;  // GPU-GPU synchronization
using Fence     = VkFence;      // GPU-CPU synchronization
using Event     = VkEvent;      // Fine-grained GPU synchronization

// ============================================================================
// Vulkan Enum/Struct Types (Common)
// ============================================================================

using Format             = VkFormat;              // Image/buffer format
using ColorSpaceKHR      = VkColorSpaceKHR;       // Color space
using PresentModeKHR     = VkPresentModeKHR;      // Presentation mode
using SharingMode        = VkSharingMode;         // Resource sharing
using ImageLayout        = VkImageLayout;         // Image memory layout
using AttachmentLoadOp   = VkAttachmentLoadOp;    // Render pass load operation
using PipelineStageFlags = VkPipelineStageFlags;  // Pipeline stage flags
using AccessFlags        = VkAccessFlags;         // Memory access flags

// ============================================================================
// Commonly Used Vulkan Structs
// ============================================================================

using Viewport        = VkViewport;         // Viewport definition
using Rect2D          = VkRect2D;           // 2D rectangle
using Offset2D        = VkOffset2D;         // 2D offset
using Extent2D        = VkExtent2D;         // 2D extent (width, height)
using Extent3D        = VkExtent3D;         // 3D extent
using ClearValue      = VkClearValue;       // Clear value for attachments
using ClearColorValue = VkClearColorValue;  // Clear color

// ============================================================================
// Commonly Used C++ Structs
// ============================================================================

using Vec3List = std::vector<Vec3>;
using Vec4List = std::vector<Vec4>;
using Mat4List = std::vector<Mat4>;

}  // namespace Types
}  // namespace DownPour
