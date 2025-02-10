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
        VkBufferCopy vertices_copy{
            .srcOffset = index_buffer_size,
            .dstOffset = 0,
            .size = vertex_buffer_size,
        };
        VkBufferCopy copies[]{ indices_copy, vertices_copy };
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, new_mesh.index_buffer.buffer, 2, copies);
    });

    destroy_buffer(allocator, staging_buffer);

    return new_mesh;
}
