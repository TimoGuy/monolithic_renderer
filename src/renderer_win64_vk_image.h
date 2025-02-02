#pragma once

#if _WIN64

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>


namespace vk_image
{

struct Allocated_image
{
    VkImage       image;
    VkImageView   image_view;
    VmaAllocation allocation;
    VkExtent3D    image_extent;
    VkFormat      image_format;
};

}  // namespace vk_image

#endif  // _WIN64
