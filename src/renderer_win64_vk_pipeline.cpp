#include "renderer_win64_vk_pipeline.h"

#include <fstream>
#include <iostream>
#include <vector>


bool vk_pipeline::load_shader_module(const char* file_path,
                                     VkDevice device,
                                     VkShaderModule& out_shader_module)
{
    std::ifstream file{ file_path, std::ios::ate | std::ios::binary };
    if (!file.is_open())
    {
        std::cerr << "ERROR: shader module not found: " << file_path << std::endl;
        return false;
    }

    // Read spir-v.
    size_t file_size{ static_cast<size_t>(file.tellg()) };
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();

    // Create shader module.
    VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = (buffer.size() * sizeof(uint32_t)),
        .pCode = buffer.data(),
    };

    VkShaderModule shader_module;
    VkResult err{
        vkCreateShaderModule(device, &create_info, nullptr, &shader_module) };
    if (err)
    {
        std::cerr << "ERROR: creating shader module failed: " << file_path << std::endl;
        return false;
    }
    out_shader_module = shader_module;

    return true;
}
