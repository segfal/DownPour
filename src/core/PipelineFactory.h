#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

namespace DownPour {

/**
 * @brief Configuration for Vulkan graphics pipeline creation
 */
struct PipelineConfig {
    std::string                        vertShader;
    std::string                        fragShader;
    VkPipelineLayout                   layout           = VK_NULL_HANDLE;
    bool                               enableBlending   = false;
    bool                               enableDepthWrite = true;
    VkCompareOp                        depthCompareOp   = VK_COMPARE_OP_LESS;
    VkCullModeFlags                    cullMode         = VK_CULL_MODE_BACK_BIT;
    VkPrimitiveTopology                topology         = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    float                              lineWidth        = 1.0f;
    std::vector<VkDescriptorSetLayout> descriptorLayouts;

    // Optional custom vertex input (if non-empty, overrides default Vertex layout)
    std::vector<VkVertexInputBindingDescription>   customBindings;
    std::vector<VkVertexInputAttributeDescription> customAttributes;

    // When true, pipeline has no vertex input (e.g., fullscreen triangle from gl_VertexIndex)
    bool noVertexInput = false;
};

/**
 * @brief Factory for creating Vulkan graphics pipelines
 *
 * Encapsulates pipeline creation logic, shader loading, and pipeline configuration.
 */
class PipelineFactory {
public:
    PipelineFactory()  = default;
    ~PipelineFactory() = default;

    /**
     * @brief Create a graphics pipeline from configuration
     *
     * @param device Vulkan logical device
     * @param config Pipeline configuration
     * @param renderPass Render pass the pipeline will be used with
     * @param extent Viewport/scissor extent
     * @return Created pipeline handle
     */
    static VkPipeline createPipeline(VkDevice device, const PipelineConfig& config, VkRenderPass renderPass,
                                     VkExtent2D extent);

    /**
     * @brief Create a pipeline layout from descriptor set layouts
     *
     * @param device Vulkan logical device
     * @param descriptorLayouts Descriptor set layouts
     * @return Created pipeline layout handle
     */
    static VkPipelineLayout createPipelineLayout(VkDevice                                  device,
                                                 const std::vector<VkDescriptorSetLayout>& descriptorLayouts);

    /**
     * @brief Create a pipeline layout with push constants
     */
    static VkPipelineLayout createPipelineLayoutWithPushConstants(
        VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptorLayouts,
        const std::vector<VkPushConstantRange>& pushConstantRanges);

private:
    /**
     * @brief Load shader module from SPIR-V file
     *
     * @param device Vulkan logical device
     * @param filename Path to SPIR-V shader file
     * @return Created shader module handle
     */
    static VkShaderModule loadShaderModule(VkDevice device, const std::string& filename);

    /**
     * @brief Read file contents into byte buffer
     *
     * @param filename Path to file
     * @return File contents as byte vector
     */
    static std::vector<char> readFile(const std::string& filename);
};

}  // namespace DownPour
