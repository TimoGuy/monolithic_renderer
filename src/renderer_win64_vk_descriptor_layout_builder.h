#pragma once

#if _WIN64

#include <vector>
#include <vulkan/vulkan.h>


namespace vk_desc
{

struct DescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device,
                                VkShaderStageFlags shader_stages,
                                void* pNext = nullptr,
                                VkDescriptorSetLayoutCreateFlags flags = 0);
};

}  // namespace vk_desc

#endif  // _WIN64
