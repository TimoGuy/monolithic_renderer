#include "renderer_win64_vk_util.h"


// Submission.
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

// Image.
VkImageCreateInfo vk_util::image_create_info(VkFormat format,
                                             VkImageUsageFlags usage_flags,
                                             VkExtent3D extent)
{
    VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,  // @NOTE: for MSAA.
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage_flags,
    };
    return info;
}

VkImageViewCreateInfo vk_util::image_view_create_info(VkFormat format,
                                                      VkImage image,
                                                      VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange{
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    return info;
}

// Image manipulation.
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

void vk_util::blit_image_to_image(VkCommandBuffer cmd,
                                  VkImage source,
                                  VkImage destination,
                                  VkExtent2D src_size,
                                  VkExtent2D dst_size,
                                  VkFilter filter /*= VK_FILTER_LINEAR*/)
{
    VkImageBlit2 blit_region{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets{
            { .x = 0, .y = 0, .z = 0, },
            { .x = static_cast<int32_t>(src_size.width),
              .y = static_cast<int32_t>(src_size.height),
              .z = 1, },
        },
        .dstSubresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets{
            { .x = 0, .y = 0, .z = 0, },
            { .x = static_cast<int32_t>(dst_size.width),
              .y = static_cast<int32_t>(dst_size.height),
              .z = 1, },
        },
    };

    VkBlitImageInfo2 blit_info{
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blit_region,
        .filter = filter,
    };

    vkCmdBlitImage2(cmd, &blit_info);
}

// Rendering.
VkPipelineShaderStageCreateInfo vk_util::pipeline_shader_stage_info(VkShaderStageFlagBits stage,
                                                                    VkShaderModule shader)
{
    VkPipelineShaderStageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .stage = stage,
        .module = shader,
        .pName = "main",
    };
    return info;
}

VkRenderingAttachmentInfo vk_util::attachment_info(
    VkImageView image_view,
    VkClearValue* clear_value,
    VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
    VkRenderingAttachmentInfo attachment_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = image_view,
        .imageLayout = layout,
        .loadOp = (clear_value == nullptr ?
                       VK_ATTACHMENT_LOAD_OP_LOAD :
                       VK_ATTACHMENT_LOAD_OP_CLEAR),
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    if (clear_value != nullptr)
    {
        attachment_info.clearValue = *clear_value;
    }
    return attachment_info;
}

VkRenderingInfo vk_util::rendering_info(VkExtent2D render_extent,
                                        VkRenderingAttachmentInfo* color_attachment,
                                        VkRenderingAttachmentInfo* depth_attachment)
{
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, render_extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = color_attachment,
        .pDepthAttachment = depth_attachment,
        .pStencilAttachment = nullptr,
    };
    return rendering_info;
}