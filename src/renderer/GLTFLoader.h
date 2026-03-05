#pragma once

#include <string>

namespace DownPour {

class Model;

/**
 * @brief Utility for loading GLTF/GLB files into Model data structures
 *
 * Static class that parses GLTF files using tinygltf and populates Model instances.
 * Separates file I/O and parsing logic from the Model data container.
 */
class GLTFLoader {
public:
    /**
     * @brief Load GLTF/GLB file into Model
     *
     * Parses the file and populates the model with vertices, indices, materials,
     * hierarchy, and metadata. Does not create Vulkan buffers - that's handled
     * by Model::loadFromFile() after this returns.
     *
     * @param filepath Path to .gltf or .glb file
     * @param outModel Model to populate with loaded data
     * @return true on success, false on failure
     */
    static bool load(const std::string& filepath, Model& outModel);

private:
    // Helper to resolve texture paths (external or embedded)
    static std::string resolveTexturePath(const std::string& modelPath, const std::string& textureUri);

    // Helper to process GLTF texture references
    static void processGLTFTexture(const std::string& filepath, const void* modelPtr, int textureIndex,
                                   std::string& outPath, struct EmbeddedTexture& outEmbedded, bool& outHasFlag);
};

}  // namespace DownPour
