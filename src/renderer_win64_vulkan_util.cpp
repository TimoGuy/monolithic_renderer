#include "renderer_win64_vulkan_util.h"


void vk_util::transition_image(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout current_layout,
                               VkImageLayout new_layout)
{
    VkImageAspectFlags aspect_mask{
        static_cast<VkImageAspectFlags>(
            (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ?
                VK_IMAGE_ASPECT_DEPTH_BIT :
                VK_IMAGE_ASPECT_COLOR_BIT
        )
    };

    VkImageSubresourceRange subresource_range{
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    VkImageMemoryBarrier2 image_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,  // @TODO: inefficient (https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples)
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,  // @TODO: inefficient
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = current_layout,
        .newLayout = new_layout,
        .image = image,
        .subresourceRange = subresource_range,
    };

    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

VkSemaphoreSubmitInfo vk_util::semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                                     VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stage_mask,
        .deviceIndex = 0,
    };
	return info;
}

VkCommandBufferSubmitInfo vk_util::command_buffer_submit_info(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0,
    };
	return info;
}

VkSubmitInfo2 vk_util::submit_info(VkCommandBufferSubmitInfo* cmd,
                                   VkSemaphoreSubmitInfo* signal_semaphore_info,
                                   VkSemaphoreSubmitInfo* wait_semaphore_info)
{
    VkSubmitInfo2 info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .waitSemaphoreInfoCount = (wait_semaphore_info == nullptr) ? 0u : 1u,
        .pWaitSemaphoreInfos = wait_semaphore_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = cmd,
        .signalSemaphoreInfoCount = (signal_semaphore_info == nullptr) ? 0u : 1u,
        .pSignalSemaphoreInfos = signal_semaphore_info,
    };
    return info;
}
