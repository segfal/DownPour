// SPDX-License-Identifier: MIT
#pragma once

#include "core/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace DownPour {

// Use centralized type aliases
using namespace DownPour::Types;
typedef std::string str;
typedef uint32_t    uint;

// Forward declarations
class Model;

/**
 * @brief Handle for referencing SceneNodes (index-based for cache efficiency)
 *
 * Uses index + generation to detect stale handles after node deletion.
 */
struct NodeHandle {
    uint index      = INVALID_INDEX;
    uint generation = 0;

    bool isValid() const { return index != INVALID_INDEX; }

    bool operator==(const NodeHandle& other) const { return index == other.index && generation == other.generation; }

    bool operator!=(const NodeHandle& other) const { return !(*this == other); }

    static constexpr uint INVALID_INDEX = 0xFFFFFFFF;
};

typedef std::vector<NodeHandle> NodeHandleList;

/**
 * @brief Node in the scene graph hierarchy
 *
 * Stores local and world transforms, parent-child relationships,
 * and references to renderable geometry. Uses flat index-based handles
 * instead of pointers for cache efficiency and stability.
 */
struct SceneNode {
    // Identification
    str  name;
    uint generation = 0;

    // Hierarchy (stored as flat indices, not pointers)
    NodeHandle     parent;
    NodeHandleList children;

    // Transform (stored as TRS for easy animation)
    Vec3 localPosition = Vec3(0.0f);
    Quat localRotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
    Vec3 localScale    = Vec3(1.0f);

    // Cached world transform (updated during transform propagation)
    Mat4 worldTransform = Mat4(1.0f);
    bool isDirty        = true;  // Needs transform update

    // Rendering data (optional - not all nodes have meshes)
    struct RenderData {
        const Model* model = nullptr;  // Reference to geometry
        uint         meshIndex;        // Which mesh in the glTF
        uint         primitiveIndex;   // Which primitive within the mesh
        uint         materialId;       // MaterialManager ID
        bool         isVisible     = true;
        bool         isTransparent = false;

        // Index range for rendering this specific primitive
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
    };
    typedef std::optional<RenderData> RenderDataOpt;
    RenderDataOpt                     renderData;

    // Bounding volume (for frustum culling, future optimization)
    Vec3 boundsMin = Vec3(0.0f);
    Vec3 boundsMax = Vec3(0.0f);

    // Flags
    bool isStatic = true;  // Static nodes can skip transform updates after first frame

    // Helper methods
    Mat4 getLocalTransform() const;
    void setLocalTransform(const Mat4& transform);
    void setLocalPosition(const Vec3& pos);
    void setLocalRotation(const Quat& rot);
    void setLocalScale(const Vec3& scale);
};

}  // namespace DownPour
