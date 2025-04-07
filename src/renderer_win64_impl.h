#pragma once

// Windows implementation of renderer.
#if _WIN64

#include "renderer.h"

// @NOTE: Vulkan, VMA, GLFW, VkBootstrap have to be included in this order.
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include "VkBootstrap.h"

#include <array>
#include <cinttypes>
#include <cstring>
#include <iostream>
#include "renderer_win64_vk_buffer.h"
#include "renderer_win64_vk_descriptor_layout_builder.h"
#include "renderer_win64_vk_image.h"
#include "renderer_win64_vk_immediate_submit.h"

namespace vk_util { struct Immediate_submit_support; }


struct Frame_data
{
    VkCommandPool command_pool;
    VkCommandBuffer main_command_buffer;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;

    vk_buffer::Allocated_buffer camera_buffer;
    vk_buffer::GPU_geo_per_frame_buffer geo_per_frame_buffer;
};

struct Descriptor_set_w_layout
{
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout descriptor_layout;
};

constexpr uint32_t k_frame_overlap{ 2 };

extern std::atomic<Monolithic_renderer*> s_mr_singleton_ptr;

class Monolithic_renderer::Impl
{
public:
    Impl(const std::string& name,
         int32_t content_width,
         int32_t content_height,
         int32_t fallback_content_width,
         int32_t fallback_content_height,
         Job_source& source);

    enum class Stage : uint32_t
    {
        BUILD_WINDOW = 0,
        BUILD,
        LOAD_ASSETS,
        UPDATE_DATA,
        RENDER,
        TEARDOWN,
        END_OF_LIFE,
    };
    std::atomic<Stage> m_stage{ Stage::BUILD_WINDOW };
    std::atomic_bool m_shutdown_flag{ false };
    std::atomic_bool m_finished_shutdown{ false };

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

    // Render geometry object lifetime.
    render_geo_obj_key_t create_render_geo_obj(const std::string& model_name,
                                               const std::string& material_set_name,
                                               geo_instance::Geo_render_pass render_pass,
                                               bool is_shadow_caster,
                                               phys_obj::Transform_holder* transform_holder);
    void destroy_render_geo_obj(render_geo_obj_key_t key);
    void set_render_geo_obj_transform(render_geo_obj_key_t key,
                                      mat4 transform);

    // Jobs.
    inline static const uint32_t k_glfw_window_job_key{ 0xB00B1E55 };

    class Build_window_job : public Job_ifc
    {
    public:
        Build_window_job(Job_source& source, Monolithic_renderer::Impl& pimpl)
            : Job_ifc("Window Build job", source, k_glfw_window_job_key)
            , m_pimpl(pimpl)
        {
        }

        int32_t execute() override;

        Monolithic_renderer::Impl& m_pimpl;
    };
    std::unique_ptr<Build_window_job> m_build_window_job;

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

    class Load_assets_job : public Job_ifc
    {
    public:
        Load_assets_job(Job_source& source, Monolithic_renderer::Impl& pimpl)
            : Job_ifc("Load assets job", source)
            , m_pimpl(pimpl)
        {
        }

        int32_t execute() override;

        Monolithic_renderer::Impl& m_pimpl;
    };
    std::unique_ptr<Load_assets_job> m_load_assets_job;

    class Update_poll_window_events_job : public Job_ifc
    {
    public:
        Update_poll_window_events_job(Job_source& source)
            : Job_ifc("Poll Window Events job", source, k_glfw_window_job_key)
        {
        }

        int32_t execute() override;
    };
    std::unique_ptr<Update_poll_window_events_job> m_update_poll_window_events_job;

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

    // Geometry graphics render pass.
    struct Geometry_graphics_pass
    {
        struct Per_frame_data
        {
            Descriptor_set_w_layout camera_data;
            Descriptor_set_w_layout shadow_camera_data;  // @TODO: IMPLEMENT THIS!!!!!!
        };
        std::array<Per_frame_data, k_frame_overlap> per_frame_datas;
        Descriptor_set_w_layout material_param_sets_data;
        Descriptor_set_w_layout bounding_spheres_data;

        std::vector<VkDescriptorSet> material_param_definition_descriptor_sets;  // @CHECK: Maybe this should be held by the material bank???
        VkDescriptorSetLayout material_param_definition_descriptor_layout;

        VkPipeline culling_pipeline;
        VkPipelineLayout culling_pipeline_layout;

        VkPipeline write_draw_cmds_pipeline;
        VkPipelineLayout write_draw_cmds_pipeline_layout;
    };

private:
    // Win64 window setup/teardown.
    bool build_window();
    bool teardown_window();

    // Vulkan renderer setup/teardown.
    bool build_vulkan_renderer();
    bool teardown_vulkan_renderer();
    bool wait_for_renderer_idle();

    // Setup jobs.
    bool setup_initial_camera_props();

    // Misc?????
    bool write_material_param_sets_to_descriptor_sets();
    bool write_bounding_spheres_to_descriptor_sets();

    // Tick procedures.
    bool update_and_upload_render_data();
    bool render();

    std::string m_name;
    int32_t m_window_width;
    int32_t m_window_height;
    int32_t m_fallback_window_width;
    int32_t m_fallback_window_height;
    GLFWwindow* m_window{ nullptr };

    vk_util::Immediate_submit_support m_immediate_submit_support;

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
        VkExtent2D extent;
    } m_v_swapchain;

    struct HDR_draw_image
    {
        vk_image::Allocated_image image;
        VkExtent2D                extent;
    } m_v_HDR_draw_image;

    vk_desc::Descriptor_allocator m_v_descriptor_alloc;

    vk_buffer::GPU_geo_resource_buffer m_v_geo_passes_resource_buffer;
    Geometry_graphics_pass m_v_geometry_graphics_pass;

    inline Geometry_graphics_pass::Per_frame_data& get_current_geom_per_frame_data()
    {
        return m_v_geometry_graphics_pass
            .per_frame_datas[m_frame_number % k_frame_overlap];
    }

    struct Sample_pass
    {
        VkDescriptorSet descriptor_set;
        VkDescriptorSetLayout descriptor_layout;
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
    } m_v_sample_pass;

    struct Sample_graphics_pass
    {
        VkPipeline pipeline;
        VkPipelineLayout pipeline_layout;
    } m_v_sample_graphics_pass;

    Frame_data m_frames[k_frame_overlap];
    std::atomic_size_t m_frame_number{ 0 };
    
    inline Frame_data& get_current_frame()
    {
        return m_frames[m_frame_number % k_frame_overlap];
    }
};

#endif  // _WIN64
