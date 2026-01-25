// SPDX-License-Identifier: MIT
#pragma once

#include "core/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace DownPour {

// Use centralized type aliases
using namespace DownPour::Types;

// Forward declarations
class Model;

/**
 * @brief Handle for referencing SceneNodes (index-based for cache efficiency)
 *
 * Uses index + generation to detect stale handles after node deletion.
 */
struct NodeHandle {
    uint32_t index      = INVALID_INDEX;
    uint32_t generation = 0;

    bool isValid() const { return index != INVALID_INDEX; }

    bool operator==(const NodeHandle& other) const { return index == other.index && generation == other.generation; }

    bool operator!=(const NodeHandle& other) const { return !(*this == other); }

    static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
};

/**
 * @brief Node in the scene graph hierarchy
 *
 * Stores local and world transforms, parent-child relationships,
 * and references to renderable geometry. Uses flat index-based handles
 * instead of pointers for cache efficiency and stability.
 */
struct SceneNode {
    // Identification
    std::string name;
    uint32_t    generation = 0;

    // Hierarchy (stored as flat indices, not pointers)
    NodeHandle              parent;
    std::vector<NodeHandle> children;

    // Transform (stored as TRS for easy animation)
    glm::vec3 localPosition = glm::vec3(0.0f);
    glm::quat localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
    glm::vec3 localScale    = glm::vec3(1.0f);

    // Cached world transform (updated during transform propagation)
    glm::mat4 worldTransform = glm::mat4(1.0f);
    bool      isDirty        = true;  // Needs transform update

    // Rendering data (optional - not all nodes have meshes)
    struct RenderData {
        const Model* model = nullptr;  // Reference to geometry
        uint32_t     meshIndex;        // Which mesh in the glTF
        uint32_t     primitiveIndex;   // Which primitive within the mesh
        uint32_t     materialId;       // MaterialManager ID
        bool         isVisible     = true;
        bool         isTransparent = false;

        // Index range for rendering this specific primitive
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
    };
    std::optional<RenderData> renderData;

    // Bounding volume (for frustum culling, future optimization)
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);

    // Flags
    bool isStatic = true;  // Static nodes can skip transform updates after first frame

    // Helper methods
    glm::mat4 getLocalTransform() const;
    void      setLocalTransform(const glm::mat4& transform);
    void      setLocalPosition(const glm::vec3& pos);
    void      setLocalRotation(const glm::quat& rot);
    void      setLocalScale(const glm::vec3& scale);
};

}  // namespace DownPour
