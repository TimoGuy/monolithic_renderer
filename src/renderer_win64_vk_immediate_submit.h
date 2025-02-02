#pragma once

#if _WIN64

#include <functional>
#include <vulkan/vulkan.h>


namespace vk_util
{

struct Immediate_submit_support
{
    VkFence fence;
    VkCommandBuffer command_buffer;
    VkCommandPool command_pool;
};

void init_immediate_submit_support(Immediate_submit_support& out_support,
                                   VkDevice device,
                                   uint32_t graphics_queue_family_idx);
void destroy_immediate_submit_support(const Immediate_submit_support& support,
                                      VkDevice device);

void immediate_submit(const Immediate_submit_support& support,
                      VkDevice device,
                      VkQueue queue,
                      std::function<void(VkCommandBuffer cmd)>&& func);

}  // namespace vk_util

#endif  // _WIN64