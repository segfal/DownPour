#pragma once

#include <vulkan/vulkan.h>

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace DownPour {

/**
 * @brief Wrapper for Vulkan texture resources
 *
 * Encapsulates all Vulkan handles needed for a single texture,
 * providing type safety and easier resource management.
 */
struct TextureHandle {
    VkImage        image   = VK_NULL_HANDLE;
    VkImageView    view    = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;

    bool isValid() const { return image != VK_NULL_HANDLE; }

    void reset() {
        image   = VK_NULL_HANDLE;
        view    = VK_NULL_HANDLE;
        sampler = VK_NULL_HANDLE;
        memory  = VK_NULL_HANDLE;
    }
};

/**
 * @brief Material properties (data-only, no GPU resources)
 *
 * Describes the visual and rendering properties of a material
 * without coupling to any specific graphics API.
 */
struct MaterialProperties {
    float alphaValue           = 1.0f;   // 0.0 = fully transparent, 1.0 = opaque
    bool  isTransparent        = false;  // Whether material requires alpha blending
    bool  hasNormalMap         = false;  // Material uses normal mapping
    bool  hasMetallicRoughness = false;  // Material uses PBR metallic-roughness
    bool  hasEmissive          = false;  // Material has emissive component

    // TODO: Add more PBR properties as needed
    // float metallicFactor = 0.0f;
    // float roughnessFactor = 1.0f;
    // glm::vec3 emissiveFactor = glm::vec3(0.0f);
};

/**
 * @brief Material definition (asset data)
 *
 * Represents a material as loaded from a model file or defined by the application.
 * Contains references to texture assets (paths) and rendering properties,
 * but no GPU-specific resources.
 */
/**
 * @brief Embedded texture data (for GLB files)
 *
 * Stores raw pixel data for textures embedded in binary glTF files.
 */
struct EmbeddedTexture {
    std::vector<unsigned char> pixels;  // Raw RGBA pixel data
    int                        width  = 0;
    int                        height = 0;

    bool isValid() const { return !pixels.empty() && width > 0 && height > 0; }
};

struct Material {
    uint32_t           id;     // Unique material identifier
    std::string        name;   // Material name (from glTF or user-defined)
    MaterialProperties props;  // Visual and rendering properties

    // Texture asset paths (for external textures)
    std::string baseColorTexture;
    std::string normalMapTexture;
    std::string metallicRoughnessTexture;
    std::string emissiveTexture;

    // Embedded texture data (for GLB files with embedded textures)
    EmbeddedTexture embeddedBaseColor;
    EmbeddedTexture embeddedNormalMap;
    EmbeddedTexture embeddedMetallicRoughness;
    EmbeddedTexture embeddedEmissive;

    // Mesh association (which mesh/primitive this material belongs to)
    int32_t meshIndex      = -1;  // glTF mesh index (-1 if not associated)
    int32_t primitiveIndex = -1;  // Primitive within the mesh

    // Index range for rendering (which part of mesh uses this material)
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;

    // Helper to check if we have a base color texture (path or embedded)
    bool hasBaseColorTexture() const { return !baseColorTexture.empty() || embeddedBaseColor.isValid(); }
};

struct MaterialDispatcher {
    std::string                                                 name;
    std::function<bool(const Material&)>                        dispatchFunction;
    std::map<std::string, std::function<bool(const Material&)>> textureChecks;

    bool dispatch(const Material& material) { return dispatchFunction(material); }

    bool checkTexture(const std::string& name, const Material& material) {
        auto it = textureChecks.find(name);
        return it != textureChecks.end() ? it->second(material) : false;
    }

    void addTextureCheck(const std::string& name, std::function<bool(const Material&)> checkFunc) {
        textureChecks[name] = checkFunc;
    }

    MaterialDispatcher(const Material& material) {  // lambdas to check if empty
        textureChecks["baseColor"]         = [](const Material& material) { return material.baseColorTexture.empty(); };
        textureChecks["normalMap"]         = [](const Material& material) { return material.normalMapTexture.empty(); };
        textureChecks["metallicRoughness"] = [](const Material& material) {
            return material.metallicRoughnessTexture.empty();
        };
        textureChecks["emissive"] = [](const Material& material) { return material.emissiveTexture.empty(); };
    }
};

/**
 * @brief Vulkan-specific material resources
 *
 * Internal structure holding all GPU resources for a material.
 * Managed by MaterialManager, not exposed to external code.
 */
struct VulkanMaterialResources {
    TextureHandle                baseColor;
    TextureHandle                normalMap;
    TextureHandle                metallicRoughness;
    TextureHandle                emissive;
    std::vector<VkDescriptorSet> descriptorSets;  // Per-frame descriptor sets

    bool hasAnyTextures() const {
        return baseColor.isValid() || normalMap.isValid() || metallicRoughness.isValid() || emissive.isValid();
    }
};

/**
 * @brief Material manager for GPU resource lifecycle
 *
 * Handles creation, loading, and binding of material GPU resources.
 * Separates material data (Material struct) from GPU implementation (Vulkan resources).
 *
 * Usage:
 *   MaterialManager matMgr(device, physicalDevice, commandPool, queue);
 *   uint32_t matId = matMgr.createMaterial(materialData);
 *   matMgr.bindMaterial(matId, commandBuffer, pipelineLayout);
 */
class MaterialManager {
public:
    /**
     * @brief Construct material manager with Vulkan context
     */
    MaterialManager(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);

    ~MaterialManager();

    /**
     * @brief Create material and load its GPU resources
     *
     * Loads textures from disk, creates Vulkan resources, and returns
     * a unique material ID for later binding.
     *
     * @param material Material definition with texture paths
     * @return Unique material ID for this material
     */
    uint32_t createMaterial(const Material& material);

    /**
     * @brief Get descriptor set for a material
     *
     * @param materialId Material ID returned from createMaterial()
     * @return Descriptor set binding all material textures
     */
    VkDescriptorSet getDescriptorSet(uint32_t materialId) const;

    /**
     * @brief Bind material for rendering
     *
     * Binds the material's descriptor set to the command buffer.
     * Must be called before drawing geometry using this material.
     *
     * @param                materialId Material ID to bind
     * @param                cmd Command buffer to record into
     * @param                layout Pipeline layout for descriptor set binding
     */
    void bindMaterial(uint32_t materialId, VkCommandBuffer cmd, VkPipelineLayout layout);

    /**
     * @brief Get material properties
     *
     * @param materialId Material ID
     * @return Material properties (alpha, transparency, etc.)
     */
    const MaterialProperties& getProperties(uint32_t materialId) const;

    /**
     * @brief Initialize descriptor set support
     *
     * Must be called after creating descriptor pool and layout in Application.
     * MaterialManager will use these to create descriptor sets for materials.
     *
     * @param layout Descriptor set layout from Application
     * @param pool Descriptor pool from Application
     * @param maxFramesInFlight Number of frames in flight (typically 2)
     */
    void initDescriptorSupport(VkDescriptorSetLayout layout, VkDescriptorPool pool, uint32_t maxFramesInFlight);

    /**
     * @brief Create descriptor sets for all existing materials
     *
     * Call this after initDescriptorSupport() to create descriptor sets
     * for materials that were created before descriptor support was initialized.
     */
    void createDescriptorSetsForExistingMaterials();

    /**
     * @brief Get descriptor set for a material
     *
     * @param materialId Material ID
     * @param frameIndex Current frame index
     * @return Descriptor set for binding
     */
    VkDescriptorSet getDescriptorSet(uint32_t materialId, uint32_t frameIndex) const;

    /**
     * @brief Clean up all GPU resources
     */
    void cleanup();

private:
    VkDevice              device;
    VkPhysicalDevice      physicalDevice;
    VkCommandPool         commandPool;
    VkQueue               graphicsQueue;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool      descriptorPool;
    uint32_t              maxFramesInFlight;

    std::unordered_map<uint32_t, VulkanMaterialResources> resources;
    std::unordered_map<uint32_t, MaterialProperties>      properties;
    uint32_t                                              nextMaterialId;

    // Default textures for materials without specific textures
    TextureHandle defaultWhiteTexture;

    // Helper methods for texture loading
    TextureHandle loadTexture(const std::string& path);
    TextureHandle loadTextureFromData(const EmbeddedTexture& embeddedTex);
    TextureHandle createDefaultWhiteTexture();
    void          createTextureImage(const unsigned char* pixels, const int width, const int height, const int channels,
                                     TextureHandle& outTexture);
    void          createTextureImageView(TextureHandle& texture);
    void          createTextureSampler(TextureHandle& texture);
    void          destroyTextureHandle(TextureHandle& texture);
    void createImage(const uint32_t width, const uint32_t height, const VkFormat format, const VkImageTiling tiling,
                     const VkImageUsageFlags usage, const VkMemoryPropertyFlags properties, VkImage& image,
                     VkDeviceMemory& imageMemory);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void     copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

}  // namespace DownPour
