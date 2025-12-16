#pragma once

#include <glm/glm.hpp>
#include <string>

/**
 * @brief Named mesh structure holding mesh name and index range
 *
 * A glTF mesh can contain multiple primitives. This struct stores
 * the index range for a specific primitive within a mesh.
 */
struct NamedMesh {
    std::string name;            // Mesh name from glTF (geometry name)
    std::string nodeName;        // Node name from glTF (scene object name)
    uint32_t    meshIndex;       // Which mesh in the glTF (for node lookup)
    uint32_t    primitiveIndex;  // Which primitive within the mesh (0, 1, 2...)
    uint32_t    indexStart;      // Starting index in the index buffer
    uint32_t    indexCount;      // Number of indices for this primitive
    glm::mat4   transform;       // Optional transform (defaults to identity)

    glm::vec3 minBounds;
    glm::vec3 maxBounds;

    NamedMesh()
        : meshIndex(0),
          primitiveIndex(0),
          indexStart(0),
          indexCount(0),
          transform(glm::mat4(1.0f)),
          minBounds(0.0f),
          maxBounds(0.0f) {}
};
