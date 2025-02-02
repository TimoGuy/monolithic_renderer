#pragma once

#if _WIN64

#include <vulkan/vulkan.h>

// @TODO: MOVE THESE FUNCTIONS TO PIPELINE_BUILDER.h/cpp
namespace vk_pipeline
{

bool load_shader_module(const char* file_path,
                        VkDevice device,
                        VkShaderModule& out_shader_module);

}  // namespace vk_pipeline

#endif  // _WIN64
