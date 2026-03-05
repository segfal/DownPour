#include "GBuffer.h"

#include "../core/ResourceManager.h"

#include <stdexcept>

namespace DownPour {

// ============================================================================
// G-Buffer Attachment Formats
//
//   [0] R8G8B8A8_UNORM      — Albedo.rgb + Metallic (8-bit per channel is enough
//                              for color and 0-1 metallic)
//   [1] R16G16B16A16_SFLOAT — Normal.xyz + Roughness (normals need precision for
//                              specular; float16 gives 10-bit mantissa)
//   [2] R8G8B8A8_UNORM      — Emissive.rgb + AO (low-frequency signals, 8-bit ok)
//
// Total memory at 800x600: (4 + 8 + 4) bytes/pixel × 480,000 pixels = 7.68 MB
// ============================================================================

static constexpr VkFormat GBUFFER_FORMATS[GBuffer::ATTACHMENT_COUNT] = {
    VK_FORMAT_R8G8B8A8_UNORM,       // Albedo + Metallic
    VK_FORMAT_R16G16B16A16_SFLOAT,   // Normal + Roughness
    VK_FORMAT_R8G8B8A8_UNORM,       // Emissive + AO
};

void GBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice,
                     uint32_t width, uint32_t height,
                     VkFormat depthFormat, VkImageView depthImageView) {
    for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i) {
        formats[i] = GBUFFER_FORMATS[i];
    }

    createImages(device, physicalDevice, width, height);
    createRenderPass(device, depthFormat);
    createFramebuffer(device, width, height, depthImageView);
    createSampler(device);
}

void GBuffer::destroy(VkDevice device) {
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }

    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i) {
        if (imageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageViews[i], nullptr);
            imageViews[i] = VK_NULL_HANDLE;
        }
        if (images[i] != VK_NULL_HANDLE) {
            vkDestroyImage(device, images[i], nullptr);
            images[i] = VK_NULL_HANDLE;
        }
        if (imageMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device, imageMemory[i], nullptr);
            imageMemory[i] = VK_NULL_HANDLE;
        }
    }
}

// ============================================================================
// Image Creation
//
// Each G-Buffer attachment is a 2D image with:
//   - COLOR_ATTACHMENT_BIT: so it can be written as a render target
//   - SAMPLED_BIT: so the lighting pass can read it as a texture
// ============================================================================

void GBuffer::createImages(VkDevice device, VkPhysicalDevice physicalDevice,
                           uint32_t width, uint32_t height) {
    for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i) {
        ResourceManager::createImage(
            device, physicalDevice, width, height,
            formats[i],
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            images[i], imageMemory[i]);

        // Create image view for each attachment
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = images[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = formats[i];
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create G-Buffer image view");
        }
    }
}

// ============================================================================
// Render Pass
//
// The G-Buffer render pass has 4 attachments:
//   [0-2] Color attachments (the 3 G-Buffer textures)
//   [3]   Depth attachment (shared with the forward pass)
//
// Single subpass writes to all 3 color attachments + depth.
// After the pass, color attachments transition to SHADER_READ_ONLY so the
// lighting pass can sample them. Depth stays as DEPTH_STENCIL_ATTACHMENT
// because the forward transparent pass still needs it for depth testing.
// ============================================================================

void GBuffer::createRenderPass(VkDevice device, VkFormat depthFormat) {
    // 3 color attachment descriptions + 1 depth
    std::array<VkAttachmentDescription, ATTACHMENT_COUNT + 1> attachments{};

    // Color attachments [0-2]
    for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i) {
        attachments[i].format         = formats[i];
        attachments[i].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;     // Must store — lighting reads these
        attachments[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;        // Don't care about previous contents
        attachments[i].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // Ready for lighting to sample
    }

    // Depth attachment [3]
    attachments[ATTACHMENT_COUNT].format         = depthFormat;
    attachments[ATTACHMENT_COUNT].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[ATTACHMENT_COUNT].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[ATTACHMENT_COUNT].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;   // Must store — forward pass and lighting need it
    attachments[ATTACHMENT_COUNT].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[ATTACHMENT_COUNT].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[ATTACHMENT_COUNT].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[ATTACHMENT_COUNT].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // Forward pass reuses this

    // Subpass: writes to all 3 color + depth
    std::array<VkAttachmentReference, ATTACHMENT_COUNT> colorRefs{};
    for (uint32_t i = 0; i < ATTACHMENT_COUNT; ++i) {
        colorRefs[i].attachment = i;
        colorRefs[i].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference depthRef{};
    depthRef.attachment = ATTACHMENT_COUNT;  // Index 3
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = ATTACHMENT_COUNT;
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

    // Dependency: ensure G-Buffer writes complete before the lighting pass reads them.
    // VK_SUBPASS_EXTERNAL → subpass 0: wait for previous frame to finish before clearing.
    // subpass 0 → VK_SUBPASS_EXTERNAL: ensure writes finish before external passes read.
    std::array<VkSubpassDependency, 2> dependencies{};

    // Before: previous frame's reads must complete before we clear
    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = 0;

    // After: our writes must complete before the lighting pass samples G-Buffer
    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer render pass");
    }
}

// ============================================================================
// Framebuffer
//
// Binds the 3 G-Buffer image views + the shared depth image view into a
// framebuffer compatible with the G-Buffer render pass.
// ============================================================================

void GBuffer::createFramebuffer(VkDevice device, uint32_t width, uint32_t height,
                                VkImageView depthImageView) {
    std::array<VkImageView, ATTACHMENT_COUNT + 1> attachments = {
        imageViews[0],   // Albedo + Metallic
        imageViews[1],   // Normal + Roughness
        imageViews[2],   // Emissive + AO
        depthImageView   // Shared depth (owned by Application)
    };

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = renderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbInfo.pAttachments    = attachments.data();
    fbInfo.width           = width;
    fbInfo.height          = height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer framebuffer");
    }
}

// ============================================================================
// Sampler
//
// The lighting pass reads G-Buffer textures through this sampler.
// Linear filtering for normals/albedo, clamp-to-edge to avoid border artifacts.
// ============================================================================

void GBuffer::createSampler(VkDevice device) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_NEAREST;   // Nearest for G-Buffer — we want exact texel values,
    samplerInfo.minFilter    = VK_FILTER_NEAREST;   // not interpolated material properties between pixels
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable    = VK_FALSE;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create G-Buffer sampler");
    }
}

}  // namespace DownPour
