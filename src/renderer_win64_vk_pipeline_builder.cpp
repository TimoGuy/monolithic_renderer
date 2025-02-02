#include "renderer_win64_vk_pipeline_builder.h"

#include <cassert>
#include <iostream>
#include "renderer_win64_vk_util.h"


void vk_pipeline::Graphics_pipeline_builder::clear()
{
    // Set all default values.
    m_shader_stages.clear();
    m_input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    };
    m_rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    };
    m_color_blend_attachment = {};
    m_multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    };
    m_pipeline_layout = {};
    m_depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    m_render_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    };
    m_color_attachment_format = VK_FORMAT_UNDEFINED;
}

void vk_pipeline::Graphics_pipeline_builder::set_pipeline_layout(VkPipelineLayout layout)
{
    m_pipeline_layout = layout;
}

void vk_pipeline::Graphics_pipeline_builder::set_shaders(VkShaderModule vertex_shader,
                                                         VkShaderModule fragment_shader)
{
    m_shader_stages.clear();
    m_shader_stages.reserve(2);
    m_shader_stages.emplace_back(
        vk_util::pipeline_shader_stage_info(VK_SHADER_STAGE_VERTEX_BIT,
                                            vertex_shader));
    m_shader_stages.emplace_back(
        vk_util::pipeline_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            fragment_shader));
}

void vk_pipeline::Graphics_pipeline_builder::set_input_topology(VkPrimitiveTopology topology)
{
    m_input_assembly.topology = topology;
    // @NOTE: Enable primitive restart for strips.
    m_input_assembly.primitiveRestartEnable = VK_FALSE;
}

void vk_pipeline::Graphics_pipeline_builder::set_polygon_mode(VkPolygonMode mode)
{
    m_rasterizer.polygonMode = mode;
    m_rasterizer.lineWidth = 1.0f;
}

void vk_pipeline::Graphics_pipeline_builder::set_cull_mode(VkCullModeFlags cull_mode,
                                                           VkFrontFace front_face)
{
    m_rasterizer.cullMode = cull_mode;
    m_rasterizer.frontFace = front_face;
}

void vk_pipeline::Graphics_pipeline_builder::disable_blending()
{
    m_color_blend_attachment.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT);
    m_color_blend_attachment.blendEnable = VK_FALSE;
}

void vk_pipeline::Graphics_pipeline_builder::set_multisampling_none()
{
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampling.minSampleShading = 1.0f;
    m_multisampling.pSampleMask = nullptr;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
}

void vk_pipeline::Graphics_pipeline_builder::set_color_attachment_format(VkFormat format)
{
    m_color_attachment_format = format;
    m_render_info.colorAttachmentCount = 1;
    m_render_info.pColorAttachmentFormats = &m_color_attachment_format;
}

void vk_pipeline::Graphics_pipeline_builder::set_depth_format(VkFormat format)
{
    m_render_info.depthAttachmentFormat = format;
}

void vk_pipeline::Graphics_pipeline_builder::disable_depthtest()
{
    m_depth_stencil.depthTestEnable = VK_FALSE;
    m_depth_stencil.depthWriteEnable = VK_FALSE;
    m_depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    m_depth_stencil.depthBoundsTestEnable = VK_FALSE;
    m_depth_stencil.stencilTestEnable = VK_FALSE;
    m_depth_stencil.front = {};
    m_depth_stencil.back = {};
    m_depth_stencil.minDepthBounds = 0.0f;
    m_depth_stencil.maxDepthBounds = 1.0f;
}

VkPipeline vk_pipeline::Graphics_pipeline_builder::build_pipeline(VkDevice device)
{
    assert(m_color_attachment_format != VK_FORMAT_UNDEFINED);

    VkPipelineViewportStateCreateInfo viewport_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &m_color_blend_attachment,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &m_render_info,
        .stageCount = static_cast<uint32_t>(m_shader_stages.size()),
        .pStages = m_shader_stages.data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &m_input_assembly,
        .pViewportState = &viewport_info,
        .pRasterizationState = &m_rasterizer,
        .pMultisampleState = &m_multisampling,
        .pDepthStencilState = &m_depth_stencil,
        .pColorBlendState = &color_blend_info,
        .layout = m_pipeline_layout,
    };

    VkDynamicState dynamic_states[]{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::size(dynamic_states),
        .pDynamicStates = dynamic_states,
    };
    pipeline_info.pDynamicState = &dynamic_info;

    // Create pipeline.
    VkPipeline new_pipeline;
    VkResult err{
        vkCreateGraphicsPipelines(device,
                                  VK_NULL_HANDLE,
                                  1,
                                  &pipeline_info,
                                  nullptr,
                                  &new_pipeline) };
    if (err)
    {
        std::cerr << "ERROR: failed to create graphics pipeline." << std::endl;
        return VK_NULL_HANDLE;
    }

    return new_pipeline;
}
