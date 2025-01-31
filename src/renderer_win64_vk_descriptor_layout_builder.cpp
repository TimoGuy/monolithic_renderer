#include "renderer_win64_vk_descriptor_layout_builder.h"

#include <cassert>
#include <iostream>



void vk_desc::Descriptor_layout_builder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding new_binding{
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
    };
    bindings.push_back(new_binding);
}

void vk_desc::Descriptor_layout_builder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout vk_desc::Descriptor_layout_builder::build(VkDevice device,
                                                              VkShaderStageFlags shader_stages,
                                                              void* pNext /*= nullptr*/,
                                                              VkDescriptorSetLayoutCreateFlags flags /*= 0*/)
{
    for (auto& binding : bindings)
    {
        binding.stageFlags |= shader_stages;
    }

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout set_layout;
    VkResult err{
        vkCreateDescriptorSetLayout(device, &info, nullptr, &set_layout)
    };
    if (err)
    {
        std::cerr << "ERROR: Descriptor set layout creation failed." << std::endl;
        assert(false);
    }

    return set_layout;
}

void vk_desc::Descriptor_allocator::init_pool(VkDevice device,
                                              uint32_t max_sets,
                                              std::span<Pool_size_ratio> pool_ratios)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (Pool_size_ratio ratio : pool_ratios)
    {
        pool_sizes.push_back(
            VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = static_cast<uint32_t>(ratio.ratio * max_sets),
            }
        );
    }

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void vk_desc::Descriptor_allocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void vk_desc::Descriptor_allocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet vk_desc::Descriptor_allocator::allocate(VkDevice device,
                                                        VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet set;
    VkResult err{
        vkAllocateDescriptorSets(device, &alloc_info, &set) };
    if (err)
    {
        std::cerr << "ERROR: Allocate descriptor set failed." << std::endl;
    }

    return set;
}
