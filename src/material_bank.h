#pragma once

#include <cinttypes>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>


// @NOTE: A material is only for the geometry pipelines.
namespace material_bank
{

struct GPU_pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    std::vector<VkDescriptorSet> descriptor_sets;
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

// Pipeline.
uint32_t register_pipeline(const std::string& pipe_name,
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

}  // namespace material_bank
