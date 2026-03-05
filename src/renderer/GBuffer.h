#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>

namespace DownPour {

/**
 * @brief G-Buffer for deferred rendering
 *
 * Manages the geometry buffer textures used in the deferred shading pipeline.
 * The G-Buffer pass writes material properties into multiple render targets (MRT),
 * which the lighting pass reads as textures to compute final shading.
 *
 * Layout:
 *   Attachment 0 (R8G8B8A8_UNORM):      Albedo.rgb + Metallic
 *   Attachment 1 (R16G16B16A16_SFLOAT):  WorldNormal.xyz + Roughness
 *   Attachment 2 (R8G8B8A8_UNORM):       Emissive.rgb + AO
 *   Depth (D32_SFLOAT):                  Shared with forward pass (owned externally)
 *
 * World-space position is NOT stored — it's reconstructed in the lighting shader
 * from depth + invViewProj, saving an entire RGBA32F texture (16 bytes/pixel).
 */
class GBuffer {
public:
    static constexpr uint32_t ATTACHMENT_COUNT = 3;  // Number of color attachments

    GBuffer()  = default;
    ~GBuffer() = default;

    /**
     * @brief Create all G-Buffer resources
     *
     * @param device         Vulkan logical device
     * @param physicalDevice Physical device (for memory allocation)
     * @param width          Framebuffer width (matches swapchain)
     * @param height         Framebuffer height (matches swapchain)
     * @param depthFormat    Depth format (must match the shared depth image)
     * @param depthImageView Externally-owned depth image view (shared with forward pass)
     */
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                uint32_t width, uint32_t height,
                VkFormat depthFormat, VkImageView depthImageView);

    /**
     * @brief Destroy all G-Buffer resources
     */
    void destroy(VkDevice device);

    // Accessors
    VkRenderPass   getRenderPass()  const { return renderPass; }
    VkFramebuffer  getFramebuffer() const { return framebuffer; }
    VkSampler      getSampler()     const { return sampler; }

    VkImageView getAlbedoMetallicView()  const { return imageViews[0]; }
    VkImageView getNormalRoughnessView() const { return imageViews[1]; }
    VkImageView getEmissiveAOView()      const { return imageViews[2]; }

    /** @brief Get the format of a specific G-Buffer attachment */
    VkFormat getFormat(uint32_t index) const { return formats[index]; }

private:
    // G-Buffer color images (one per attachment, NOT per-frame —
    // only one G-Buffer pass runs per frame, protected by fences)
    std::array<VkImage, ATTACHMENT_COUNT>        images      = {};
    std::array<VkDeviceMemory, ATTACHMENT_COUNT> imageMemory = {};
    std::array<VkImageView, ATTACHMENT_COUNT>    imageViews  = {};
    std::array<VkFormat, ATTACHMENT_COUNT>        formats     = {};

    VkRenderPass  renderPass  = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkSampler     sampler     = VK_NULL_HANDLE;  // Shared sampler for reading in lighting pass

    void createImages(VkDevice device, VkPhysicalDevice physicalDevice,
                      uint32_t width, uint32_t height);
    void createRenderPass(VkDevice device, VkFormat depthFormat);
    void createFramebuffer(VkDevice device, uint32_t width, uint32_t height,
                           VkImageView depthImageView);
    void createSampler(VkDevice device);
};

}  // namespace DownPour
