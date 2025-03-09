#pragma once

#if _WIN64
#include <vulkan/vulkan.h>
struct GLFWwindow;
#else
#error "Unsupported OS"
#endif  // _WIN64


namespace imgui_system
{

// Setup/teardown.
#if _WIN64
bool build_imgui(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue graphics_queue,
    uint32_t graphics_queue_family_idx,
    VkFormat swapchain_image_format);
#endif  // _WIN64
bool teardown_imgui();

// Memory barriers.
void mutex_lock();
void mutex_unlock();

// Rendering.
void set_imgui_enabled(bool flag);
void set_imgui_visible(bool flag);
bool render_imgui();

// Actual drawing.
#if _WIN64
bool render_imgui_onto_swapchain(
    VkCommandBuffer cmd,
    VkExtent2D render_extent,
    VkImageView target_image_view);
#endif  // _WIN64

}  // namespace imgui_system
