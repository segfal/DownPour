// SPDX-License-Identifier: MIT
#pragma once

#include "Scene.h"
#include "renderer/Model.h"

#include <unordered_map>
#include <vector>

namespace DownPour {

/**
 * @brief Utility for populating scenes from loaded models
 *
 * Converts a Model's glTF hierarchy into a scene graph of SceneNodes.
 */
class SceneBuilder {
public:
    /**
     * @brief Create scene graph from glTF model hierarchy
     *
     * Traverses the model's node hierarchy and creates corresponding SceneNodes
     * with proper parent-child relationships and transforms. Attaches rendering
     * data for nodes that reference meshes.
     *
     * @param scene Target scene to populate
     * @param model Source model with glTF hierarchy
     * @param materialIds Map from material index to MaterialManager ID
     * @return Root node handle of created hierarchy (or multiple roots if scene has multiple)
     */
    static std::vector<NodeHandle> buildFromModel(Scene*                                     scene,
                                                   const Model*                               model,
                                                   const std::unordered_map<size_t, uint32_t>& materialIds);

private:
    /**
     * @brief Recursively create scene nodes from glTF hierarchy
     *
     * @param scene Target scene
     * @param model Source model
     * @param nodeIndex glTF node index to process
     * @param parentHandle Parent SceneNode handle (invalid for root nodes)
     * @param materialIds Material ID mapping
     * @return Created SceneNode handle
     */
    static NodeHandle createNodeRecursive(Scene*                                     scene,
                                          const Model*                               model,
                                          int                                        nodeIndex,
                                          NodeHandle                                 parentHandle,
                                          const std::unordered_map<size_t, uint32_t>& materialIds);
};

}  // namespace DownPour
