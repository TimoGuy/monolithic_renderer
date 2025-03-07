#pragma once

#if _WIN64

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>


namespace vk_buffer
{

struct Allocated_buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

}  // namespace vk_buffer

#endif  // _WIN64
