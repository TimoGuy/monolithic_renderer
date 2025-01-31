#pragma once

#if _WIN64

#include <span>
#include <vector>
#include <vulkan/vulkan.h>


namespace vk_desc
{

struct Descriptor_layout_builder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device,
                                VkShaderStageFlags shader_stages,
                                void* pNext = nullptr,
                                VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct Descriptor_allocator
{
    struct Pool_size_ratio
    {
        VkDescriptorType type;
        float_t ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device,
                   uint32_t max_sets,
                   std::span<Pool_size_ratio> pool_ratios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

}  // namespace vk_desc

#endif  // _WIN64
