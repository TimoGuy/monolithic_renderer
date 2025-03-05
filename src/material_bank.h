#pragma once

#include <cinttypes>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>


// @NOTE: A material is only for the geometry pipelines.
namespace material_bank
{

// GPU_pipeline helpers.
enum class Camera_type
{
    MAIN_VIEW = 0,
    SHADOW_VIEW,
    NUM_CAMERA_TYPES
};

enum class Mat_param_def_type
{
    UNSUPPORTED_TYPE = -1,

    INT = 0,
    UINT,
    FLOAT,

    IVEC2, IVEC3, IVEC4,
    UVEC2, UVEC3, UVEC4,
     VEC2,  VEC3,  VEC4,
    
    MAT3, MAT4,

    TEXTURE_NAME,  // @NOTE: Computes down to a uint32_t of the texture idx.

    NUM_TYPES
};

struct Material_parameter_definition
{
    std::string param_name;
    Mat_param_def_type param_type{ Mat_param_def_type::UNSUPPORTED_TYPE };

    // @NOTE: Calculated in construction.
    struct Calculated
    {
        size_t param_block_offset;
        size_t param_size_padded;
    } calculated;
};

struct GPU_pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    const GPU_pipeline* shadow_pipeline{ nullptr };
    const GPU_pipeline* z_prepass_pipeline{ nullptr };

    Camera_type camera_type;
    std::vector<Material_parameter_definition> material_param_definitions;

    // @NOTE: Calculated in construction.
    struct Calculated
    {
        size_t material_param_block_size_padded{ 0 };
    } calculated;
};

constexpr uint32_t k_invalid_material_idx{ (uint32_t)-1 };

struct GPU_material
{
    uint32_t pipeline_idx;
    // @TODO: add different material param things for adding into the gpu.

    // Local index within the group of materials using a pipeline.
    uint32_t cooked_material_param_local_idx;
};

struct GPU_material_set
{
    std::vector<uint32_t> material_indexes;
};

// Passing references.
void set_descriptor_layout_references(
    VkDescriptorSetLayout main_camera_descriptor_layout,
    VkDescriptorSetLayout shadow_camera_descriptor_layout,
    VkDescriptorSetLayout material_sets_indexing_descriptor_layout,
    VkDescriptorSetLayout material_agnostic_descriptor_layout);

// Pipeline.
GPU_pipeline create_geometry_material_pipeline(
    VkDevice device,
    VkFormat draw_format,
    bool has_z_prepass,
    Camera_type camera_type,
    bool use_material_params,
    std::vector<Material_parameter_definition>&& material_param_definitions,
    const char* vert_shader_path,
    const char* frag_shader_path);

uint32_t register_pipeline(const std::string& pipe_name);

void define_pipeline(const std::string& pipe_name,
                     const std::string& optional_z_prepass_pipe_name,
                     const std::string& optional_shadow_pipe_name,
                     GPU_pipeline&& new_pipeline);

uint32_t get_pipeline_idx_from_name(const std::string& pipe_name);

const GPU_pipeline& get_pipeline(uint32_t idx);

bool teardown_all_pipelines();

// Material.
uint32_t register_material(const std::string& mat_name,
                           GPU_material&& new_material);

bool cook_all_material_param_indices();

uint32_t get_mat_idx_from_name(const std::string& mat_name);

const GPU_material& get_material(uint32_t idx);

bool teardown_all_materials();

// Material set.
uint32_t register_material_set(const std::string& mat_set_name,
                               GPU_material_set&& new_material_set);

uint32_t get_mat_set_idx_from_name(const std::string& mat_set_name);

const GPU_material_set& get_material_set(uint32_t idx);

const std::vector<GPU_material_set>& get_all_material_sets();

}  // namespace material_bank
