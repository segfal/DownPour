#pragma once

#include "core/Types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace DownPour {
namespace Vulkan {

// Import commonly used types from Types namespace
using namespace DownPour::Types;

/**
 * @brief Helper struct to store queue family indices
 *
 * Stores indices for graphics and present queue families.
 * A complete set of indices is required for Vulkan device creation.
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    /**
     * @brief Check if all required queue families are found
     * @return true if both graphics and present families have values
     */
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

/**
 * @brief Helper struct to store swap chain support details
 *
 * Contains information about surface capabilities, formats, and present modes
 * required for swap chain creation.
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

}  // namespace Vulkan
}  // namespace DownPour
