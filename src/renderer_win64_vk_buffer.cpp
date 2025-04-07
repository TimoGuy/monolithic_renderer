#include "renderer_win64_vk_buffer.h"

#if _DEBUG
#include <atomic>
#endif  // _DEBUG
#include <cassert>
#include <iostream>
#include <vk_mem_alloc.h>
#include "geo_instance.h"
#include "renderer_win64_vk_immediate_submit.h"
#include "transform_read_ifc.h"  // From ticking_world_simulation component.


namespace vk_buffer
{

// @TODO: delete this! @THEA: I feel like for now I'm not gonna bother with changed indices until the instance pool data structure is more set in stone.
// static std::vector<uint32_t> s_changed_indices;

#if _DEBUG
    // Since these should be immutable, assert that they aren't being attempted to change.
    std::atomic_uint32_t s_num_times_uploaded_mesh{ 0 };
    std::atomic_uint32_t s_num_times_uploaded_material_param_sets{ 0 };
    std::atomic_uint32_t s_num_times_uploaded_bounding_spheres{ 0 };
#endif  // _DEBUG

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
                              size_t old_size,
                              size_t new_size,
                              VkBufferUsageFlags usage,
                              VmaMemoryUsage memory_usage)
{
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
    assert((s_num_times_uploaded_mesh++) == 0);

    const size_t index_buffer_size{ indices.size() * sizeof(uint32_t) };
    const size_t vertex_buffer_size{ vertices.size() * sizeof(GPU_vertex) };

    // Create mesh buffer.
    GPU_mesh_buffer new_mesh{
        .index_buffer{
            create_buffer(allocator,
                          index_buffer_size,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY)
        },
        .vertex_buffer{
            create_buffer(allocator,
                          vertex_buffer_size,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY)
        },
    };

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
    assert((s_num_times_uploaded_material_param_sets++) == 0);

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

    out_resources.material_param_index_buffer_size =
        mat_param_indices_buffer_size;

    size_t mat_param_sets_buffer_size{
        sizeof(uint32_t) * mat_param_set_start_indices.size() };
    auto& mat_param_sets_buffer{ out_resources.material_param_set_buffer };
    mat_param_sets_buffer =
        create_buffer(allocator,
                      mat_param_sets_buffer_size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    out_resources.material_param_set_buffer_size =
        mat_param_sets_buffer_size;

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
    assert((s_num_times_uploaded_bounding_spheres++) == 0);
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

    out_resources.bounding_sphere_buffer_size =
        bs_buffer_size;

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

void vk_buffer::initialize_base_sized_per_frame_buffer(VkDevice device,
                                                       VmaAllocator allocator,
                                                       GPU_geo_per_frame_buffer& frame_buffer)
{
    size_t capacity{ frame_buffer.expand_elems_interval };
    size_t count_capacity{ frame_buffer.expand_count_elems_interval };

    // Get buffer device addresses.
    VkBufferDeviceAddressInfo device_address_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    };

    // Instance data buffer.
    frame_buffer.instance_data_buffer =
        create_buffer(allocator,
                      sizeof(gpu_geo_data::GPU_geo_instance_data) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    device_address_info.buffer = frame_buffer.instance_data_buffer.buffer;
    frame_buffer.instance_data_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.num_instance_data_elems = 0;
    frame_buffer.num_instance_data_elem_capacity = capacity;

    // Visible result buffer.
    frame_buffer.visible_result_buffer =
        create_buffer(allocator,
                      sizeof(uint32_t) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
    device_address_info.buffer = frame_buffer.visible_result_buffer.buffer;
    frame_buffer.visible_result_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.num_visible_result_elems = 0;
    frame_buffer.num_visible_result_elem_capacity = capacity;

    // Primitive group base indices buffer.
    frame_buffer.primitive_group_base_index_buffer =
        create_buffer(allocator,
                      sizeof(uint32_t) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    device_address_info.buffer = frame_buffer.primitive_group_base_index_buffer.buffer;
    frame_buffer.primitive_group_base_index_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.num_primitive_group_base_index_elems = 0;
    frame_buffer.num_primitive_group_base_index_elem_capacity = capacity;

    // Count buffer indices buffer.
    frame_buffer.count_buffer_index_buffer =
        create_buffer(allocator,
                      sizeof(uint32_t) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    device_address_info.buffer = frame_buffer.count_buffer_index_buffer.buffer;
    frame_buffer.count_buffer_index_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.num_count_buffer_index_elems = 0;
    frame_buffer.num_count_buffer_index_elem_capacity = capacity;

    // Indirect draw cmds input/output buffer.
    frame_buffer.indirect_command_buffer =
        create_buffer(allocator,
                      sizeof(VkDrawIndexedIndirectCommand) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    device_address_info.buffer = frame_buffer.indirect_command_buffer.buffer;
    frame_buffer.indirect_command_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.culled_indirect_command_buffer =
        create_buffer(allocator,
                      sizeof(VkDrawIndexedIndirectCommand) * capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
    device_address_info.buffer = frame_buffer.culled_indirect_command_buffer.buffer;
    frame_buffer.culled_indirect_command_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.num_indirect_cmd_elems = 0;
    frame_buffer.num_indirect_cmd_elem_capacity = capacity;

    // Indirect draw cmd counts buffer.
    frame_buffer.indirect_counts_buffer =
        create_buffer(allocator,
                      sizeof(uint32_t) * count_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
    device_address_info.buffer = frame_buffer.indirect_counts_buffer.buffer;
    frame_buffer.indirect_counts_buffer_address =
        vkGetBufferDeviceAddress(device, &device_address_info);
    frame_buffer.num_indirect_counts_elems = 0;
    frame_buffer.num_indirect_counts_elem_capacity = count_capacity;
}

// bool vk_buffer::set_new_changed_indices(std::vector<uint32_t>&& changed_indices,
//                                         std::vector<GPU_geo_per_frame_buffer*>& all_per_frame_buffers)
// {
//     // @TODO: START HERE!!!!!! Think about what you want to do as far as the changed indices function.
//     //        It would honestly be nice to only update the indices that have changed within the buffer.
//     //        Idk... think about it!!!!

//     // Ignore empty changed indices lists.
//     if (changed_indices.empty())
//     {
//         std::cerr << "WARNING: Empty `changed_indices` list passed in. Ignoring." << std::endl; 
//         return true;
//     }

//     // Check that all previous changes have propagated.
//     for (auto& frame_buffer : all_per_frame_buffers)
//     {
//         if (!frame_buffer->changes_processed)
//         {
//             std::cerr << "WARNING: Previously submitted changes have not finished propagation." << std::endl;
//             return false;
//         }
//     }

//     // Replace changed indices.
//     s_changed_indices = std::move(changed_indices);
//     std::sort(s_changed_indices.begin(), s_changed_indices.end());
//     for (auto& frame_buffer : all_per_frame_buffers)
//     {
//         frame_buffer->changes_processed = false;
//     }

//     return true;
// }

void vk_buffer::flag_update_all_instances(std::vector<GPU_geo_per_frame_buffer*>& all_per_frame_buffers)
{
    // @TODO: @INCOMPLETE: This should be more efficientttttttt //
    // @NOTE: essentially just marks all frame buffers as needing to rebuild all instances.
    for (auto frame_buffer : all_per_frame_buffers)
    {
        frame_buffer->changes_processed = false;
    }
    //////////////////////////////////////////////////////////////
}

void vk_buffer::upload_changed_per_frame_data(const vk_util::Immediate_submit_support& support,
                                              VkDevice device,
                                              VkQueue queue,
                                              VmaAllocator allocator,
                                              GPU_geo_per_frame_buffer& frame_buffer)
{
    assert(false);  // @INCOMPLETE: (Line 524) Need to set `changes_processed = false;` when a tranform reader handle wants to update the transform information.  -Thea 2025/04/04

    // Exit if no changes.
    if (frame_buffer.changes_processed)
        return;

    // @TODO: Maybe there could be a more optimal way of doing this but as
    //        long as it's not gonna be too slow, we're just gonna rewrite
    //        the whole buffer for both instance and indirect buffers.
    
    // Get buffer device addresses.
    VkBufferDeviceAddressInfo device_address_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    };

    // Assign ids to all instances.
    auto unique_instances{ geo_instance::get_all_unique_instances() };
    for (size_t i = 0; i < unique_instances.size(); i++)
    {
        auto& inst{ unique_instances[i] };
        inst->cooked_buffer_instance_id = static_cast<uint32_t>(i);
    }

    // Update instance data buffer sizing.
    frame_buffer.num_instance_data_elems = unique_instances.size();
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
                      sizeof(gpu_geo_data::GPU_geo_instance_data) *
                        frame_buffer.num_instance_data_elem_capacity,
                      sizeof(gpu_geo_data::GPU_geo_instance_data) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
        device_address_info.buffer = frame_buffer.instance_data_buffer.buffer;
        frame_buffer.instance_data_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);
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
            // Update gpu transform data if there is a transform reader.
            if (inst->transform_reader_handle != nullptr)
            {
                inst->transform_reader_handle->read_current_transform(
                    inst->gpu_instance_data.transform, 0.5f);  // @HARDCODE
            }

            memcpy(data,
                   &inst->gpu_instance_data,
                   sizeof(gpu_geo_data::GPU_geo_instance_data));
            data++;
        }
        vmaUnmapMemory(allocator,
                       frame_buffer.instance_data_buffer.allocation);
    }

    // Update visible result buffer sizing.
    frame_buffer.num_visible_result_elems = unique_instances.size();
    if (unique_instances.size() > frame_buffer.num_visible_result_elem_capacity)
    {
        size_t new_capacity{
            unique_instances.size() +
                (unique_instances.size() % frame_buffer.expand_elems_interval) };
        expand_buffer(support,  // @TODO: @NOTE: Change this to the non-copying buffer expansion bc everything gets calculated on the GPU.
                      device,
                      queue,
                      allocator,
                      frame_buffer.visible_result_buffer,
                      sizeof(uint32_t) * frame_buffer.num_visible_result_elem_capacity,
                      sizeof(uint32_t) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
        device_address_info.buffer = frame_buffer.visible_result_buffer.buffer;
        frame_buffer.visible_result_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);
        frame_buffer.num_visible_result_elem_capacity = new_capacity;
    }
    // @NOTE: Don't upload visible result data bc it all gets calculated on GPU.

    // Collect primitive group indices and count buffer indices.
    using Primitive_group_index_t = uint32_t;
    using Count_buffer_index_t = uint32_t;
    std::vector<Primitive_group_index_t> primitive_group_base_indices;
    std::vector<Count_buffer_index_t> count_buffer_indices;

    auto all_primitives{ geo_instance::get_all_primitives() };
    auto all_base_primitive_groups{ geo_instance::get_all_base_primitive_indices() };

    primitive_group_base_indices.resize(all_primitives.size());
    count_buffer_indices.resize(all_primitives.size());
    size_t num_primitives_written{ 0 };

    Count_buffer_index_t current_cnt_buf_idx{ 0 };
    for (size_t i = 0; i < all_base_primitive_groups.size(); i++)
    {
        auto& primitive_group{ all_base_primitive_groups[i] };
        uint32_t from_idx{ primitive_group.second };
        uint32_t to_idx{ static_cast<uint32_t>(all_primitives.size()) };
        if (i < all_base_primitive_groups.size() - 1)
        {
            to_idx = all_base_primitive_groups[i + 1].second;
        }

        for (uint32_t j = from_idx; j < to_idx; j++)
        {
            primitive_group_base_indices[j] = primitive_group.second;
            count_buffer_indices[j]         = current_cnt_buf_idx;
            num_primitives_written++;
        }

        current_cnt_buf_idx++;
    }
    assert(num_primitives_written == all_primitives.size());
    assert(num_primitives_written == primitive_group_base_indices.size());
    assert(num_primitives_written == count_buffer_indices.size());

    // Update primitive group base indices buffer
    // and count buffer indices buffer sizing.
    assert(primitive_group_base_indices.size() == count_buffer_indices.size());

    frame_buffer.num_primitive_group_base_index_elems =
        primitive_group_base_indices.size();
    frame_buffer.num_count_buffer_index_elems = count_buffer_indices.size();

    if (primitive_group_base_indices.size() >
        frame_buffer.num_primitive_group_base_index_elem_capacity)
    {
        size_t new_capacity{
            primitive_group_base_indices.size() +
                (primitive_group_base_indices.size() %
                    frame_buffer.expand_elems_interval) };

        expand_buffer(support,
                      device,
                      queue,
                      allocator,
                      frame_buffer.primitive_group_base_index_buffer,
                      sizeof(uint32_t) * frame_buffer.num_primitive_group_base_index_elem_capacity,
                      sizeof(uint32_t) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
        device_address_info.buffer = frame_buffer.primitive_group_base_index_buffer.buffer;
        frame_buffer.primitive_group_base_index_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);
        frame_buffer.num_primitive_group_base_index_elem_capacity = new_capacity;

        expand_buffer(support,
                      device,
                      queue,
                      allocator,
                      frame_buffer.count_buffer_index_buffer,
                      sizeof(uint32_t) * frame_buffer.num_count_buffer_index_elem_capacity,
                      sizeof(uint32_t) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
        device_address_info.buffer = frame_buffer.count_buffer_index_buffer.buffer;
        frame_buffer.count_buffer_index_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);
        frame_buffer.num_count_buffer_index_elem_capacity = new_capacity;
    }

    // Upload primitive group base indices.
    {
        void* data;
        vmaMapMemory(allocator,
                     frame_buffer.primitive_group_base_index_buffer.allocation,
                     &data);
        memcpy(data,
               primitive_group_base_indices.data(),
               sizeof(Primitive_group_index_t) * primitive_group_base_indices.size());
        vmaUnmapMemory(allocator,
                       frame_buffer.primitive_group_base_index_buffer.allocation);
    }

    // Upload count buffer indices.
    {
        void* data;
        vmaMapMemory(allocator,
                     frame_buffer.count_buffer_index_buffer.allocation,
                     &data);
        memcpy(data,
               count_buffer_indices.data(),
               sizeof(Count_buffer_index_t) * count_buffer_indices.size());
        vmaUnmapMemory(allocator,
                       frame_buffer.count_buffer_index_buffer.allocation);
    }

    // Update indirect cmd buffer sizing.
    frame_buffer.num_indirect_cmd_elems = all_primitives.size();
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
                      sizeof(VkDrawIndexedIndirectCommand) *
                        frame_buffer.num_indirect_cmd_elem_capacity,
                      sizeof(VkDrawIndexedIndirectCommand) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
        device_address_info.buffer = frame_buffer.indirect_command_buffer.buffer;
        frame_buffer.indirect_command_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);

        // Simply destroy and recreate buffer since no copying needed.
        destroy_buffer(allocator, frame_buffer.culled_indirect_command_buffer);
        frame_buffer.culled_indirect_command_buffer =
            create_buffer(allocator,
                          sizeof(VkDrawIndexedIndirectCommand) * new_capacity,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                              VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);
        device_address_info.buffer =
            frame_buffer.culled_indirect_command_buffer.buffer;
        frame_buffer.culled_indirect_command_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);

        frame_buffer.num_indirect_cmd_elem_capacity = new_capacity;
    }

    // Upload indirect cmds data.
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

    // Count number of primitive groups to create count buffer.
    uint32_t num_primitive_groups{
        static_cast<uint32_t>(all_base_primitive_groups.size()) };

    // Update indirect counts sizing.
    frame_buffer.num_indirect_counts_elems = num_primitive_groups;
    if (num_primitive_groups > frame_buffer.num_indirect_counts_elem_capacity)
    {
        size_t new_capacity{
            num_primitive_groups +
                (num_primitive_groups % frame_buffer.expand_elems_interval) };
        expand_buffer(support,
                      device,
                      queue,
                      allocator,
                      frame_buffer.indirect_counts_buffer,
                      sizeof(uint32_t) * frame_buffer.num_indirect_counts_elem_capacity,
                      sizeof(uint32_t) * new_capacity,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);
        device_address_info.buffer = frame_buffer.indirect_counts_buffer.buffer;
        frame_buffer.indirect_counts_buffer_address =
            vkGetBufferDeviceAddress(device, &device_address_info);
        frame_buffer.num_indirect_counts_elem_capacity = new_capacity;
    }
    // @NOTE: Don't populate buffer bc it gets written to.

    frame_buffer.changes_processed = true;
}
