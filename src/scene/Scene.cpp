// SPDX-License-Identifier: MIT
#include "Scene.h"

#include <algorithm>
#include <queue>

namespace DownPour {

Scene::Scene(const std::string& name) : name(name) {}

NodeHandle Scene::createNode(const std::string& nodeName) {
    uint32_t index = allocateNodeSlot();

    SceneNode& node  = nodes[index];
    node.name        = nodeName;
    node.generation  = node.generation + 1;  // Increment generation for new node
    node.parent      = NodeHandle{};         // No parent (invalid handle)
    node.children.clear();
    node.localPosition = glm::vec3(0.0f);
    node.localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    node.localScale    = glm::vec3(1.0f);
    node.worldTransform = glm::mat4(1.0f);
    node.isDirty       = true;
    node.renderData    = std::nullopt;
    node.isStatic      = true;

    NodeHandle handle{index, node.generation};

    // Add to root nodes (no parent)
    rootNodes.push_back(handle);

    // Add to active nodes list
    activeNodes.push_back(handle);

    // Add to name lookup
    nameToHandle[nodeName] = handle;

    return handle;
}

NodeHandle Scene::createNode(const std::string& nodeName, NodeHandle parent) {
    NodeHandle handle = createNode(nodeName);

    if (parent.isValid()) {
        setParent(handle, parent);
    }

    return handle;
}

void Scene::destroyNode(NodeHandle handle) {
    if (!isHandleValid(handle))
        return;

    SceneNode* node = getNode(handle);
    if (!node)
        return;

    // Remove from parent's children list
    if (node->parent.isValid()) {
        SceneNode* parentNode = getNode(node->parent);
        if (parentNode) {
            auto& children = parentNode->children;
            children.erase(std::remove(children.begin(), children.end(), handle), children.end());
        }
    } else {
        // Remove from root nodes
        rootNodes.erase(std::remove(rootNodes.begin(), rootNodes.end(), handle), rootNodes.end());
    }

    // Recursively destroy children
    std::vector<NodeHandle> childrenCopy = node->children;  // Copy because destroyNode modifies the list
    for (NodeHandle child : childrenCopy) {
        destroyNode(child);
    }

    // Remove from name lookup
    auto it = nameToHandle.find(node->name);
    if (it != nameToHandle.end() && it->second == handle) {
        nameToHandle.erase(it);
    }

    // Remove from active nodes list
    activeNodes.erase(std::remove(activeNodes.begin(), activeNodes.end(), handle), activeNodes.end());

    // Free the slot
    freeNodeSlot(handle.index);
}

SceneNode* Scene::getNode(NodeHandle handle) {
    if (!isHandleValid(handle))
        return nullptr;
    return &nodes[handle.index];
}

const SceneNode* Scene::getNode(NodeHandle handle) const {
    if (!isHandleValid(handle))
        return nullptr;
    return &nodes[handle.index];
}

NodeHandle Scene::findNode(const std::string& nodeName) const {
    auto it = nameToHandle.find(nodeName);
    if (it != nameToHandle.end()) {
        return it->second;
    }
    return NodeHandle{};  // Invalid handle
}

std::vector<NodeHandle> Scene::findNodesWithPrefix(const std::string& prefix) const {
    std::vector<NodeHandle> results;
    for (const auto& [name, handle] : nameToHandle) {
        if (name.find(prefix) == 0) {  // Starts with prefix
            results.push_back(handle);
        }
    }
    return results;
}

void Scene::setParent(NodeHandle child, NodeHandle parent) {
    if (!isHandleValid(child))
        return;

    SceneNode* childNode = getNode(child);
    if (!childNode)
        return;

    // Remove from old parent
    if (childNode->parent.isValid()) {
        SceneNode* oldParent = getNode(childNode->parent);
        if (oldParent) {
            auto& children = oldParent->children;
            children.erase(std::remove(children.begin(), children.end(), child), children.end());
        }
    } else {
        // Remove from root nodes
        rootNodes.erase(std::remove(rootNodes.begin(), rootNodes.end(), child), rootNodes.end());
    }

    // Set new parent
    childNode->parent = parent;

    if (parent.isValid()) {
        SceneNode* parentNode = getNode(parent);
        if (parentNode) {
            parentNode->children.push_back(child);
        }
    } else {
        // No parent means it's a root node
        rootNodes.push_back(child);
    }

    // Mark child subtree as dirty
    markSubtreeDirty(child);
}

void Scene::removeParent(NodeHandle child) {
    setParent(child, NodeHandle{});  // Set to invalid handle (root)
}

void Scene::updateTransforms() {
    if (rootNodes.empty())
        return;

    // Iterative BFS to avoid deep recursion
    std::queue<std::pair<NodeHandle, glm::mat4>> queue;

    // Start with root nodes (identity parent transform)
    for (NodeHandle rootHandle : rootNodes) {
        queue.push({rootHandle, glm::mat4(1.0f)});
    }

    while (!queue.empty()) {
        auto [handle, parentWorld] = queue.front();
        queue.pop();

        SceneNode* node = getNode(handle);
        if (!node)
            continue;

        // Skip static nodes that are already clean
        if (node->isStatic && !node->isDirty)
            continue;

        // Compute world transform: parent * local
        glm::mat4 localTransform = node->getLocalTransform();
        node->worldTransform     = parentWorld * localTransform;
        node->isDirty            = false;

        // Queue children with this node's world transform
        for (NodeHandle childHandle : node->children) {
            queue.push({childHandle, node->worldTransform});
        }
    }
}

void Scene::markDirty(NodeHandle handle) {
    if (!isHandleValid(handle))
        return;

    SceneNode* node = getNode(handle);
    if (node) {
        node->isDirty = true;
    }
}

void Scene::markSubtreeDirty(NodeHandle handle) {
    if (!isHandleValid(handle))
        return;

    // Iterative BFS to mark node and all descendants as dirty
    std::queue<NodeHandle> queue;
    queue.push(handle);

    while (!queue.empty()) {
        NodeHandle current = queue.front();
        queue.pop();

        SceneNode* node = getNode(current);
        if (!node)
            continue;

        node->isDirty = true;

        // Queue all children
        for (NodeHandle child : node->children) {
            queue.push(child);
        }
    }
}

std::vector<Scene::RenderBatch> Scene::getRenderBatches() const {
    // Group nodes by (Model*, isTransparent) for efficient rendering
    std::unordered_map<const Model*, std::vector<SceneNode*>> opaqueBatches;
    std::unordered_map<const Model*, std::vector<SceneNode*>> transparentBatches;

    // Collect all ACTIVE nodes with render data (skips freed nodes)
    for (const NodeHandle& handle : activeNodes) {
        const SceneNode* node = getNode(handle);
        if (!node)
            continue;
        
        if (!node->renderData || !node->renderData->isVisible)
            continue;

        // CRITICAL: Check if model pointer is valid
        const Model* model = node->renderData->model;
        if (!model) {
            continue;  // Skip nodes with null model pointer
        }

        bool isTransparent = node->renderData->isTransparent;

        if (isTransparent) {
            transparentBatches[model].push_back(const_cast<SceneNode*>(node));
        } else {
            opaqueBatches[model].push_back(const_cast<SceneNode*>(node));
        }
    }

    // Build render batches: opaque first, then transparent
    std::vector<RenderBatch> batches;

    for (const auto& [model, nodeList] : opaqueBatches) {
        batches.push_back(RenderBatch{model, nodeList, false});
    }

    for (const auto& [model, nodeList] : transparentBatches) {
        batches.push_back(RenderBatch{model, nodeList, true});
    }

    return batches;
}

void Scene::collectVisibleNodes(const glm::mat4& viewProj, std::vector<SceneNode*>& outNodes) const {
    // TODO: Frustum culling implementation
    // For now, collect all ACTIVE nodes with render data (skips freed nodes)
    for (const NodeHandle& handle : activeNodes) {
        const SceneNode* node = getNode(handle);
        if (node && node->renderData && node->renderData->isVisible) {
            outNodes.push_back(const_cast<SceneNode*>(node));
        }
    }
}

void Scene::clear() {
    nodes.clear();
    freeList.clear();
    rootNodes.clear();
    activeNodes.clear();
    nameToHandle.clear();
}

bool Scene::isHandleValid(NodeHandle handle) const {
    if (!handle.isValid())
        return false;
    if (handle.index >= nodes.size())
        return false;
    return nodes[handle.index].generation == handle.generation;
}

uint32_t Scene::allocateNodeSlot() {
    if (!freeList.empty()) {
        // Reuse a freed slot
        uint32_t index = freeList.back();
        freeList.pop_back();
        return index;
    } else {
        // Allocate a new slot
        uint32_t index = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back();
        return index;
    }
}

void Scene::freeNodeSlot(uint32_t index) {
    if (index >= nodes.size())
        return;

    // Increment generation to invalidate old handles
    nodes[index].generation++;

    // Add to free list for reuse
    freeList.push_back(index);
}

}  // namespace DownPour
