#pragma once

#include <vulkan/vulkan.h>

#include <any>
#include <map>
#include <stdexcept>
#include <string>

namespace DownPour {
namespace Vulkan {

/**
 * @brief Core Vulkan context holding essential GPU resources
 *
 * This struct provides direct access to the most commonly used Vulkan handles.
 * These resources are required by most rendering operations and should be
 * initialized early in the application lifecycle.
 */
struct VulkanCoreContext {
    // Instance and device handles
    VkInstance       instance        = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice         device          = VK_NULL_HANDLE;

    // Surface for presentation
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Queue handles
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue  = VK_NULL_HANDLE;

    // Command and descriptor resources
    VkCommandPool    command_pool    = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    // Swapchain resources
    VkSwapchainKHR swapchain             = VK_NULL_HANDLE;
    VkFormat       swapchain_format      = VK_FORMAT_UNDEFINED;
    VkExtent2D     swapchain_extent      = {0, 0};
    VkRenderPass   render_pass           = VK_NULL_HANDLE;

    /**
     * @brief Check if core context is fully initialized
     * @return true if all essential handles are valid
     */
    bool isValid() const {
        return instance != VK_NULL_HANDLE && physical_device != VK_NULL_HANDLE && device != VK_NULL_HANDLE &&
               graphics_queue != VK_NULL_HANDLE && command_pool != VK_NULL_HANDLE;
    }
};

/**
 * @brief Centralized Vulkan resource dispatcher with hybrid access
 *
 * Provides both direct struct access for core resources and a flexible
 * map-based system for dynamic or optional resources. This design allows:
 *
 * - Fast, type-safe access to common resources via the `core` struct
 * - Dynamic registration of pipeline-specific or optional resources
 * - Easy passing of Vulkan context between subsystems
 *
 * Usage:
 *   VulkanDispatch dispatch;
 *   dispatch.core.device = myDevice;
 *   dispatch.set_resource("car_pipeline", myCarPipeline);
 *   auto pipeline = dispatch.get_resource<VkPipeline>("car_pipeline");
 */
class VulkanDispatch {
public:
    /**
     * @brief Core Vulkan context with essential resources
     *
     * Access pattern: dispatch.core.device, dispatch.core.graphics_queue, etc.
     */
    VulkanCoreContext core;

    /**
     * @brief Register a dynamic resource in the dispatch map
     *
     * Use snake_case for resource keys (e.g., "car_pipeline", "debug_vertex_buffer")
     *
     * @tparam T Resource type (automatically deduced)
     * @param key Unique identifier for the resource (snake_case)
     * @param resource The Vulkan resource to store
     */
    template <typename T>
    void set_resource(const std::string& key, T resource) {
        dynamic_resources[key] = resource;
    }

    /**
     * @brief Retrieve a dynamic resource from the dispatch map
     *
     * @tparam T Expected resource type
     * @param key Resource identifier
     * @return The requested resource
     * @throws std::runtime_error if resource not found or type mismatch
     */
    template <typename T>
    T get_resource(const std::string& key) const {
        auto it = dynamic_resources.find(key);
        if (it == dynamic_resources.end()) {
            throw std::runtime_error("VulkanDispatch: Resource '" + key + "' not found");
        }

        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            throw std::runtime_error("VulkanDispatch: Type mismatch for resource '" + key + "'");
        }
    }

    /**
     * @brief Check if a dynamic resource exists
     *
     * @param key Resource identifier
     * @return true if resource is registered
     */
    bool has_resource(const std::string& key) const { return dynamic_resources.find(key) != dynamic_resources.end(); }

    /**
     * @brief Remove a dynamic resource from the dispatch map
     *
     * @param key Resource identifier
     * @return true if resource was found and removed
     */
    bool remove_resource(const std::string& key) { return dynamic_resources.erase(key) > 0; }

    /**
     * @brief Get all registered dynamic resource keys
     *
     * Useful for debugging or inspecting what resources are currently registered
     *
     * @return Vector of all resource keys
     */
    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> keys;
        keys.reserve(dynamic_resources.size());
        for (const auto& pair : dynamic_resources) {
            keys.push_back(pair.first);
        }
        return keys;
    }

    /**
     * @brief Clear all dynamic resources
     *
     * Note: This does NOT destroy Vulkan handles, only removes them from the map.
     * Cleanup of actual Vulkan resources should be done separately.
     */
    void clear_dynamic_resources() { dynamic_resources.clear(); }

private:
    /**
     * @brief Map of dynamic resources using std::any for type flexibility
     *
     * Supports any Vulkan handle type (VkPipeline, VkBuffer, VkDescriptorSetLayout, etc.)
     * Keys should use snake_case for consistency (e.g., "car_pipeline", "world_descriptor_layout")
     */
    std::map<std::string, std::any> dynamic_resources;
};

}  // namespace Vulkan
}  // namespace DownPour
