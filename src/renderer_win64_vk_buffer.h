#pragma once

#if _WIN64

#include <atomic>
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

bool expand_buffer(const vk_util::Immediate_submit_support& support,
                   VkDevice device,
                   VkQueue queue,
                   VmaAllocator allocator,
                   Allocated_buffer& in_out_buffer,
                   size_t old_size,
                   size_t new_size,
                   VkBufferUsageFlags usage,
                   VmaMemoryUsage memory_usage);

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
    // @NOTE: All of these buffers are immutable.
    //   All material param sets and their combinations should be frontloaded in every combination 
    // they're designed to be in (i.e. they're deterministic).
    //
    //   I'm just writing this down bc I ended up trying to think about how to make these dynamic
    // bc I thought from their context they would definitely be mutable. Perhaps in the future it
    // may be good to have mutable material param sets???? But only if there's a REALLY good reason
    // for it.  -Thea 2025/02/25
    Allocated_buffer material_param_index_buffer;  // @TODO: add `local_` at beginning.
    size_t material_param_index_buffer_size;
    Allocated_buffer material_param_set_buffer;
    size_t material_param_set_buffer_size;
    Allocated_buffer bounding_sphere_buffer;  // @NOTE: this one does not change in size!!! Since it's fixed to the 
    size_t bounding_sphere_buffer_size;
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
    std::atomic_size_t num_instance_data_elems{ 0 };
    std::atomic_size_t num_instance_data_elem_capacity{ 0 };

    Allocated_buffer indirect_command_buffer;
    Allocated_buffer culled_indirect_command_buffer;
    std::atomic_size_t num_indirect_cmd_elems{ 0 };
    std::atomic_size_t num_indirect_cmd_elem_capacity{ 0 };

    Allocated_buffer indirect_counts_buffer;
    std::atomic_size_t num_indirect_counts_elems{ 0 };
    std::atomic_size_t num_indirect_counts_elem_capacity{ 0 };

    const size_t expand_elems_interval{ 1024 };
    const size_t expand_count_elems_interval{ 32 };

    std::atomic_bool changes_processed{ true };
};

void initialize_base_sized_per_frame_buffer(VmaAllocator allocator,
                                            GPU_geo_per_frame_buffer& frame_buffer);

// // @NOTE: Returns false if the previous requested change did not finish
// //        propagating to all the frames. False means "try again later".
// bool set_new_changed_indices(std::vector<uint32_t>&& changed_indices,
//                              std::vector<GPU_geo_per_frame_buffer*>& all_per_frame_buffers);

void flag_update_all_instances(std::vector<GPU_geo_per_frame_buffer*>& all_per_frame_buffers);

void upload_changed_per_frame_data(const vk_util::Immediate_submit_support& support,
                                   VkDevice device,
                                   VkQueue queue,
                                   VmaAllocator allocator,
                                   GPU_geo_per_frame_buffer& frame_buffer);


}  // namespace vk_buffer

#endif  // _WIN64
