#include "renderer_win64_vk_buffer.h"

#include <cassert>
#include <iostream>
#include <vk_mem_alloc.h>
#include "renderer_win64_vk_immediate_submit.h"


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
