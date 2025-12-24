#pragma once

#include <optional>
#include <cstdint>

namespace DownPour {
namespace Vulkan {

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
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

} // namespace Vulkan
} // namespace DownPour
