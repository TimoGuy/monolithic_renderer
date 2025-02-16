#pragma once

#if _WIN64

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include "cglm/cglm.h"
#include "gltf_loader.h"
#include "gpu_geo_data.h"
#include "material_bank.h"


namespace vk_util{ struct Immediate_submit_support; }
using GPU_vertex = gltf_loader::GPU_vertex;

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

GPU_mesh_buffer upload_mesh_to_gpu(const vk_util::Immediate_submit_support& support,
                                   VkDevice device,
                                   VkQueue queue,
                                   VmaAllocator allocator,
                                   std::vector<uint32_t>&& indices,
                                   std::vector<GPU_vertex>&& vertices);

struct GPU_geo_resource_buffer
{
    Allocated_buffer material_param_index_buffer;
    Allocated_buffer material_param_set_buffer;
    Allocated_buffer bounding_sphere_buffer;
};

// @NOTE: Mutates both `material_param_index_buffer` and `material_param_set_buffer`.
bool upload_material_param_sets_to_gpu(GPU_geo_resource_buffer& out_resources,
                                       const vk_util::Immediate_submit_support& support,
                                       VkDevice device,
                                       VkQueue queue,
                                       VmaAllocator allocator,
                                       const std::vector<material_bank::GPU_material_set>& all_material_sets);

bool upload_bounding_spheres_to_gpu(GPU_geo_resource_buffer& out_resources,
                                    const vk_util::Immediate_submit_support& support,
                                    VkDevice device,
                                    VkQueue queue,
                                    VmaAllocator allocator,
                                    const std::vector<gpu_geo_data::GPU_bounding_sphere>& all_bounding_spheres);

struct GPU_geo_per_frame_buffer
{
    Allocated_buffer instance_data_buffer;
    Allocated_buffer indirect_command_buffer;
};

}  // namespace vk_buffer

#endif  // _WIN64
