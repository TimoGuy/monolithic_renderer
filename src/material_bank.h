#pragma once

#include <cinttypes>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "cglm/types.h"


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

struct Material_parameter_data
{
    std::string param_name;

    union Data
    {
        int32_t  _int;
        uint32_t _uint;
        float_t  _float;
        ivec2    _ivec2;
        ivec3    _ivec3;
        ivec4    _ivec4;
        vec2     _vec2;
        vec3     _vec3;
        vec4     _vec4;
        mat3     _mat3;
        mat4     _mat4;
        uint32_t _texture_idx;
    } data;
};

struct GPU_pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    const GPU_pipeline* shadow_pipeline{ nullptr };
    const GPU_pipeline* z_prepass_pipeline{ nullptr };

    Camera_type camera_type;
    std::vector<Material_parameter_definition> material_param_definitions;
    VkDescriptorSet combined_all_material_datas_descriptor_set;

    // @NOTE: Calculated in construction.
    struct Calculated
    {
        uint32_t pipeline_creation_idx;
        size_t material_param_block_size_padded{ 0 };
    } calculated;

    void bind_pipeline(VkCommandBuffer cmd,
                       const VkViewport& viewport,
                       const VkRect2D& scissor,
                       const VkDescriptorSet* main_view_camera_descriptor_set,
                       const VkDescriptorSet* shadow_view_camera_descriptor_set,
                       VkDeviceAddress instance_data_buffer_address) const;
};

constexpr uint32_t k_invalid_material_idx{ (uint32_t)-1 };

struct GPU_material
{
    uint32_t pipeline_idx;
    std::vector<Material_parameter_data> material_param_datas;

    // Local index within the group of materials using a pipeline.
    uint32_t cooked_material_param_local_idx;
};

struct GPU_material_set
{
    std::vector<uint32_t> material_indexes;
};

// Passing references.
void set_descriptor_references(
    VkDescriptorSetLayout main_camera_descriptor_layout,
    VkDescriptorSetLayout shadow_camera_descriptor_layout,
    VkDescriptorSetLayout material_sets_indexing_descriptor_layout,
    VkDescriptorSetLayout material_agnostic_descriptor_layout,
    VkDescriptorSet material_sets_indexing_descriptor_set);

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
