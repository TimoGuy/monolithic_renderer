#pragma once

// Windows implementation of renderer.
#if _WIN64

#include "renderer.h"

// @NOTE: Vulkan, VMA, GLFW, VkBootstrap have to be included in this order.
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include "VkBootstrap.h"

#include <cinttypes>
#include <cstring>
#include <iostream>
#include "renderer_win64_vk_image.h"


struct FrameData
{
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;
};

constexpr uint32_t k_frame_overlap{ 2 };

extern std::atomic<Monolithic_renderer*> s_mr_singleton_ptr;

static void key_callback(GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods)
{

}

static void window_focus_callback(GLFWwindow* window, int32_t focused)
{

}

static void window_iconify_callback(GLFWwindow* window, int32_t iconified)
{
    // Check if window was placed back and should resume rendering.
    if (iconified == GLFW_FALSE)
    {
        std::cout << "NOTE: window unminimized. Resuming renderer." << std::endl;
        s_mr_singleton_ptr.load()->notify_windowevent_uniconification();
    }
}

class Monolithic_renderer::Impl
{
public:
    Impl(const std::string& name,
         int32_t content_width,
         int32_t content_height,
         int32_t fallback_content_width,
         int32_t fallback_content_height,
         Job_source& source)
        : m_name(name)
        , m_window_width(content_width)
        , m_window_height(content_height)
        , m_fallback_window_width(fallback_content_width)
        , m_fallback_window_height(fallback_content_height)
        , m_build_job(std::make_unique<Build_job>(source, *this))
        , m_update_data_job(std::make_unique<Update_data_job>(source, *this))
        , m_render_job(std::make_unique<Render_job>(source, *this))
        , m_teardown_job(std::make_unique<Teardown_job>(source, *this))
    {
    }

    enum class Stage : uint32_t
    {
        BUILD = 0,
        UPDATE_DATA,
        RENDER,
        TEARDOWN,
        END_OF_LIFE,
    };
    std::atomic<Stage> m_stage{ Stage::BUILD };
    std::atomic_bool m_shutdown_flag{ false };
    std::atomic_bool m_finished_shutdown{ false };

    void tick()
    {
        update_window();
        render();
    }

    void notify_uniconification()
    {
        m_request_swapchain_creation = true;  // @TODO: USE THIS FLAG AND RECREATE THAT SWAPCHAINNNNNNNNNNNNNNNN
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
    Job_next_jobs_return_data fetch_next_jobs_callback();

private:
    // Win64 window setup/teardown.
    bool build_window();
    bool teardown_window();

    // Vulkan renderer setup/teardown.
    bool build_vulkan_renderer();
    bool teardown_vulkan_renderer();

    // Tick procedures.
    bool update_window();
    bool render();

    std::string m_name;
    int32_t m_window_width;
    int32_t m_window_height;
    int32_t m_fallback_window_width;
    int32_t m_fallback_window_height;
    GLFWwindow* m_window{ nullptr };

    std::atomic_bool m_is_swapchain_out_of_date{ false };
    std::atomic_bool m_request_swapchain_creation{ false };

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

    struct Swapchain
    {
        VkSwapchainKHR swapchain{ nullptr };
        std::vector<VkImage> images;
        std::vector<VkImageView> image_views;
        VkFormat image_format;
    } m_v_swapchain;

    struct HDR_draw_image
    {
        vk_image::AllocatedImage image;
        VkExtent2D               extent;
    } m_v_HDR_draw_image;

    FrameData m_frames[k_frame_overlap];
    std::atomic_size_t m_frame_number{ 0 };
    
    inline FrameData& get_current_frame()
    {
        return m_frames[m_frame_number % k_frame_overlap];
    }
};

#endif  // _WIN64
