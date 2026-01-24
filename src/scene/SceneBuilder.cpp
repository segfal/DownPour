// SPDX-License-Identifier: MIT
#include "SceneBuilder.h"

#include <glm/gtc/matrix_transform.hpp>

namespace DownPour {

std::vector<NodeHandle> SceneBuilder::buildFromModel(Scene*                                     scene,
                                                      const Model*                               model,
                                                      const std::unordered_map<size_t, uint32_t>& materialIds) {
    if (!scene || !model) {
        return {};
    }

    // If model has no hierarchy, return empty (legacy flat models)
    if (!model->hasHierarchy()) {
        return {};
    }

    const glTFScene&              gltfScene = model->getDefaultScene();
    const std::vector<glTFNode>&  nodes     = model->getNodes();
    std::vector<NodeHandle>        rootHandles;

    // Create nodes for each root in the scene
    for (int rootIndex : gltfScene.rootNodes) {
        if (rootIndex >= 0 && rootIndex < static_cast<int>(nodes.size())) {
            NodeHandle rootHandle = createNodeRecursive(scene, model, rootIndex, NodeHandle{}, materialIds);
            if (rootHandle.isValid()) {
                rootHandles.push_back(rootHandle);
            }
        }
    }

    return rootHandles;
}

NodeHandle SceneBuilder::createNodeRecursive(Scene*                                     scene,
                                              const Model*                               model,
                                              int                                        nodeIndex,
                                              NodeHandle                                 parentHandle,
                                              const std::unordered_map<size_t, uint32_t>& materialIds) {
    const std::vector<glTFNode>& gltfNodes = model->getNodes();
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(gltfNodes.size())) {
        return NodeHandle{};
    }

    const glTFNode& gltfNode = gltfNodes[nodeIndex];

    // Create scene node
    NodeHandle handle = scene->createNode(gltfNode.name, parentHandle);
    SceneNode* node   = scene->getNode(handle);
    if (!node) {
        return NodeHandle{};
    }

    // Set transform from glTF node
    if (gltfNode.matrix != glm::mat4(1.0f)) {
        // Node uses matrix representation - decompose into TRS
        node->setLocalTransform(gltfNode.matrix);
    } else {
        // Node uses TRS representation
        node->localPosition = gltfNode.translation;
        node->localRotation = gltfNode.rotation;
        node->localScale    = gltfNode.scale;
    }

    node->isDirty = true;  // Needs transform update

    // Attach render data if node has a mesh
    if (gltfNode.meshIndex >= 0) {
        // Find materials that belong to this specific mesh
        auto meshMaterials = model->getMaterialsForMesh(gltfNode.meshIndex);

        if (!meshMaterials.empty()) {
            // Use the first primitive's material (TODO: handle multi-primitive meshes)
            const auto& [matIdx, material] = meshMaterials[0];

            // Create render data
            SceneNode::RenderData renderData;
            renderData.model          = model;
            renderData.meshIndex      = static_cast<uint32_t>(gltfNode.meshIndex);
            renderData.primitiveIndex = static_cast<uint32_t>(material->primitiveIndex);
            renderData.indexStart     = material->indexStart;
            renderData.indexCount     = material->indexCount;
            renderData.isVisible      = true;
            renderData.isTransparent  = material->props.isTransparent;

            // Get MaterialManager ID from the mapping
            auto it = materialIds.find(matIdx);
            if (it != materialIds.end()) {
                renderData.materialId = it->second;
            } else {
                // Material not in GPU mapping - skip this node's render data
                renderData.materialId = 0;  // Fallback (may cause issues)
            }

            node->renderData = renderData;
        }
        // If no materials found for this mesh, node has no render data (invisible)
    }

    // Recursively create children
    for (int childIndex : gltfNode.children) {
        createNodeRecursive(scene, model, childIndex, handle, materialIds);
    }

    return handle;
}

}  // namespace DownPour
