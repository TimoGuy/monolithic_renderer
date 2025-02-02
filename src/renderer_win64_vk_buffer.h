#pragma once

#if _WIN64

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include "cglm/cglm.h"


namespace vk_util{ struct Immediate_submit_support; }

namespace vk_buffer
{

struct Allocated_buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

Allocated_buffer create_buffer(VmaAllocator allocator,
                               size_t size,
                               VkBufferUsageFlags usage,
                               VmaMemoryUsage memory_usage);

void destroy_buffer(VmaAllocator allocator,
                    const Allocated_buffer& buffer);

struct GPU_mesh_buffer
{
    Allocated_buffer index_buffer;
    Allocated_buffer vertex_buffer;
    VkDeviceAddress  vertex_buffer_address;
};

struct GPU_vertex
{
    vec3    position;
	float_t uv_x;
	vec3    normal;
	float_t uv_y;
	vec4    color;
};

GPU_mesh_buffer upload_mesh_to_gpu(const vk_util::Immediate_submit_support &support,
                                   VkDevice device,
                                   VkQueue queue,
                                   VmaAllocator allocator,
                                   std::vector<uint32_t>&& indices,
                                   std::vector<GPU_vertex>&& vertices);

}  // namespace vk_buffer

#endif  // _WIN64
