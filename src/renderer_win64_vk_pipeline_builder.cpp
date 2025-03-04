#include "renderer_win64_vk_pipeline_builder.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include "material_bank.h"
#include "renderer_win64_vk_util.h"
#include "renderer_win64_vk_descriptor_layout_builder.h"
#include "spirv_reflect.h"


bool load_file_binary(const char* file_path, std::vector<uint32_t>& out_buffer)
{
    std::ifstream file{ file_path, std::ios::ate | std::ios::binary };
    if (!file.is_open())
    {
        std::cerr << "ERROR: shader module not found: " << file_path << std::endl;
        return false;
    }

    // Read spir-v.
    size_t file_size{ static_cast<size_t>(file.tellg()) };
    out_buffer.clear();
    out_buffer.resize(file_size / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(out_buffer.data()), file_size);
    file.close();

    return true;
}

bool vk_pipeline::load_shader_module(const char* file_path,
                                     VkDevice device,
                                     VkShaderModule& out_shader_module)
{
    std::vector<uint32_t> buffer;
    if (!load_file_binary(file_path, buffer))
    {
        return false;
    }

    // Create shader module.
    VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = (buffer.size() * sizeof(uint32_t)),
        .pCode = buffer.data(),
    };

    VkShaderModule shader_module;
    VkResult err{
        vkCreateShaderModule(device, &create_info, nullptr, &shader_module) };
    if (err)
    {
        std::cerr << "ERROR: creating shader module failed: " << file_path << std::endl;
        return false;
    }
    out_shader_module = shader_module;

    return true;
}

bool vk_pipeline::load_shader_module_spirv_reflect(const char* file_path)
{
    // I am not working on this anymore, bc there should be the list of required descriptor sets!
    // I think that using reflection and trying to maintain this will be horribly inefficient and take too long to develop.
    //   -Thea 2025/03/02
    //assert(false);
    
    
    
    std::vector<uint32_t> buffer;
    if (!load_file_binary(file_path, buffer))
    {
        return false;
    }

    // Load shader into reflection.
    spv_reflect::ShaderModule shader_module{ buffer };
    if (shader_module.GetResult() != SPV_REFLECT_RESULT_SUCCESS)
    {
        std::cerr
            << "ERROR: could not create shader module reflection -> "
            << file_path
            << std::endl;
        return false;
    }

    // Check that it's a frag shader.
    if (shader_module.GetShaderStage() != SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT)
    {
        std::cerr
            << "ERROR: provided shader is not fragment shader -> "
            << file_path
            << std::endl;
        return false;
    }


    // Check that shader has necessary material param struct.
    std::vector<SpvReflectDescriptorSet*> descriptor_sets;
    SpvReflectResult result;

    uint32_t count;
    result = shader_module.EnumerateDescriptorSets(&count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    descriptor_sets.resize(count);
    result = shader_module.EnumerateDescriptorSets(&count, descriptor_sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<material_bank::Material_parameter_definition> mat_param_defs;

    for (auto desc_set : descriptor_sets)
    for (size_t i = 0; i < desc_set->binding_count; i++)
    {
        auto binding{ desc_set->bindings[i] };
        if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
            std::string(binding->type_description->type_name) ==
                "Material_param_definitions_buffer")
        {
            // @TODO: make these all a bunch of requirements that will crash
            //   the program if not properly defined.
            assert(binding->type_description->op == SpvOpTypeStruct);
            assert(binding->type_description->member_count == 1);
            assert(binding->type_description->members[0].op == SpvOpTypeRuntimeArray);
            assert(binding->type_description->members[0].struct_type_description->op == SpvOpTypeStruct);
            assert(std::string(binding->type_description->members[0].struct_type_description->type_name) == "Material_param_definition");

            // Iterate thru all struct members.
            auto& struct_def{ *binding->type_description->members[0].struct_type_description };
            for (size_t j = 0; j < struct_def.member_count; j++)
            {
                auto& member{ struct_def.members[j] };
                material_bank::Material_parameter_definition new_definition{
                    .param_name = std::string(member.struct_member_name),
                };

                using Param_type = material_bank::Mat_param_def_type;
                switch (member.op)
                {
                case SpvOpTypeInt:
                    new_definition.param_type =
                        (member.traits.numeric.scalar.signedness == 0 ?
                            Param_type::UINT :
                            Param_type::INT);
                    break;

                case SpvOpTypeFloat:
                    new_definition.param_type = Param_type::FLOAT;
                    break;

                case SpvOpTypeVector:
                    if (member.type_flags & SPV_REFLECT_TYPE_FLAG_INT)
                    {
                        bool is_signed{
                            member.traits.numeric.scalar.signedness == 1 };
                        switch (member.traits.numeric.vector.component_count)
                        {
                        case 2:
                            new_definition.param_type =
                                (is_signed ? Param_type::IVEC2 : Param_type::UVEC2);
                            break;

                        case 3:
                            new_definition.param_type =
                                (is_signed ? Param_type::IVEC3 : Param_type::UVEC3);
                            break;

                        case 4:
                            new_definition.param_type =
                                (is_signed ? Param_type::IVEC4 : Param_type::UVEC4);
                            break;
                        }
                    }
                    else if (member.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
                    {
                        switch (member.traits.numeric.vector.component_count)
                        {
                        case 2: new_definition.param_type = Param_type::VEC2; break;
                        case 3: new_definition.param_type = Param_type::VEC3; break;
                        case 4: new_definition.param_type = Param_type::VEC4; break;
                        }
                    }
                    break;

                case SpvOpTypeMatrix:
                    if (member.traits.numeric.matrix.column_count == member.traits.numeric.matrix.row_count)
                    {
                        uint32_t dims{ member.traits.numeric.matrix.column_count };
                        switch (dims)
                        {
                        case 3: new_definition.param_type = Param_type::MAT3; break;
                        case 4: new_definition.param_type = Param_type::MAT4; break;
                        }
                    }
                    break;
                }

                // @TODO: calculate memory offset and padded size.

                // @TODO: add new definition to list.
            }
            std::cout << "SKIIIII" << std::endl;
        }
    }

    //// Build descriptor sets.
    //vk_desc::Descriptor_layout_builder builder;
    //for (auto desc_set : descriptor_sets)
    //{
    //    builder.clear();
    //    for (uint32_t i = 0; i < desc_set->binding_count; i++)
    //    {
    //        auto binding{ desc_set->bindings[i] };
    //        builder.add_binding(
    //            binding->binding,
    //            static_cast<VkDescriptorType>(binding->descriptor_type));
    //    }
    //    //builder.build()
    //}

    return true;
}

void vk_pipeline::Graphics_pipeline_builder::clear()
{
    // Set all default values.
    m_shader_stages.clear();
    m_vertex_input_desc = {};
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

void vk_pipeline::Graphics_pipeline_builder::set_vertex_input(
    const gltf_loader::Vertex_input_description& vertex_desc)
{
    m_vertex_input_desc = vertex_desc;
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

void vk_pipeline::Graphics_pipeline_builder::set_less_than_writing_depthtest()
{
    m_depth_stencil.depthTestEnable = VK_TRUE;
    m_depth_stencil.depthWriteEnable = VK_TRUE;
    m_depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    m_depth_stencil.depthBoundsTestEnable = VK_FALSE;
    m_depth_stencil.stencilTestEnable = VK_FALSE;
    m_depth_stencil.front = {};
    m_depth_stencil.back = {};
    m_depth_stencil.minDepthBounds = 0.0f;
    m_depth_stencil.maxDepthBounds = 1.0f;
}

void vk_pipeline::Graphics_pipeline_builder::set_equal_nonwriting_depthtest()
{
    m_depth_stencil.depthTestEnable = VK_TRUE;
    m_depth_stencil.depthWriteEnable = VK_FALSE;
    m_depth_stencil.depthCompareOp = VK_COMPARE_OP_EQUAL;
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
        .flags = m_vertex_input_desc.flags,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_input_desc.bindings.size()),
        .pVertexBindingDescriptions = m_vertex_input_desc.bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_input_desc.attributes.size()),
        .pVertexAttributeDescriptions = m_vertex_input_desc.attributes.data(),
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
