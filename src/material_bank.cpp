#include "material_bank.h"

#include <cassert>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include "renderer_win64_vk_pipeline_builder.h"


namespace material_bank
{

// Descriptor layout references.
static VkDescriptorSetLayout s_main_camera_descriptor_layout;
static VkDescriptorSetLayout s_shadow_camera_descriptor_layout;
static VkDescriptorSetLayout s_material_sets_indexing_descriptor_layout;
static VkDescriptorSetLayout s_material_agnostic_descriptor_layout;

// Descriptor set references.
static VkDescriptorSet s_material_sets_indexing_descriptor_set;

// Pipeline containers.
static std::unordered_map<std::string, uint32_t> s_pipe_name_to_idx;
static std::mutex s_pipe_name_to_idx_mutex;

static std::vector<GPU_pipeline> s_all_pipelines;
static std::mutex s_all_pipelines_mutex;

// Material containers.
static std::unordered_map<std::string, uint32_t> s_mat_name_to_idx;
static std::mutex s_mat_name_to_idx_mutex;

static std::vector<GPU_material> s_all_materials;
static std::mutex s_all_materials_mutex;

// Material set containers.
static std::unordered_map<std::string, uint32_t> s_mat_set_name_to_idx;
static std::mutex s_mat_set_name_to_idx_mutex;

static std::vector<GPU_material_set> s_all_material_sets;
static std::mutex s_all_material_sets_mutex;

// Material push constant.
struct GPU_material_push_constant
{
    VkDeviceAddress geo_instance_buffer;
};

}  // namespace material_bank


// GPU_pipeline.
void material_bank::GPU_pipeline::bind_pipeline(
    VkCommandBuffer cmd,
    const VkViewport& viewport,
    const VkRect2D& scissor,
    const VkDescriptorSet* main_view_camera_descriptor_set,
    const VkDescriptorSet* shadow_view_camera_descriptor_set,
    VkDeviceAddress instance_data_buffer_address) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    bool use_material_params{ !material_param_definitions.empty() };
    std::vector<VkDescriptorSet> desc_sets;
    desc_sets.resize(use_material_params ? 3 : 1);

    // Bind camera descriptor set.
    switch (camera_type)
    {
    case Camera_type::MAIN_VIEW:
        assert(main_view_camera_descriptor_set != nullptr);
        desc_sets[0] = *main_view_camera_descriptor_set;
        break;

    case Camera_type::SHADOW_VIEW:
        assert(shadow_view_camera_descriptor_set != nullptr);
        desc_sets[0] = *shadow_view_camera_descriptor_set;
        assert(false);  // @TODO: Not implemented yet!
        break;

    default:
        assert(false);
        break;
    }

    // Bind global material set and material param descriptor set (optional).
    if (use_material_params)
    {
        desc_sets[1] = s_material_sets_indexing_descriptor_set;
        desc_sets[2] = combined_all_material_datas_descriptor_set;
        assert(false);  // @TODO: @CHECK: This isn't setup yet!
    }

    // Bind descriptor sets.
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout,
                            0,
                            static_cast<uint32_t>(desc_sets.size()), desc_sets.data(),
                            0, nullptr);

    // Push instance buffer reference.
    GPU_material_push_constant mat_pc{
        .geo_instance_buffer = instance_data_buffer_address,
    };
    vkCmdPushConstants(cmd,
                       pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(GPU_material_push_constant),
                       &mat_pc);
}

// Passing references.
void material_bank::set_descriptor_references(
    VkDescriptorSetLayout main_camera_descriptor_layout,
    VkDescriptorSetLayout shadow_camera_descriptor_layout,
    VkDescriptorSetLayout material_sets_indexing_descriptor_layout,
    VkDescriptorSetLayout material_agnostic_descriptor_layout,
    VkDescriptorSet material_sets_indexing_descriptor_set)
{
    s_main_camera_descriptor_layout = main_camera_descriptor_layout;
    s_shadow_camera_descriptor_layout = shadow_camera_descriptor_layout;
    s_material_sets_indexing_descriptor_layout = material_sets_indexing_descriptor_layout;
    s_material_agnostic_descriptor_layout = material_agnostic_descriptor_layout;
    s_material_sets_indexing_descriptor_set = material_sets_indexing_descriptor_set;
}

// Pipeline.
material_bank::GPU_pipeline material_bank::create_geometry_material_pipeline(
    VkDevice device,
    VkFormat draw_format,
    bool has_z_prepass,
    Camera_type camera_type,
    bool use_material_params,
    std::vector<Material_parameter_definition>&& material_param_definitions,
    const char* vert_shader_path,
    const char* frag_shader_path)
{
    GPU_pipeline new_pipeline;
    new_pipeline.camera_type = camera_type;
    new_pipeline.material_param_definitions = std::move(material_param_definitions);

    if (use_material_params)
    {
        // Assert that since it declares using material params
        // there must be material params attached.
        assert(new_pipeline.material_param_definitions.size() > 0);

        // Verify material param definitions match and fill in calculated fields.
        size_t reflected_material_param_block_size_padded;
        std::vector<Material_parameter_definition> reflected_material_param_definitions;
        vk_pipeline::load_shader_module_spirv_reflect_extract_material_params(
            frag_shader_path,
            reflected_material_param_block_size_padded,
            reflected_material_param_definitions);

        if (new_pipeline.material_param_definitions.size() !=
            reflected_material_param_definitions.size())
        {
            std::cerr
                << "ERROR: Number of material param definitions mismatched. Given: "
                << new_pipeline.material_param_definitions.size()
                << ". Reflected: "
                << reflected_material_param_definitions.size()
                << "."
                << std::endl;
            assert(false);
        }

        size_t num_definitions{ new_pipeline.material_param_definitions.size() };
        size_t num_definitions_found{ 0 };

        for (auto& given_mat_param_def : new_pipeline.material_param_definitions)
        for (auto& reflected_mat_param_def : reflected_material_param_definitions)
        if (given_mat_param_def.param_name == reflected_mat_param_def.param_name)
        {
            if (given_mat_param_def.param_type == reflected_mat_param_def.param_type)
            {
                // One to one transfer of data.
                given_mat_param_def.calculated =
                    reflected_mat_param_def.calculated;
            }
            else if (given_mat_param_def.param_type == Mat_param_def_type::TEXTURE_NAME &&
                reflected_mat_param_def.param_type == Mat_param_def_type::UINT)
            {
                // Convert texture name to texture idx.
                given_mat_param_def.calculated =
                    reflected_mat_param_def.calculated;

                // @TODO: IMPLEMENT THIS!
                // @TODO: Textures are not supported yet!
                assert(false);
            }
            else
            {
                // Validation failed.
                std::cerr
                    << "ERROR: Reflected and given param types mismatched: "
                    << given_mat_param_def.param_name
                    << std::endl;
                assert(false);
            }

            // Move onto next given mat param.
            num_definitions_found++;
            break;
        }

        if (num_definitions_found != num_definitions)
        {
            std::cerr
                << "ERROR: Number of material param definitions found is incorrect. Found: "
                << num_definitions_found
                << ". Expected: "
                << num_definitions
                << "."
                << std::endl;
            assert(false);
        }

        new_pipeline.calculated.material_param_block_size_padded =
            reflected_material_param_block_size_padded;
    }

    // @TODO: @FUTURE: Do some more shader verification?? E.g. check that there's
    //   no other descriptor sets or push constants over or under the required amounts.

    // Create shader pipeline.
    VkShaderModule vert_shader;
    if (!vk_pipeline::load_shader_module(vert_shader_path,
                                         device,
                                         vert_shader))
    {
        std::cerr << "ERROR: Geom mat vertex shader module loading failed." << std::endl;
        assert(false);
    }

    VkShaderModule frag_shader;
    if (!vk_pipeline::load_shader_module(frag_shader_path,
                                         device,
                                         frag_shader))
    {
        std::cerr << "ERROR: Geom mat fragment shader module loading failed." << std::endl;
        assert(false);
    }

    // Create pipeline layout.
    std::vector<VkDescriptorSetLayout> descriptor_layouts;
    descriptor_layouts.resize(use_material_params ? 3 : 1);

    switch (camera_type)
    {
    case Camera_type::MAIN_VIEW:
        descriptor_layouts[0] = s_main_camera_descriptor_layout;
        break;

    case Camera_type::SHADOW_VIEW:
        // @TODO: add shadow camera descriptor layout.
        descriptor_layouts[0] = s_shadow_camera_descriptor_layout;
        assert(false);
        break;

    default:
        assert(false);
        break;
    }

    if (use_material_params)
    {
        descriptor_layouts[1] = s_material_sets_indexing_descriptor_layout;
        descriptor_layouts[2] = s_material_agnostic_descriptor_layout;
    }

    VkPushConstantRange pc_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(GPU_material_push_constant),
    };

    VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size()),
        .pSetLayouts = descriptor_layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    VkResult err{
        vkCreatePipelineLayout(device, &layout_info, nullptr, &new_pipeline.pipeline_layout) };
    if (err)
    {
        std::cerr << "ERROR: Pipeline layout creation failed." << std::endl;
        assert(false);
    }

    // Create pipeline.
    vk_pipeline::Graphics_pipeline_builder builder;
    builder.set_pipeline_layout(new_pipeline.pipeline_layout);
    builder.set_shaders(vert_shader, frag_shader);
    builder.set_vertex_input(gltf_loader::GPU_vertex::get_static_vertex_description());
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    builder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.set_multisampling_none();
    builder.disable_blending();

    if (has_z_prepass)
        builder.set_equal_nonwriting_depthtest();
    else
        builder.set_less_than_writing_depthtest();

    builder.set_color_attachment_format(draw_format);
    builder.set_depth_format(VK_FORMAT_UNDEFINED);
    new_pipeline.pipeline = builder.build_pipeline(device);

    // Clean up shader modules.
    vkDestroyShaderModule(device, vert_shader, nullptr);
    vkDestroyShaderModule(device, frag_shader, nullptr);

    // Assign creation idx.
    static std::atomic_uint32_t s_current_creation_idx{ 0 };
    new_pipeline.calculated.pipeline_creation_idx = s_current_creation_idx++;

    return new_pipeline;
}

uint32_t material_bank::register_pipeline(const std::string& pipe_name)
{
    assert(!pipe_name.empty());
    size_t emplace_idx;
    {
        std::lock_guard<std::mutex> lock{ s_all_pipelines_mutex };
        emplace_idx = s_all_pipelines.size();
        s_all_pipelines.emplace_back(GPU_pipeline{});
    }
    {
        std::lock_guard<std::mutex> lock{ s_pipe_name_to_idx_mutex };
        s_pipe_name_to_idx.emplace(std::string(pipe_name), static_cast<uint32_t>(emplace_idx));
    }
    return emplace_idx;
}

void material_bank::define_pipeline(const std::string& pipe_name,
                                    const std::string& optional_z_prepass_pipe_name,
                                    const std::string& optional_shadow_pipe_name,
                                    GPU_pipeline&& new_pipeline)
{
    if (!optional_shadow_pipe_name.empty())
    {
        new_pipeline.shadow_pipeline =
            &get_pipeline(
                get_pipeline_idx_from_name(optional_shadow_pipe_name));
    }

    if (!optional_z_prepass_pipe_name.empty())
    {
        new_pipeline.z_prepass_pipeline =
            &get_pipeline(
                get_pipeline_idx_from_name(optional_z_prepass_pipe_name));
    }

    {
        std::lock_guard<std::mutex> lock1{ s_all_pipelines_mutex };
        std::lock_guard<std::mutex> lock2{ s_pipe_name_to_idx_mutex };
        s_all_pipelines[s_pipe_name_to_idx.at(pipe_name)] = std::move(new_pipeline);
    }
}

uint32_t material_bank::get_pipeline_idx_from_name(const std::string& pipe_name)
{
    std::lock_guard<std::mutex> lock{ s_pipe_name_to_idx_mutex };
    auto it{ s_pipe_name_to_idx.find(pipe_name) };
    if (it == s_pipe_name_to_idx.end())
    {
        // Return invalid idx.
        assert(false);
        return (uint32_t)-1;
    }
    return it->second;
}

const material_bank::GPU_pipeline& material_bank::get_pipeline(uint32_t idx)
{
    std::lock_guard<std::mutex> lock{ s_all_pipelines_mutex };
    assert(idx < s_all_pipelines.size());
    return s_all_pipelines[idx];
}

bool material_bank::teardown_all_pipelines()
{
    // @TODO: implement.
    assert(false);
    return true;
}

// Material.
uint32_t material_bank::register_material(const std::string& mat_name,
                                          GPU_material&& new_material)
{
    size_t emplace_idx;
    {
        std::lock_guard<std::mutex> lock{ s_all_materials_mutex };
        emplace_idx = s_all_materials.size();
        s_all_materials.emplace_back(std::move(new_material));
    }
    {
        std::lock_guard<std::mutex> lock{ s_mat_name_to_idx_mutex };
        s_mat_name_to_idx.emplace(std::string(mat_name), static_cast<uint32_t>(emplace_idx));
    }
    return emplace_idx;
}

bool material_bank::cook_all_material_param_indices()
{
    std::lock_guard<std::mutex> lock1{ s_all_pipelines_mutex };
    std::lock_guard<std::mutex> lock2{ s_all_materials_mutex };

    std::vector<uint32_t> local_mat_counts;
    local_mat_counts.resize(s_all_pipelines.size(), 0);

    for (auto& material : s_all_materials)
    {
        uint32_t local_idx{ local_mat_counts[material.pipeline_idx]++ };
        material.cooked_material_param_local_idx = local_idx;
    }

    return true;
}

uint32_t material_bank::get_mat_idx_from_name(const std::string& mat_name)
{
    std::lock_guard<std::mutex> lock{ s_mat_name_to_idx_mutex };
    auto it{ s_mat_name_to_idx.find(mat_name) };
    if (it == s_mat_name_to_idx.end())
    {
        // Return invalid idx.
        assert(false);
        return (uint32_t)-1;
    }
    return it->second;
}

const material_bank::GPU_material& material_bank::get_material(uint32_t idx)
{
    std::lock_guard<std::mutex> lock{ s_all_materials_mutex };
    assert(idx < s_all_materials.size());
    return s_all_materials[idx];
}

bool material_bank::teardown_all_materials()
{
    // @TODO: implement.
    assert(false);
    return true;
}

// Material set.
uint32_t material_bank::register_material_set(const std::string& mat_set_name,
                                              GPU_material_set&& new_material_set)
{
    size_t emplace_idx;
    {
        std::lock_guard<std::mutex> lock{ s_all_material_sets_mutex };
        emplace_idx = s_all_material_sets.size();
        s_all_material_sets.emplace_back(std::move(new_material_set));
    }
    {
        std::lock_guard<std::mutex> lock{ s_mat_set_name_to_idx_mutex };
        s_mat_set_name_to_idx.emplace(std::string(mat_set_name), static_cast<uint32_t>(emplace_idx));
    }
    return emplace_idx;
}

uint32_t material_bank::get_mat_set_idx_from_name(const std::string& mat_set_name)
{
    std::lock_guard<std::mutex> lock{ s_mat_set_name_to_idx_mutex };
    auto it{ s_mat_set_name_to_idx.find(mat_set_name) };
    if (it == s_mat_set_name_to_idx.end())
    {
        // Return invalid idx.
        assert(false);
        return (uint32_t)-1;
    }
    return it->second;
}

const material_bank::GPU_material_set& material_bank::get_material_set(uint32_t idx)
{
    std::lock_guard<std::mutex> lock{ s_all_material_sets_mutex };
    assert(idx < s_all_material_sets.size());
    return s_all_material_sets[idx];
}

const std::vector<material_bank::GPU_material_set>& material_bank::get_all_material_sets()
{
    std::lock_guard<std::mutex> lock{ s_all_material_sets_mutex };
    return s_all_material_sets;
}
