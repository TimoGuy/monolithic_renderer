#pragma once

#if _WIN64

#include <vulkan/vulkan.h>

namespace vk_util
{

// Submission.
VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                            VkSemaphore semaphore);

VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd,
                          VkSemaphoreSubmitInfo* signal_semaphore_info,
                          VkSemaphoreSubmitInfo* wait_semaphore_info);

// Image.
VkImageCreateInfo image_create_info(VkFormat format,
                                    VkImageUsageFlags usage_flags,
                                    VkExtent3D extent);

VkImageViewCreateInfo image_view_create_info(VkFormat format,
                                             VkImage image,
                                             VkImageAspectFlags aspect_flags);

// Image manipulation.
void transition_image(VkCommandBuffer cmd,
                      VkImage image,
                      VkImageLayout current_layout,
                      VkImageLayout new_layout);

void blit_image_to_image(VkCommandBuffer cmd,
                         VkImage source,
                         VkImage destination,
                         VkExtent2D src_size,
                         VkExtent2D dst_size,
                         VkFilter filter = VK_FILTER_LINEAR);

}  // namespace vk_util


#endif  // _WIN64
