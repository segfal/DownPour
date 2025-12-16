#pragma once
#include <vulkan/vulkan.h>

#include <array>
#include <glm/glm.hpp>

namespace DownPour {

struct Vertex {
    glm::vec3                                               position;
    glm::vec3                                               normal;
    glm::vec2                                               texCoord;
    static VkVertexInputBindingDescription                  getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

};  // namespace DownPour