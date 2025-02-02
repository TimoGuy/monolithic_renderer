#include "renderer_win64_vk_immediate_submit.h"

#include <cassert>
#include <iostream>
#include "renderer_win64_vk_util.h"


void vk_util::init_immediate_submit_support(Immediate_submit_support& out_support,
                                            VkDevice device,
                                            uint32_t graphics_queue_family_idx)
{
    VkCommandPoolCreateInfo cmd_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_queue_family_idx
    };
    VkResult err{
        vkCreateCommandPool(device, &cmd_pool_info, nullptr, &out_support.command_pool) };
    if (err)
    {
        std::cerr << "ERROR: create immediate submit command pool failed." << std::endl;
        assert(false);
    }

    VkCommandBufferAllocateInfo cmd_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = out_support.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    err = vkAllocateCommandBuffers(device, &cmd_alloc_info, &out_support.command_buffer);
    if (err)
    {
        std::cerr << "ERROR: allocate immediate submit command pool failed." << std::endl;
        assert(false);
    }

    VkFenceCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,  // For waiting on it for the first frame.
    };
    err = vkCreateFence(device, &fence_create_info, nullptr, &out_support.fence);
    if (err)
    {
        std::cerr << "ERROR: create immediate submit fence failed." << std::endl;
        assert(false);
    }
}

void vk_util::destroy_immediate_submit_support(const Immediate_submit_support& support,
                                               VkDevice device)
{
    vkDestroyFence(device, support.fence, nullptr);
    vkDestroyCommandPool(device, support.command_pool, nullptr);
}

void vk_util::immediate_submit(const Immediate_submit_support& support,
                               VkDevice device,
                               VkQueue queue,
                               std::function<void(VkCommandBuffer cmd)>&& func)
{
    VkResult err{
        vkResetFences(device, 1, &support.fence) };
    if (err)
    {
        std::cerr << "ERROR: Reset immediate submit fence failed." << std::endl;
        assert(false);
    }
    err = vkResetCommandBuffer(support.command_buffer, 0);
    if (err)
    {
        std::cerr << "ERROR: reset immediate submit command buffer failed." << std::endl;
        assert(false);
    }

    // Write command buffer.
    VkCommandBuffer cmd{ support.command_buffer };
    VkCommandBufferBeginInfo cmd_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    err = vkBeginCommandBuffer(cmd, &cmd_begin_info);
    if (err)
    {
        std::cerr << "ERROR: Begin immediate submit command buffer failed." << std::endl;
        assert(false);
    }

    func(cmd);

    err = vkEndCommandBuffer(cmd);
    if (err)
    {
        std::cerr << "ERROR: End immediate submit command buffer failed." << std::endl;
        assert(false);
    }

    // Submit and execute.
    VkCommandBufferSubmitInfo cmd_buffer_submit_info{
        vk_util::command_buffer_submit_info(cmd) };
    VkSubmitInfo2 submit_info{
        vk_util::submit_info(&cmd_buffer_submit_info, nullptr, nullptr) };

    err = vkQueueSubmit2(queue, 1, &submit_info, support.fence);
    if (err)
    {
        std::cerr << "ERROR: Submit immediate submit failed." << std::endl;
        assert(false);
    }
    err = vkWaitForFences(device, 1, &support.fence, true, 9999999999);
    if (err)
    {
        std::cerr << "ERROR: Wait for immediate submit fence failed." << std::endl;
        assert(false);
    }
}
