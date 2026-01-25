// SPDX-License-Identifier: MIT
#pragma once

#include "SceneNode.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace DownPour {

/**
 * @brief Scene graph with hierarchical transforms and spatial organization
 *
 * Manages a collection of SceneNodes with parent-child relationships.
 * Provides efficient transform propagation and rendering traversal.
 * Uses flat array storage for cache efficiency.
 */
class Scene {
public:
    /**
     * @brief Construct a new Scene
     * @param name Scene name (e.g., "menu", "garage", "driving")
     */
    Scene(const std::string& name);
    ~Scene() = default;

    // Disable copy (scenes manage unique resources)
    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;

    // Node creation and management
    NodeHandle createNode(const std::string& name);
    NodeHandle createNode(const std::string& name, NodeHandle parent);
    void       destroyNode(NodeHandle handle);

    SceneNode*              getNode(NodeHandle handle);
    const SceneNode*        getNode(NodeHandle handle) const;
    NodeHandle              findNode(const std::string& name) const;
    std::vector<NodeHandle> findNodesWithPrefix(const std::string& prefix) const;

    // Hierarchy manipulation
    void                    setParent(NodeHandle child, NodeHandle parent);
    void                    removeParent(NodeHandle child);  // Make node a root
    std::vector<NodeHandle> getRootNodes() const { return rootNodes; }

    // Transform propagation
    void updateTransforms();                   // Update all dirty transforms
    void markDirty(NodeHandle handle);         // Mark node as dirty
    void markSubtreeDirty(NodeHandle handle);  // Mark node and all descendants as dirty

    // Rendering support
    struct RenderBatch {
        const Model*            model;
        std::vector<SceneNode*> nodes;  // All nodes sharing this model
        bool                    isTransparent;
    };

    std::vector<RenderBatch> getRenderBatches() const;
    void                     collectVisibleNodes(const glm::mat4& viewProj, std::vector<SceneNode*>& outNodes) const;

    // Scene management
    void               clear();
    const std::string& getName() const { return name; }
    size_t             getNodeCount() const { return nodes.size() - freeList.size(); }

    // Serialization (future)
    // void saveToFile(const std::string& filepath) const;
    // void loadFromFile(const std::string& filepath);

private:
    std::string name;

    // Flat array of nodes (better cache performance than tree of pointers)
    std::vector<SceneNode> nodes;
    std::vector<uint32_t>  freeList;  // Indices of deleted nodes for reuse

    // Root nodes (nodes with no parent)
    std::vector<NodeHandle> rootNodes;

    // Active nodes (excludes nodes in free list) - for efficient iteration
    std::vector<NodeHandle> activeNodes;

    // Name lookup cache
    std::unordered_map<std::string, NodeHandle> nameToHandle;

    // Helper methods
    void     propagateTransform(NodeHandle handle, const glm::mat4& parentWorld);
    bool     isHandleValid(NodeHandle handle) const;
    uint32_t allocateNodeSlot();
    void     freeNodeSlot(uint32_t index);
};

}  // namespace DownPour
