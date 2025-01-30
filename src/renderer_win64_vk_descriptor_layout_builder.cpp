#include "renderer_win64_vk_descriptor_layout_builder.h"

#include <cassert>
#include <iostream>



void vk_desc::DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding new_binding{
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
    };
    bindings.push_back(new_binding);
}

void vk_desc::DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout vk_desc::DescriptorLayoutBuilder::build(VkDevice device,
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
        std::cerr << "ERROR: Descriptor set layout creation failed" << std::endl;
        assert(false);
    }

    return set_layout;
}
