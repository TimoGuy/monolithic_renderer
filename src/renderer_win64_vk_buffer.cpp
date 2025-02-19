#include "renderer_win64_vk_buffer.h"

#include <cassert>
#include <iostream>
#include <vk_mem_alloc.h>
#include "geo_instance.h"
#include "renderer_win64_vk_immediate_submit.h"


namespace vk_buffer
{

// @TODO: delete this! @THEA: I feel like for now I'm not gonna bother with changed indices until the instance pool data structure is more set in stone.
// static std::vector<uint32_t> s_changed_indices;

}  // namespace vk_buffer


vk_buffer::Allocated_buffer vk_buffer::create_buffer(VmaAllocator allocator,
                                                     size_t size,
                                                     VkBufferUsageFlags usage,
                                                     VmaMemoryUsage memory_usage)
{
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = usage,
    };

    VmaAllocationCreateInfo alloc_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = memory_usage,
    };

    Allocated_buffer new_buffer;
    VkResult err{
        vmaCreateBuffer(allocator,
                        &buffer_info,
                        &alloc_info,
                        &new_buffer.buffer,
                        &new_buffer.allocation,
                        &new_buffer.info) };
    if (err)
    {
        std::cerr << "ERROR: Buffer creation failed." << std::endl;
        assert(false);
    }

    return new_buffer;
}

void vk_buffer::destroy_buffer(VmaAllocator allocator,
                               const Allocated_buffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

bool vk_buffer::expand_buffer(const vk_util::Immediate_submit_support& support,
                              VkDevice device,
                              VkQueue queue,
                              VmaAllocator allocator,
                              Allocated_buffer& in_out_buffer,
                              size_t new_size,
                              VkBufferUsageFlags usage,
                              VmaMemoryUsage memory_usage)
{
    size_t old_size{ in_out_buffer.info.size };
    if (new_size < old_size)
    {
        std::cerr << "ERROR: new_size is smaller than old_size for expansion." << std::endl;
        assert(false);
        return false;
    }

    // Create new buffer, copy contents of old to new, and delete old buffer.
    Allocated_buffer new_buffer{
        create_buffer(allocator, new_size, usage, memory_usage) };
    
    vk_util::immediate_submit(support, device, queue, [&](VkCommandBuffer cmd) {
        VkBufferCopy old_to_new_copy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = old_size,
        };
        vkCmdCopyBuffer(cmd, in_out_buffer.buffer, new_buffer.buffer, 1, &old_to_new_copy);
    });

    destroy_buffer(allocator, in_out_buffer);

    in_out_buffer = new_buffer;

    return true;
}

vk_buffer::GPU_mesh_buffer vk_buffer::upload_mesh_to_gpu(const vk_util::Immediate_submit_support& support,
                                                         VkDevice device,
                                                         VkQueue queue,
                                                         VmaAllocator allocator,
                                                         std::vector<uint32_t>&& indices,
                                                         std::vector<GPU_vertex>&& vertices)
{
    const size_t index_buffer_size{ indices.size() * sizeof(uint32_t) };
    const size_t vertex_buffer_size{ vertices.size() * sizeof(GPU_vertex) };

    // Create mesh buffer.
    GPU_mesh_buffer new_mesh{
        .index_buffer{
            create_buffer(allocator,
                          index_buffer_size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY)
        },
        .vertex_buffer{
            create_buffer(allocator,
                          vertex_buffer_size,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY)
        },
    };

    VkBufferDeviceAddressInfo device_address_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = new_mesh.vertex_buffer.buffer,
    };
    new_mesh.vertex_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);

    // Upload data.
    Allocated_buffer staging_buffer{
        create_buffer(allocator,
                      index_buffer_size + vertex_buffer_size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY) };

    // Copy index buffer and vertex buffer.
    void* data;
    vmaMapMemory(allocator, staging_buffer.allocation, &data);
    memcpy(data, indices.data(), index_buffer_size);
    memcpy((char*)data + index_buffer_size, vertices.data(), vertex_buffer_size);
    vmaUnmapMemory(allocator, staging_buffer.allocation);

    vk_util::immediate_submit(support, device, queue, [&](VkCommandBuffer cmd) {
        VkBufferCopy indices_copy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = index_buffer_size,
        };
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_mesh.index_buffer.buffer, 1, &indices_copy);
        VkBufferCopy vertices_copy{
            .srcOffset = index_buffer_size,
            .dstOffset = 0,
            .size = vertex_buffer_size,
        };
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_mesh.vertex_buffer.buffer, 1, &vertices_copy);
    });

    destroy_buffer(allocator, staging_buffer);

    return new_mesh;
}

bool vk_buffer::upload_material_param_sets_to_gpu(
    GPU_geo_resource_buffer& out_resources,
    const vk_util::Immediate_submit_support& support,
    VkDevice device,
    VkQueue queue,
    VmaAllocator allocator,
    const std::vector<material_bank::GPU_material_set>& all_material_sets)
{
    // Write material param indices (`cooked_material_param_local_idx`)
    // (@NOTE: different from material idx)
    size_t num_mat_param_indices{ 0 };
    for (auto& mat_set : all_material_sets)
        num_mat_param_indices += mat_set.material_indexes.size();

    std::vector<uint32_t> flat_mat_param_indices;
    flat_mat_param_indices.reserve(num_mat_param_indices);

    std::vector<uint32_t> mat_param_set_start_indices;
    mat_param_set_start_indices.reserve(all_material_sets.size());

    for (auto& mat_set : all_material_sets)
    {
        mat_param_set_start_indices.emplace_back(flat_mat_param_indices.size());
        for (auto mat_idx : mat_set.material_indexes)
        {
            auto& material{ material_bank::get_material(mat_idx) };
            flat_mat_param_indices.emplace_back(
                material.cooked_material_param_local_idx);
        }
    }

    // Write to GPU.
    size_t mat_param_indices_buffer_size{
        sizeof(uint32_t) * flat_mat_param_indices.size() };
    auto& mat_param_indices_buffer{ out_resources.material_param_index_buffer };
    mat_param_indices_buffer =
        create_buffer(allocator,
                      mat_param_indices_buffer_size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    size_t mat_param_sets_buffer_size{
        sizeof(uint32_t) * mat_param_set_start_indices.size() };
    auto& mat_param_sets_buffer{ out_resources.material_param_set_buffer };
    mat_param_sets_buffer =
        create_buffer(allocator,
                      sizeof(uint32_t) * mat_param_set_start_indices.size(),
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    auto staging_buffer{
        create_buffer(allocator,
                      mat_param_indices_buffer_size + mat_param_sets_buffer_size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY) };

    void* data;
    vmaMapMemory(allocator, staging_buffer.allocation, &data);
    memcpy(data, flat_mat_param_indices.data(), mat_param_indices_buffer_size);
    memcpy((char*)data + mat_param_indices_buffer_size, mat_param_set_start_indices.data(), mat_param_sets_buffer_size);
    vmaUnmapMemory(allocator, staging_buffer.allocation);

    vk_util::immediate_submit(support, device, queue, [&](VkCommandBuffer cmd) {
        VkBufferCopy mat_params_copy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = mat_param_indices_buffer_size,
        };
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, mat_param_indices_buffer.buffer, 1, &mat_params_copy);
        VkBufferCopy mat_sets_copy{
            .srcOffset = mat_param_indices_buffer_size,
            .dstOffset = 0,
            .size = mat_param_sets_buffer_size,
        };
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, mat_param_sets_buffer.buffer, 1, &mat_sets_copy);
    });

    destroy_buffer(allocator, staging_buffer);

    return true;
}

bool vk_buffer::upload_bounding_spheres_to_gpu(
    GPU_geo_resource_buffer& out_resources,
    const vk_util::Immediate_submit_support& support,
    VkDevice device,
    VkQueue queue,
    VmaAllocator allocator,
    const std::vector<gpu_geo_data::GPU_bounding_sphere>& all_bounding_spheres)
{
    static_assert(sizeof(gpu_geo_data::GPU_bounding_sphere) == sizeof(vec4));

    size_t bs_buffer_size{
        sizeof(gpu_geo_data::GPU_bounding_sphere) * all_bounding_spheres.size() };

    // Write to GPU.
    out_resources.bounding_sphere_buffer =
        create_buffer(allocator,
                      bs_buffer_size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    auto staging_buffer{
        create_buffer(allocator,
                      bs_buffer_size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY) };

    void* data;
    vmaMapMemory(allocator, staging_buffer.allocation, &data);
    memcpy(data, all_bounding_spheres.data(), bs_buffer_size);
    vmaUnmapMemory(allocator, staging_buffer.allocation);

    vk_util::immediate_submit(support, device, queue, [&](VkCommandBuffer cmd) {
        VkBufferCopy bounding_spheres_copy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = bs_buffer_size,
        };
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, out_resources.bounding_sphere_buffer.buffer, 1, &bounding_spheres_copy);
    });

    destroy_buffer(allocator, staging_buffer);

    return true;
}

void vk_buffer::initialize_base_sized_per_frame_buffer(VmaAllocator allocator, GPU_geo_per_frame_buffer& frame_buffer)
{
    size_t capacity{ frame_buffer.expand_elems_interval };

    frame_buffer.instance_data_buffer =
        create_buffer(allocator,
                      sizeof(gpu_geo_data::GPU_geo_instance_data) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    frame_buffer.num_instance_data_elems = 0;
    frame_buffer.num_instance_data_elem_capacity = capacity;

    frame_buffer.indirect_command_buffer =
        create_buffer(allocator,
                      sizeof(VkDrawIndexedIndirectCommand) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    frame_buffer.culled_indirect_command_buffer =
        create_buffer(allocator,
                      sizeof(VkDrawIndexedIndirectCommand) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
    frame_buffer.num_indirect_cmd_elems = 0;
    frame_buffer.num_indirect_cmd_elem_capacity = capacity;
}

bool vk_buffer::set_new_changed_indices(std::vector<uint32_t>&& changed_indices,
                                        std::vector<GPU_geo_per_frame_buffer*>& all_per_frame_buffers)
{
    // @TODO: START HERE!!!!!! Think about what you want to do as far as the changed indices function.
    //        It would honestly be nice to only update the indices that have changed within the buffer.
    //        Idk... think about it!!!!

    // Ignore empty changed indices lists.
    if (changed_indices.empty())
    {
        std::cerr << "WARNING: Empty `changed_indices` list passed in. Ignoring." << std::endl; 
        return true;
    }

    // Check that all previous changes have propagated.
    for (auto& frame_buffer : all_per_frame_buffers)
    {
        if (!frame_buffer->changes_processed)
        {
            std::cerr << "WARNING: Previously submitted changes have not finished propagation." << std::endl;
            return false;
        }
    }

    // Replace changed indices.
    s_changed_indices = std::move(changed_indices);
    std::sort(s_changed_indices.begin(), s_changed_indices.end());
    for (auto& frame_buffer : all_per_frame_buffers)
    {
        frame_buffer->changes_processed = false;
    }

    return true;
}

void vk_buffer::upload_changed_per_frame_data(const vk_util::Immediate_submit_support& support,
                                              VkDevice device,
                                              VkQueue queue,
                                              VmaAllocator allocator,
                                              GPU_geo_per_frame_buffer& frame_buffer)
{
    // Exit if no changes.
    if (frame_buffer.changes_processed)
        return;

    // @TODO: Maybe there could be a more optimal way of doing this but as
    //        long as it's not gonna be too slow, we're just gonna rewrite
    //        the whole buffer for both instance and indirect buffers.

    // Assign ids to all instances.
    auto unique_instances{ geo_instance::get_all_unique_instances() };
    for (size_t i = 0; i < unique_instances.size(); i++)
    {
        auto& inst{ unique_instances[i] };
        inst->cooked_buffer_instance_id = static_cast<uint32_t>(i);
    }

    // Update instance data buffer sizing.
    if (unique_instances.size() > frame_buffer.num_instance_data_elem_capacity)
    {
        size_t new_capacity{
            unique_instances.size() +
                (unique_instances.size() % frame_buffer.expand_elems_interval) };
        expand_buffer(support,
                      device,
                      queue,
                      allocator,
                      frame_buffer.instance_data_buffer,
                      sizeof(gpu_geo_data::GPU_geo_instance_data) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame_buffer.num_instance_data_elem_capacity = new_capacity;
    }

    // Upload instance data.
    {
        gpu_geo_data::GPU_geo_instance_data* data;
        vmaMapMemory(allocator,
                     frame_buffer.instance_data_buffer.allocation,
                     reinterpret_cast<void**>(&data));
        for (auto inst : unique_instances)
        {
            memcpy(data,
                   &inst->gpu_instance_data,
                   sizeof(gpu_geo_data::GPU_geo_instance_data));
            data++;
        }
        vmaUnmapMemory(allocator,
                       frame_buffer.instance_data_buffer.allocation);
    }

    // Update indirect cmd buffer sizing.
    auto all_primitives{ geo_instance::get_all_primitives() };
    if (all_primitives.size() > frame_buffer.num_indirect_cmd_elem_capacity)
    {
        size_t new_capacity{
            all_primitives.size() +
                (all_primitives.size() %
                    frame_buffer.expand_elems_interval) };
        expand_buffer(support,
                      device,
                      queue,
                      allocator,
                      frame_buffer.indirect_command_buffer,
                      sizeof(VkDrawIndexedIndirectCommand) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Simply destroy and recreate buffer since no copying needed.
        destroy_buffer(allocator, frame_buffer.culled_indirect_command_buffer);
        frame_buffer.culled_indirect_command_buffer =
            create_buffer(allocator,
                          sizeof(VkDrawIndexedIndirectCommand) * new_capacity,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);

        frame_buffer.num_indirect_cmd_elem_capacity = new_capacity;
    }

    // Upload indirect data.
    std::vector<VkDrawIndexedIndirectCommand> new_indirect_cmds;
    new_indirect_cmds.reserve(all_primitives.size());
    for (auto prim : all_primitives)
    {
        new_indirect_cmds.emplace_back(                // VkDrawIndexedIndirectCommand
            prim->primitive->index_count,              // .indexCount
            1,                                         // .instanceCount
            prim->primitive->start_index,              // .firstIndex
            0,                                         // .vertexOffset
            prim->instance->cooked_buffer_instance_id  // .firstInstance
        );
    }
    {
        void* data;
        vmaMapMemory(allocator,
                     frame_buffer.indirect_command_buffer.allocation,
                     &data);
        memcpy(data,
               new_indirect_cmds.data(),
               sizeof(VkDrawIndexedIndirectCommand) * new_indirect_cmds.size());
        vmaUnmapMemory(allocator,
                       frame_buffer.indirect_command_buffer.allocation);
    }

    frame_buffer.changes_processed = true;
}
