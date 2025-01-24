#pragma once

// Windows implementation of renderer.
#if _WIN64

#include "renderer.h"

// @NOTE: Vulkan, VMA, and GLFW have to be included in this order.
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <cinttypes>
#include <cstring>
#include <iostream>
#include "VkBootstrap.h"


struct FrameData
{
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;
};

constexpr uint32_t k_frame_overlap{ 2 };

static void key_callback(GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods)
{

}

class Monolithic_renderer::Impl
{
public:
    Impl(const std::string& name, int32_t content_width, int32_t content_height, Job_source& source)
        : m_name(name)
        , m_window_width(content_width)
        , m_window_height(content_height)
        , m_build_job(std::make_unique<Build_job>(source, *this))
        , m_teardown_job(std::make_unique<Teardown_job>(source, *this))
    {
    }

    enum class Stage : uint32_t
    {
        BUILD = 0,
        UPDATE_DATA,
        RENDER,
        TEARDOWN,
        EXIT,
    };
    std::atomic<Stage> m_stage{ Stage::BUILD };
    std::atomic_bool m_shutdown_flag{ false };
    std::atomic_bool m_finished_shutdown{ false };

    void tick()
    {
        update_window();
        render();
    }

    bool is_requesting_close()
    {
        return glfwWindowShouldClose(m_window);
    }

    void request_shutdown()
    {
        m_shutdown_flag = true;
    }

    bool is_finished_shutdown()
    {
        return m_finished_shutdown;
    }

    // Jobs.
    class Build_job : public Job_ifc
    {
    public:
        Build_job(Job_source& source, Monolithic_renderer::Impl& pimpl)
            : Job_ifc("Renderer Build job", source)
            , m_pimpl(pimpl)
        {
        }

        int32_t execute() override;

        Monolithic_renderer::Impl& m_pimpl;
    };
    std::unique_ptr<Build_job> m_build_job;

    class Update_data_job : public Job_ifc
    {
    public:
        Update_data_job(Job_source& source, Monolithic_renderer::Impl& pimpl)
            : Job_ifc("Renderer Update Data job", source)
            , m_pimpl(pimpl)
        {
        }

        int32_t execute() override;

        Monolithic_renderer::Impl& m_pimpl;
    };
    std::unique_ptr<Update_data_job> m_update_data_job;

    class Render_job : public Job_ifc
    {
    public:
        Render_job(Job_source& source, Monolithic_renderer::Impl& pimpl)
            : Job_ifc("Renderer Render job", source)
            , m_pimpl(pimpl)
        {
        }

        int32_t execute() override;

        Monolithic_renderer::Impl& m_pimpl;
    };
    std::unique_ptr<Render_job> m_render_job;

    class Teardown_job : public Job_ifc
    {
    public:
        Teardown_job(Job_source& source, Monolithic_renderer::Impl& pimpl)
            : Job_ifc("Renderer Teardown job", source)
            , m_pimpl(pimpl)
        {
        }

        int32_t execute() override;

        Monolithic_renderer::Impl& m_pimpl;
    };
    std::unique_ptr<Teardown_job> m_teardown_job;

    // Fetch next jobs.
    std::vector<Job_ifc*> fetch_next_jobs_callback();

private:
    // Win64 window setup/teardown.
    bool build_window();
    bool teardown_window();

    // Vulkan renderer setup/teardown.
    bool build_vulkan_renderer();
    bool teardown_vulkan_renderer();

    // Tick procedures.
    void update_window();
    void render();

    std::string m_name;
    int32_t m_window_width;
    int32_t m_window_height;

    GLFWwindow* m_window{ nullptr };
    VkInstance m_v_instance{ nullptr };
#if _DEBUG
    VkDebugUtilsMessengerEXT m_v_debug_utils_messenger{ nullptr };
#endif
    VkSurfaceKHR m_v_surface{ nullptr };
    VkPhysicalDevice m_v_physical_device{ nullptr };
    VkPhysicalDeviceProperties m_v_physical_device_properties;
    VkDevice m_v_device{ nullptr };
    VkQueue m_v_graphics_queue{ nullptr };
    uint32_t m_v_graphics_queue_family_idx;
    VmaAllocator m_v_vma_allocator{ nullptr };
    VkSwapchainKHR m_v_swapchain{ nullptr };
    std::vector<VkImage> m_v_swapchain_images;
    std::vector<VkImageView> m_v_swapchain_image_views;
    VkFormat m_v_swapchain_image_format;

    FrameData m_frames[k_frame_overlap];
    std::atomic_size_t m_frame_number{ 0 };
    
    inline FrameData& get_current_frame()
    {
        return m_frames[m_frame_number % k_frame_overlap];
    }

};

#endif  // _WIN64
