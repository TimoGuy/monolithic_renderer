#pragma once

#if _WIN64

#include <vector>
#include <vulkan/vulkan.h>
#include "gltf_loader.h"

namespace vk_pipeline
{

bool load_shader_module(const char* file_path,
                        VkDevice device,
                        VkShaderModule& out_shader_module);

// @TODO: Add compute pipeline builder.

class Graphics_pipeline_builder
{
public:
    Graphics_pipeline_builder() { clear(); }

    void clear();
    void set_pipeline_layout(VkPipelineLayout layout);
    void set_shaders(VkShaderModule vertex_shader,
                     VkShaderModule fragment_shader);
    void set_vertex_input(const gltf_loader::Vertex_input_description& vertex_desc);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face);
    void disable_blending();
    void set_multisampling_none();
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void disable_depthtest();
    VkPipeline build_pipeline(VkDevice device);

private:
    VkPipelineLayout m_pipeline_layout;
    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
    gltf_loader::Vertex_input_description m_vertex_input_desc;
    VkPipelineInputAssemblyStateCreateInfo m_input_assembly;
    VkPipelineRasterizationStateCreateInfo m_rasterizer;
    VkPipelineColorBlendAttachmentState m_color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo m_multisampling;
    VkPipelineDepthStencilStateCreateInfo m_depth_stencil;
    VkPipelineRenderingCreateInfo m_render_info;
    VkFormat m_color_attachment_format;
};

}

#endif  // _WIN64
