#include "Model.h"

#include "GLTFLoader.h"

#include <stdexcept>

namespace DownPour {

void Model::loadFromFile(const std::string& filepath, VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue) {
    // Load data from GLTF file using GLTFLoader
    if (!GLTFLoader::load(filepath, *this)) {
        throw std::runtime_error("Failed to load model: " + filepath);
    }

    // Create Vulkan buffers from loaded geometry
    geometry.createBuffers(vertices, indices, device, physicalDevice, commandPool, graphicsQueue);
}

void Model::cleanup(VkDevice device) {
    materials.clear();
    geometry.cleanup(device);
}

VkBuffer Model::getVertexBuffer() const {
    return geometry.getVertexBuffer();
}

VkBuffer Model::getIndexBuffer() const {
    return geometry.getIndexBuffer();
}

uint32_t Model::getIndexCount() const {
    return geometry.getIndexCount();
}

glm::mat4 Model::getModelMatrix() const {
    return this->modelMatrix;
}

void Model::setModelMatrix(const glm::mat4& matrix) {
    this->modelMatrix = matrix;
}

const std::vector<Material>& Model::getMaterials() const {
    return this->materials;
}

size_t Model::getMaterialCount() const {
    return this->materials.size();
}

const Material& Model::getMaterial(size_t index) const {
    if (index >= this->materials.size())
        throw std::out_of_range("Material index out of range");
    return this->materials[index];
}

std::vector<std::pair<size_t, const Material*>> Model::getMaterialsForMesh(int32_t meshIndex) const {
    std::vector<std::pair<size_t, const Material*>> results;
    for (size_t i = 0; i < materials.size(); i++) {
        if (materials[i].meshIndex == meshIndex) {
            results.push_back({i, &materials[i]});
        }
    }
    return results;
}

glm::vec3 Model::getMinBounds() const {
    return minBounds;
}

glm::vec3 Model::getMaxBounds() const {
    return maxBounds;
}

glm::vec3 Model::getDimensions() const {
    return maxBounds - minBounds;
}

glm::vec3 Model::getCenter() const {
    return (minBounds + maxBounds) * 0.5f;
}

void Model::getHierarchyBounds(glm::vec3& minOut, glm::vec3& maxOut) const {
    if (nodes.empty() || scenes.empty()) {
        minOut = minBounds;
        maxOut = maxBounds;
        return;
    }

    bool      initialized = false;
    glm::vec3 finalMin(0.0f), finalMax(0.0f);

    const auto& defaultScene = scenes[defaultSceneIndex];

    // Recursive helper to traverse hierarchy
    auto traverse = [&](auto& self, int nodeIdx, const glm::mat4& parentMatrix) -> void {
        const auto& node = nodes[nodeIdx];

        // Compute local matrix
        glm::mat4 localMatrix;
        if (node.matrix != glm::mat4(1.0f)) {
            localMatrix = node.matrix;
        } else {
            localMatrix = glm::translate(glm::mat4(1.0f), node.translation) * glm::mat4_cast(node.rotation) *
                          glm::scale(glm::mat4(1.0f), node.scale);
        }

        glm::mat4 globalMatrix = parentMatrix * localMatrix;

        // If node has a mesh, transform its bounding boxes
        if (node.meshIndex >= 0) {
            for (const auto& mesh : namedMeshes) {
                if (mesh.meshIndex == static_cast<uint32_t>(node.meshIndex)) {
                    // Transform primitive bounding box corners
                    glm::vec3 corners[8] = {{mesh.minBounds.x, mesh.minBounds.y, mesh.minBounds.z},
                                            {mesh.minBounds.x, mesh.minBounds.y, mesh.maxBounds.z},
                                            {mesh.minBounds.x, mesh.maxBounds.y, mesh.minBounds.z},
                                            {mesh.minBounds.x, mesh.maxBounds.y, mesh.maxBounds.z},
                                            {mesh.maxBounds.x, mesh.minBounds.y, mesh.minBounds.z},
                                            {mesh.maxBounds.x, mesh.minBounds.y, mesh.maxBounds.z},
                                            {mesh.maxBounds.x, mesh.maxBounds.y, mesh.minBounds.z},
                                            {mesh.maxBounds.x, mesh.maxBounds.y, mesh.maxBounds.z}};

                    for (int i = 0; i < 8; ++i) {
                        glm::vec3 p = glm::vec3(globalMatrix * glm::vec4(corners[i], 1.0f));
                        if (!initialized) {
                            finalMin = finalMax = p;
                            initialized         = true;
                        } else {
                            finalMin = glm::min(finalMin, p);
                            finalMax = glm::max(finalMax, p);
                        }
                    }
                }
            }
        }

        // Recursively process children
        for (int childIdx : node.children) {
            self(self, childIdx, globalMatrix);
        }
    };

    for (int rootIdx : defaultScene.rootNodes) {
        traverse(traverse, rootIdx, glm::mat4(1.0f));
    }

    if (initialized) {
        minOut = finalMin;
        maxOut = finalMax;
    } else {
        minOut = minBounds;
        maxOut = maxBounds;
    }
}

std::vector<std::string> Model::getMeshNames() const {
    std::vector<std::string> names;
    names.reserve(namedMeshes.size());
    for (const auto& mesh : namedMeshes)
        names.push_back(mesh.name);
    return names;
}

bool Model::getMeshByName(const std::string& name, NamedMesh& outMesh) const {
    for (const auto& mesh : namedMeshes) {
        if (mesh.name == name) {
            outMesh = mesh;
            return true;
        }
    }
    return false;
}

std::vector<NamedMesh> Model::getMeshesByPrefix(const std::string& prefix) const {
    std::vector<NamedMesh> matches;
    for (const auto& mesh : namedMeshes) {
        if (mesh.name.find(prefix) == 0) {
            matches.push_back(mesh);
        }
    }
    return matches;
}

bool Model::getMeshIndexRange(const std::string& name, uint32_t& outStart, uint32_t& outCount) const {
    NamedMesh mesh;
    if (getMeshByName(name, mesh)) {
        outStart = mesh.indexStart;
        outCount = mesh.indexCount;
        return true;
    }
    return false;
}

// Hierarchy accessors
const std::vector<glTFNode>& Model::getNodes() const {
    return nodes;
}

const glTFScene& Model::getDefaultScene() const {
    static glTFScene emptyScene;
    if (scenes.empty()) {
        return emptyScene;
    }
    return scenes[defaultSceneIndex];
}

const std::vector<glTFScene>& Model::getScenes() const {
    return scenes;
}

bool Model::hasHierarchy() const {
    return !nodes.empty();
}

}  // namespace DownPour
