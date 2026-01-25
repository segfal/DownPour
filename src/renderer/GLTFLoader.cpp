#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "GLTFLoader.h"
#include "Model.h"
#include "logger/Logger.h"

#include <tiny_gltf.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace DownPour {

bool GLTFLoader::load(const std::string& filepath, Model& outModel) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model    model;
    std::string        err, warn;

    bool ret = false;
    if (filepath.substr(filepath.find_last_of(".") + 1) == "glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }

    // Check for loading errors
    Log logger;

    if (!warn.empty()) {
        logger.log("warning", "glTF Warning: " + warn);
    }

    if (!err.empty()) {
        logger.log("error", "glTF Error: " + err);
    }

    if (!ret) {
        logger.log("fatal", "Failed to load glTF model: " + filepath);
        return false;
    }

    logger.log("info", "Successfully loaded glTF model: " + filepath);
    logger.log("info", "  Nodes: " + std::to_string(model.nodes.size()));
    logger.log("info", "  Meshes: " + std::to_string(model.meshes.size()));
    logger.log("info", "  Materials: " + std::to_string(model.materials.size()));

    // Process each mesh in the model
    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
        const auto& mesh = model.meshes[meshIdx];
        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); primIdx++) {
            const auto& primitive = mesh.primitives[primIdx];
            // Track index start for this primitive
            uint32_t primitiveIndexStart = static_cast<uint32_t>(outModel.indices.size());

            // Set up bounds tracking for this primitive
            glm::vec3 primMin(0.0f), primMax(0.0f);
            NamedMesh namedMesh;

            // Get accessors
            const tinygltf::Accessor&   posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView     = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer&     posBuffer   = model.buffers[posView.buffer];

            // Get normals if available
            const tinygltf::Accessor*   normalAccessor = nullptr;
            const tinygltf::BufferView* normalView     = nullptr;
            const tinygltf::Buffer*     normalBuffer   = nullptr;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
                normalView     = &model.bufferViews[normalAccessor->bufferView];
                normalBuffer   = &model.buffers[normalView->buffer];
            }

            // Get texture coordinates if available
            const tinygltf::Accessor*   texCoordAccessor = nullptr;
            const tinygltf::BufferView* texCoordView     = nullptr;
            const tinygltf::Buffer*     texCoordBuffer   = nullptr;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoordView     = &model.bufferViews[texCoordAccessor->bufferView];
                texCoordBuffer   = &model.buffers[texCoordView->buffer];
            }

            // Extract vertex data
            size_t vertexStart = outModel.vertices.size();
            for (size_t i = 0; i < posAccessor.count; ++i) {
                Vertex vertex{};

                // Position
                const float* pos = reinterpret_cast<const float*>(
                    &posBuffer.data[posView.byteOffset + posAccessor.byteOffset + i * 12]);
                vertex.position = glm::vec3(pos[0], pos[1], pos[2]);

                // Track primitive bounds
                if (i == 0) {
                    primMin = primMax = vertex.position;
                } else {
                    primMin = glm::min(primMin, vertex.position);
                    primMax = glm::max(primMax, vertex.position);
                }

                // Normal
                if (normalAccessor) {
                    const float* norm = reinterpret_cast<const float*>(
                        &normalBuffer->data[normalView->byteOffset + normalAccessor->byteOffset + i * 12]);
                    vertex.normal = glm::vec3(norm[0], norm[1], norm[2]);
                } else {
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                // Texture coordinates
                if (texCoordAccessor) {
                    const float* texCoord = reinterpret_cast<const float*>(
                        &texCoordBuffer->data[texCoordView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
                    vertex.texCoord = glm::vec2(texCoord[0], texCoord[1]);
                } else {
                    vertex.texCoord = glm::vec2(0.0f, 0.0f);
                }

                outModel.vertices.push_back(vertex);
            }

            // Extract index data
            if (primitive.indices >= 0) {
                const tinygltf::Accessor&   indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView     = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer&     indexBuffer   = model.buffers[indexView.buffer];

                const void* dataPtr = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];

                for (size_t i = 0; i < indexAccessor.count; ++i) {
                    uint32_t index = 0;
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        index = static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(dataPtr)[i]);
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        index = reinterpret_cast<const uint32_t*>(dataPtr)[i];
                    }
                    outModel.indices.push_back(vertexStart + index);
                }
            }

            // Calculate index count for this primitive
            uint32_t primitiveIndexCount = static_cast<uint32_t>(outModel.indices.size()) - primitiveIndexStart;

            // Store mesh/primitive bounding box
            namedMesh.minBounds = primMin;
            namedMesh.maxBounds = primMax;

            // Store named mesh information
            namedMesh.name           = mesh.name;
            namedMesh.meshIndex      = static_cast<uint32_t>(meshIdx);
            namedMesh.primitiveIndex = static_cast<uint32_t>(primIdx);
            namedMesh.indexStart     = primitiveIndexStart;
            namedMesh.indexCount     = primitiveIndexCount;
            namedMesh.transform      = glm::mat4(1.0f);

            // If mesh has multiple primitives, append primitive index to name
            if (mesh.primitives.size() > 1) {
                namedMesh.name += "_primitive" + std::to_string(namedMesh.primitiveIndex);
            }

            // Store for hierarchy calculation even if unnamed
            outModel.namedMeshes.push_back(namedMesh);

            // Load texture/material if available
            if (primitive.material >= 0) {
                const tinygltf::Material& gltfMaterial = model.materials[primitive.material];
                Material                  newMaterial;
                bool                      dummyFlag = false;
                newMaterial.name           = gltfMaterial.name;
                newMaterial.meshIndex      = static_cast<int32_t>(meshIdx);
                newMaterial.primitiveIndex = static_cast<int32_t>(primIdx);
                newMaterial.indexStart     = primitiveIndexStart;
                newMaterial.indexCount     = primitiveIndexCount;

                // Name-based glass detection
                std::string nameLower = gltfMaterial.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                if (nameLower.find("glass") != std::string::npos || nameLower.find("window") != std::string::npos ||
                    nameLower.find("windshield") != std::string::npos ||
                    nameLower.find("transparent") != std::string::npos) {
                    newMaterial.props.isTransparent = true;
                    if (newMaterial.props.alphaValue >= 0.99f) {
                        newMaterial.props.alphaValue = 0.3f;
                    }
                }

                // Detect transparency from glTF alphaMode
                if (gltfMaterial.alphaMode == "BLEND")
                    newMaterial.props.isTransparent = true;

                // Get base color factor alpha
                bool hasCustomAlpha = (newMaterial.props.alphaValue < 0.99f);
                if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() >= 4 && !hasCustomAlpha) {
                    newMaterial.props.alphaValue =
                        static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]);
                    if (newMaterial.props.alphaValue < 0.99f) {
                        newMaterial.props.isTransparent = true;
                    }
                }

                // Extract texture paths or embedded data using helper
                processGLTFTexture(filepath, &model, gltfMaterial.pbrMetallicRoughness.baseColorTexture.index,
                                   newMaterial.baseColorTexture, newMaterial.embeddedBaseColor, dummyFlag);

                processGLTFTexture(filepath, &model, gltfMaterial.normalTexture.index, newMaterial.normalMapTexture,
                                   newMaterial.embeddedNormalMap, newMaterial.props.hasNormalMap);

                processGLTFTexture(filepath, &model, gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index,
                                   newMaterial.metallicRoughnessTexture, newMaterial.embeddedMetallicRoughness,
                                   newMaterial.props.hasMetallicRoughness);

                processGLTFTexture(filepath, &model, gltfMaterial.emissiveTexture.index, newMaterial.emissiveTexture,
                                   newMaterial.embeddedEmissive, newMaterial.props.hasEmissive);

                outModel.materials.push_back(newMaterial);
            }
        }
    }

    // Calculate bounding box
    if (!outModel.vertices.empty()) {
        outModel.minBounds = outModel.vertices[0].position;
        outModel.maxBounds = outModel.vertices[0].position;

        for (const auto& vertex : outModel.vertices) {
            outModel.minBounds.x = std::min(outModel.minBounds.x, vertex.position.x);
            outModel.minBounds.y = std::min(outModel.minBounds.y, vertex.position.y);
            outModel.minBounds.z = std::min(outModel.minBounds.z, vertex.position.z);
            outModel.maxBounds.x = std::max(outModel.maxBounds.x, vertex.position.x);
            outModel.maxBounds.y = std::max(outModel.maxBounds.y, vertex.position.y);
            outModel.maxBounds.z = std::max(outModel.maxBounds.z, vertex.position.z);
        }
    }

    // Extract glTF hierarchy (scene graph)
    outModel.nodes.reserve(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); i++) {
        const tinygltf::Node& gltfNode = model.nodes[i];
        glTFNode              node;

        node.name      = gltfNode.name;
        node.meshIndex = gltfNode.mesh;

        // Extract transform - glTF nodes can have either matrix or TRS
        if (!gltfNode.matrix.empty() && gltfNode.matrix.size() == 16) {
            // Node uses matrix representation (glTF stores matrices in column-major order)
            const auto& m = gltfNode.matrix;
            node.matrix   = glm::mat4(static_cast<float>(m[0]), static_cast<float>(m[1]), static_cast<float>(m[2]),
                                      static_cast<float>(m[3]), static_cast<float>(m[4]), static_cast<float>(m[5]),
                                      static_cast<float>(m[6]), static_cast<float>(m[7]), static_cast<float>(m[8]),
                                      static_cast<float>(m[9]), static_cast<float>(m[10]), static_cast<float>(m[11]),
                                      static_cast<float>(m[12]), static_cast<float>(m[13]), static_cast<float>(m[14]),
                                      static_cast<float>(m[15]));
        } else {
            // Node uses TRS representation
            if (!gltfNode.translation.empty() && gltfNode.translation.size() == 3) {
                node.translation = glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
            }
            if (!gltfNode.rotation.empty() && gltfNode.rotation.size() == 4) {
                // glTF quaternion order: [x, y, z, w]
                // glm quaternion order: [w, x, y, z]
                node.rotation =
                    glm::quat(static_cast<float>(gltfNode.rotation[3]), static_cast<float>(gltfNode.rotation[0]),
                              static_cast<float>(gltfNode.rotation[1]), static_cast<float>(gltfNode.rotation[2]));
            }
            if (!gltfNode.scale.empty() && gltfNode.scale.size() == 3) {
                node.scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
            }
        }

        // Extract children indices
        node.children.reserve(gltfNode.children.size());
        for (int childIdx : gltfNode.children) {
            node.children.push_back(childIdx);
        }

        outModel.nodes.push_back(node);
    }

    // Build parent links from children
    for (size_t i = 0; i < outModel.nodes.size(); i++) {
        for (int childIdx : outModel.nodes[i].children) {
            if (childIdx >= 0 && childIdx < static_cast<int>(outModel.nodes.size())) {
                outModel.nodes[childIdx].parent = static_cast<int>(i);
            }
        }
    }

    // Extract scenes
    outModel.scenes.reserve(model.scenes.size());
    for (const auto& gltfScene : model.scenes) {
        glTFScene scene;
        scene.name      = gltfScene.name;
        scene.rootNodes = gltfScene.nodes;
        outModel.scenes.push_back(scene);
    }
    outModel.defaultSceneIndex =
        (model.defaultScene >= 0 && model.defaultScene < static_cast<int>(outModel.scenes.size())) ? model.defaultScene : 0;

    return true;
}

std::string GLTFLoader::resolveTexturePath(const std::string& modelPath, const std::string& textureUri) {
    // Get directory of model file
    std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();

    // Resolve relative URI (try model directory first)
    std::filesystem::path texturePath = modelDir / textureUri;

    if (std::filesystem::exists(texturePath)) {
        return texturePath.string();
    }

    // Try alternate location: assets/textures/[model_name]/[texture_file]
    std::string           modelName       = std::filesystem::path(modelPath).stem().string();
    std::filesystem::path textureFilename = std::filesystem::path(textureUri).filename();
    std::filesystem::path altPath         = std::filesystem::path("assets/textures") / modelName / textureFilename;

    if (std::filesystem::exists(altPath)) {
        return altPath.string();
    }

    // Return original path and let loading fail gracefully
    std::cerr << "Warning: Could not resolve texture path for: " << textureUri << std::endl;
    std::cerr << "  Tried: " << texturePath.string() << std::endl;
    std::cerr << "  Tried: " << altPath.string() << std::endl;
    return texturePath.string();
}

void GLTFLoader::processGLTFTexture(const std::string& filepath, const void* modelPtr, int textureIndex,
                                   std::string& outPath, EmbeddedTexture& outEmbedded, bool& outHasFlag) {
    if (textureIndex < 0)
        return;

    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    if (textureIndex >= static_cast<int>(model.textures.size()))
        return;

    const auto& texture = model.textures[textureIndex];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
        return;

    const auto& image = model.images[texture.source];

    if (!image.uri.empty()) {
        outPath = resolveTexturePath(filepath, image.uri);
    } else if (!image.image.empty()) {
        outEmbedded.pixels = image.image;
        outEmbedded.width  = image.width;
        outEmbedded.height = image.height;
    }
    outHasFlag = true;
}

} // namespace DownPour
