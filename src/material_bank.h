#pragma once

#include <cinttypes>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>


namespace material_bank
{

constexpr uint32_t k_invalid_material_idx{ (uint32_t)-1 };

struct GPU_material  // @CHECK I think this would be what I need for making materials.
{
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    std::vector<VkDescriptorSet> descriptor_sets;
};

uint32_t register_material(const std::string& mat_name,
                           GPU_material&& new_material);

uint32_t get_mat_idx_from_name(const std::string& mat_name);

const GPU_material& get_material(uint32_t idx);

}  // namespace material_bank
