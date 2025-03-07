#if _WIN64

#include "renderer_win64_impl.h"

// @NOTE: Vulkan, VMA, and GLFW have to be included in this order.
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include "VkBootstrap.h"

#include <cinttypes>
#include <cstring>
#include <iostream>
#include "camera.h"
#include "geo_instance.h"
#include "gltf_loader.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "material_bank.h"
#include "multithreaded_job_system_public.h"
#include "renderer_win64_vk_pipeline_builder.h"
#include "renderer_win64_vk_util.h"
#include "timing_reporter_public.h"


// For extern symbol.
std::atomic<Monolithic_renderer*> s_mr_singleton_ptr{ nullptr };

Monolithic_renderer::Impl::Impl(const std::string& name,
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
    , m_load_assets_job(std::make_unique<Load_assets_job>(source, *this))
    , m_update_data_job(std::make_unique<Update_data_job>(source, *this))
    , m_render_job(std::make_unique<Render_job>(source, *this))
    , m_teardown_job(std::make_unique<Teardown_job>(source, *this))
{
}

// Jobs.
int32_t Monolithic_renderer::Impl::Build_job::execute()
{
    bool success{ true };
    success &= m_pimpl.build_window();
    success &= m_pimpl.build_vulkan_renderer();
    success &= m_pimpl.build_imgui();
    success &= m_pimpl.setup_initial_camera_props();
    return success ? 0 : 1;
}

int32_t Monolithic_renderer::Impl::Load_assets_job::execute()
{
    // @TODO: @THEA: add these material and model constructions into the actual soranin game as a constructor param.
    // @TODO: change this into reading a json file for material info.

    // Add material features.
    // material_bank::emplace_descriptor_set_layout_feature("camera", asdfasdfasdf);
    // material_bank::emplace_buffer_reference_feature("instance_data");
    // material_bank::emplace_descriptor_set_layout_feature("material_sets", asdfasdfasdf);


    // Pipelines.
    TIMING_REPORT_START(reg_pipes);
    VkFormat draw_format{ m_pimpl.m_v_HDR_draw_image.image.image_format };

    material_bank::set_descriptor_references(
        m_pimpl.m_v_geometry_graphics_pass.per_frame_datas.front().camera_data.descriptor_layout,
        m_pimpl.m_v_geometry_graphics_pass.per_frame_datas.front().shadow_camera_data.descriptor_layout,
        m_pimpl.m_v_geometry_graphics_pass.material_param_sets_data.descriptor_layout,
        m_pimpl.m_v_geometry_graphics_pass.material_param_definition_descriptor_layout,
        m_pimpl.m_v_geometry_graphics_pass.material_param_sets_data.descriptor_set);

    material_bank::register_pipeline("missing");
    material_bank::register_pipeline("opaque_z_prepass");
    material_bank::register_pipeline("opaque_shadow");  // @TODO: IMPLEMENT SHADOWS
    
    auto& v_device{ m_pimpl.m_v_device };
    material_bank::define_pipeline("missing",
                                   "opaque_z_prepass",
                                   "", // "opaque_shadow",  // @TODO: ADD SHADOWS.
                                   material_bank::create_geometry_material_pipeline(
                                       v_device,
                                       draw_format,
                                       true,
                                       material_bank::Camera_type::MAIN_VIEW,
                                       true,
                                       {
                                           { "color", material_bank::Mat_param_def_type::VEC4 },
                                       },
                                       "assets/shaders/geommat_missing.vert.spv",
                                       "assets/shaders/geommat_missing.frag.spv"));
    material_bank::define_pipeline("opaque_z_prepass",
                                   "",
                                   "",
                                   material_bank::create_geometry_material_pipeline(
                                       v_device,
                                       draw_format,
                                       false,
                                       material_bank::Camera_type::MAIN_VIEW,
                                       false,
                                       {},
                                       "assets/shaders/geommat_opaque_z_prepass.vert.spv",
                                       "assets/shaders/geommat_opaque_z_prepass.frag.spv"));
    // @TODO: IMPLEMENT SHADOWS
    // material_bank::define_pipeline("opaque_shadow",
    //                                "",
    //                                "",
    //                                material_bank::create_geometry_material_pipeline(
    //                                    v_device,
    //                                    draw_format,
    //                                    false,
    //                                    material_bank::Camera_type::SHADOW_VIEW,
    //                                    false,
    //                                    {},
    //                                    "assets/shaders/geommat_opaque_shadow.vert.spv",
    //                                    "assets/shaders/geommat_opaque_shadow.frag.spv"));
    TIMING_REPORT_END_AND_PRINT(reg_pipes, "Register Material Pipelines: ");

    // Materials.
    TIMING_REPORT_START(reg_mats);
    using Mat_data = material_bank::Material_parameter_data;
    material_bank::register_material("Red", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.2f, 0.2f, 0.2f, 1.0f } }, },
        },
    });
    material_bank::register_material("Body", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 1.0f, 0.0f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("Tights", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 1.0f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("gold", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.0f, 1.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("slime_body", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_cel_shaded"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 1.0f, 0.0f, 1.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("clothing_tights", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 1.0f, 1.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("slimegirl_eyebrows", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 1.0f, 1.0f, 1.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("slimegirl_eyes", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.0f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("slime_hair", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_cel_shaded"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.5f, 0.0f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("suede_white", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.5f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("suede_gray", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.0f, 0.5f, 1.0f } }, },
        },
    });
    material_bank::register_material("rubber_black", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.5f, 0.0f, 0.5f, 1.0f } }, },
        },
    });
    material_bank::register_material("plastic_green", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.5f, 0.5f, 1.0f } }, },
        },
    });
    material_bank::register_material("denim", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.5f, 0.5f, 0.5f, 1.0f } }, },
        },
    });
    material_bank::register_material("leather", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.2f, 0.0f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("corduroy_white", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.2f, 0.0f, 1.0f } }, },
        },
    });
    material_bank::register_material("ribbed_tan", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.0f, 0.0f, 0.2f, 1.0f } }, },
        },
    });
    material_bank::register_material("knitting_green", {
        //.pipeline_idx = material_bank::get_pipeline_idx_from_name("pbr_default"),
        .pipeline_idx = material_bank::get_pipeline_idx_from_name("missing"),
        .material_param_datas{
            Mat_data{ .param_name = "color", .data{ ._vec4{ 0.2f, 0.0f, 0.2f, 1.0f } }, },
        },
    });
    material_bank::cook_all_material_param_indices_and_pipeline_conns();
    TIMING_REPORT_END_AND_PRINT(reg_mats, "Register and Cook Materials: ");

    // Material sets.
    TIMING_REPORT_START(reg_mat_sets);
    material_bank::register_material_set(
        "slime_girl_mat_set_0",
        material_bank::GPU_material_set{
            {
                material_bank::get_mat_idx_from_name("gold"),
                material_bank::get_mat_idx_from_name("slime_body"),
                material_bank::get_mat_idx_from_name("clothing_tights"),
                material_bank::get_mat_idx_from_name("slimegirl_eyebrows"),
                material_bank::get_mat_idx_from_name("slimegirl_eyes"),
                material_bank::get_mat_idx_from_name("slime_hair"),
                material_bank::get_mat_idx_from_name("suede_white"),
                material_bank::get_mat_idx_from_name("suede_gray"),
                material_bank::get_mat_idx_from_name("rubber_black"),
                material_bank::get_mat_idx_from_name("plastic_green"),
                material_bank::get_mat_idx_from_name("denim"),
                material_bank::get_mat_idx_from_name("leather"),
                material_bank::get_mat_idx_from_name("corduroy_white"),
                material_bank::get_mat_idx_from_name("plastic_green"),
                material_bank::get_mat_idx_from_name("leather"),
                material_bank::get_mat_idx_from_name("corduroy_white"),
                material_bank::get_mat_idx_from_name("ribbed_tan"),
                material_bank::get_mat_idx_from_name("leather"),
                material_bank::get_mat_idx_from_name("gold"),
                material_bank::get_mat_idx_from_name("knitting_green"),
            }
        }
    );
    material_bank::register_material_set(
        "enemy_wip_mat_set_0",
        material_bank::GPU_material_set{
            {
                material_bank::get_mat_idx_from_name("Body"),
                material_bank::get_mat_idx_from_name("Tights"),
            }
        }
    );
    material_bank::register_material_set(
        "box_mat_set_0",
        material_bank::GPU_material_set{
            {
                material_bank::get_mat_idx_from_name("rubber_black"),
            }
        }
    );
    TIMING_REPORT_END_AND_PRINT(reg_mat_sets, "Register Material Sets: ");

    // Models.
    TIMING_REPORT_START(upload_combined_mesh);
    gltf_loader::load_gltf("assets/models/slime_girl.glb");  // @TODO: figure out way to string lookup models.
    gltf_loader::load_gltf("assets/models/enemy_wip.glb");
    gltf_loader::load_gltf("assets/models/box.gltf");
    gltf_loader::upload_combined_mesh(m_pimpl.m_immediate_submit_support,
                                      m_pimpl.m_v_device,
                                      m_pimpl.m_v_graphics_queue,
                                      m_pimpl.m_v_vma_allocator);
    TIMING_REPORT_END_AND_PRINT(upload_combined_mesh, "Load All Models and Upload Combined Mesh: ");

    // @DEBUG: @NOCHECKIN: create some instances (for testing). //
    geo_instance::register_geo_instance(geo_instance::Geo_instance{
        .model_idx = 0,  // @TODO: figure out way to string lookup models.
        .render_pass = geo_instance::Geo_render_pass::OPAQUE,
        .is_shadow_caster = true,
        .gpu_instance_data{
            .material_param_set_idx = material_bank::get_mat_set_idx_from_name("slime_girl_mat_set_0")
        },
    });
    geo_instance::register_geo_instance(geo_instance::Geo_instance{
        .model_idx = 1,  // @TODO: figure out way to string lookup models.
        .render_pass = geo_instance::Geo_render_pass::OPAQUE,
        .is_shadow_caster = true,
        .gpu_instance_data{
            .material_param_set_idx = material_bank::get_mat_set_idx_from_name("enemy_wip_mat_set_0")
        },
    });
    geo_instance::register_geo_instance(geo_instance::Geo_instance{
        .model_idx = 2,  // @TODO: figure out way to string lookup models.
        .render_pass = geo_instance::Geo_render_pass::OPAQUE,
        .is_shadow_caster = true,
        .gpu_instance_data{
            .material_param_set_idx = material_bank::get_mat_set_idx_from_name("box_mat_set_0")
        },
    });
    //////////////////////////////////////////////////////////////

    // Upload material param indices and material sets.
    TIMING_REPORT_START(upload_material_sets);
    vk_buffer::upload_material_param_sets_to_gpu(m_pimpl.m_v_geo_passes_resource_buffer,
                                                 m_pimpl.m_immediate_submit_support,
                                                 m_pimpl.m_v_device,
                                                 m_pimpl.m_v_graphics_queue,
                                                 m_pimpl.m_v_vma_allocator,
                                                 material_bank::get_all_material_sets());
    m_pimpl.write_material_param_sets_to_descriptor_sets();
    TIMING_REPORT_END_AND_PRINT(upload_material_sets, "Upload Material Param Indices and Material Sets: ");

    // Upload material param datas.
    TIMING_REPORT_START(upload_material_param_datas);
    material_bank::cook_and_upload_pipeline_material_param_datas_to_gpu(
        m_pimpl.m_immediate_submit_support,
        m_pimpl.m_v_device,
        m_pimpl.m_v_graphics_queue,
        m_pimpl.m_v_vma_allocator,
        m_pimpl.m_v_descriptor_alloc);
    TIMING_REPORT_END_AND_PRINT(upload_material_param_datas, "Upload Material Param Datas for Pipeline: ");


    // Upload bounding sphere data.
    TIMING_REPORT_START(upload_bs);
    vk_buffer::upload_bounding_spheres_to_gpu(m_pimpl.m_v_geo_passes_resource_buffer,
                                              m_pimpl.m_immediate_submit_support,
                                              m_pimpl.m_v_device,
                                              m_pimpl.m_v_graphics_queue,
                                              m_pimpl.m_v_vma_allocator,
                                              gltf_loader::get_all_bounding_spheres());
    m_pimpl.write_bounding_spheres_to_descriptor_sets();
    TIMING_REPORT_END_AND_PRINT(upload_bs, "Upload Bounding Spheres: ");

    // Report memory heap usage.
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_pimpl.m_v_vma_allocator, budgets);
    for (uint32_t i = 0; i < m_pimpl.m_v_vma_allocator->GetMemoryHeapCount(); i++)
    {
        uint32_t allocs{ budgets[i].statistics.allocationCount };
        float_t allocs_mb{ budgets[i].statistics.allocationBytes / 1024.0f / 1024.0f };
        uint32_t blocks{ budgets[i].statistics.blockCount };
        float_t blocks_mb{ budgets[i].statistics.blockBytes / 1024.0f / 1024.0f };
        float_t usage_mb{ budgets[i].usage / 1024.0f / 1024.0f };
        float_t budget_mb{ budgets[i].budget / 1024.0f / 1024.0f };
        std::cout
            << (i == 0 ? "-=-=- GPU Memory Report -=-=-" : "") << std::endl
            << "# Memory Heap " << i << std::endl
            << "  " << allocs << " allocations (" << allocs_mb << " MB)" << std::endl
            << "  " << blocks << " blocks (" << blocks_mb << " MB)" << std::endl
            << "  " << usage_mb << " MB usage / " << budget_mb << " MB budget" << std::endl;
    }
    return 0;
}

int32_t Monolithic_renderer::Impl::Update_data_job::execute()
{
    bool success{ true };
    success &= m_pimpl.update_window();
    return success ? 0 : 1;
}

int32_t Monolithic_renderer::Impl::Render_job::execute()
{
    bool success{ true };
    success &= m_pimpl.update_and_upload_render_data();
    success &= m_pimpl.render();
    return success ? 0 : 1;
}

int32_t Monolithic_renderer::Impl::Teardown_job::execute()
{
    bool success{ true };
    success &= m_pimpl.wait_for_renderer_idle();
    success &= gltf_loader::teardown_all_meshes();
    success &= material_bank::teardown_all_materials();
    success &= m_pimpl.teardown_imgui();
    success &= m_pimpl.teardown_vulkan_renderer();
    success &= m_pimpl.teardown_window();

    // Mark finishing shutdown.
    m_pimpl.m_finished_shutdown = true;

    return success ? 0 : 1;
}

// Job source callback.
Job_source::Job_next_jobs_return_data Monolithic_renderer::Impl::fetch_next_jobs_callback()
{
    Job_next_jobs_return_data return_data;

    switch (m_stage.load())
    {
        case Stage::BUILD:
            return_data.jobs = {
                m_build_job.get(),
            };
            m_stage = Stage::LOAD_ASSETS;
            break;

        case Stage::LOAD_ASSETS:
            return_data.jobs = {
                m_load_assets_job.get(),
            };
            m_stage = Stage::UPDATE_DATA;
            break;

        case Stage::UPDATE_DATA:
            return_data.jobs = {
                m_update_data_job.get(),
            };
            m_stage = Stage::RENDER;
            break;

        case Stage::RENDER:
            return_data.jobs = {
                m_render_job.get(),
            };
            // @NOTE: the render job checks if a shutdown is
            //        requested, and at that point the stage
            //        will be set to teardown instead of update.
            m_stage = Stage::UPDATE_DATA;
            break;

        case Stage::TEARDOWN:
            return_data.jobs = {
                m_teardown_job.get(),
            };
            m_stage = Stage::END_OF_LIFE;
            break;

        case Stage::END_OF_LIFE:
            return_data.signal_end_of_life = true;
            break;
    }

    return return_data;
}

// Win64 window setup/teardown.
bool Monolithic_renderer::Impl::build_window()
{
    // Setup GLFW window.
    if (!glfwInit())
    {
        std::cerr << "ERROR: GLFW Init failed." << std::endl;
        return false;
    }

    struct Monitor_workarea
    {
        int32_t xpos;
        int32_t ypos;
        int32_t width;
        int32_t height;
    } monitor_workarea;
    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(),
                           &monitor_workarea.xpos,
                           &monitor_workarea.ypos,
                           &monitor_workarea.width,
                           &monitor_workarea.height);
    
    if (m_window_width >= monitor_workarea.width ||
        m_window_height >= monitor_workarea.height)
    {
        // Use fallback window sizing.
        assert(m_fallback_window_width < monitor_workarea.width);
        assert(m_fallback_window_height < monitor_workarea.height);
        m_window_width = m_fallback_window_width;
        m_window_height = m_fallback_window_height;
    }

    int32_t centered_window_pos[2]{
        monitor_workarea.xpos
            + static_cast<int32_t>(monitor_workarea.width * 0.5
                - m_window_width * 0.5),
        monitor_workarea.ypos
            + static_cast<int32_t>(monitor_workarea.height * 0.5
                - m_window_height * 0.5),
    };

    glfwWindowHint(GLFW_POSITION_X, centered_window_pos[0]);
    glfwWindowHint(GLFW_POSITION_Y, centered_window_pos[1]);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // @TODO: Have change sizing functions (but not controllable from directly controlling the window).

    m_window = glfwCreateWindow(m_window_width, m_window_height, m_name.c_str(), nullptr, nullptr);
    if (!m_window)
    {
        std::cerr << "ERROR: Window creation failed." << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetKeyCallback(m_window, key_callback);
    glfwSetWindowFocusCallback(m_window, window_focus_callback);
    glfwSetWindowIconifyCallback(m_window, window_iconify_callback);

    return true;
}

bool Monolithic_renderer::Impl::teardown_window()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
    return true;
}

// Vulkan renderer setup/teardown.
bool build_vulkan_renderer__vulkan(GLFWwindow* window,
                                   VkInstance& out_instance,
#if _DEBUG
                                   VkDebugUtilsMessengerEXT& out_debug_utils_messenger,
#endif
                                   VkSurfaceKHR& out_surface,
                                   VkPhysicalDevice& out_physical_device,
                                   VkPhysicalDeviceProperties& out_physical_device_properties,
                                   VkDevice& out_device,
                                   vkb::Device& out_vkb_device)
{
    VkResult err;

    // Build vulkan instance (targeting Vulkan 1.3).
    vkb::InstanceBuilder builder;
    vkb::Result<vkb::Instance> instance_build_result{
        builder
            .set_app_name("Hawsoo Monolithic Renderer")
            .require_api_version(1, 3, 0)
#if _DEBUG
            .request_validation_layers(true)
            .use_default_debug_messenger()
#endif
            .build()
    };
    if (!instance_build_result.has_value())
    {
        std::cerr << "ERROR: Vulkan instance creation failed." << std::endl;
        return false;
    }

    vkb::Instance instance{ instance_build_result.value() };
    out_instance = instance.instance;
#if _DEBUG
    out_debug_utils_messenger = instance.debug_messenger;
#endif

    // Build presentation surface.
    err = glfwCreateWindowSurface(out_instance, window, nullptr, &out_surface);
    if (err)
    {
        std::cerr << "ERROR: Vulkan surface creation failed." << std::endl;
        return false;
    }

    // Vulkan 1.3 features.
    VkPhysicalDeviceVulkan13Features vulkan13_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = nullptr,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    // Vulkan 1.2 features.
    VkPhysicalDeviceVulkan12Features vulkan12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
        // For `vkCmdDrawIndexedIndirectCount`.
        .drawIndirectCount = VK_TRUE,
        // For non-uniform, dynamic arrays of textures in shaders.
        .descriptorIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        // For MIN/MAX sampler when creating mip chains for occlusion culling.
        .samplerFilterMinmax = VK_TRUE,
        // For buffer references in the stead of descriptor sets.
        .bufferDeviceAddress = VK_TRUE,
    };

    // Select physical device.
    vkb::PhysicalDeviceSelector selector{ instance };
    vkb::PhysicalDevice physical_device{
        selector
            .set_minimum_version(1, 3)
            .set_required_features_13(vulkan13_features)
            .set_required_features_12(vulkan12_features)
            .set_surface(out_surface)
            .set_required_features({
                // @NOTE: @FEATURES: Enable required features right here
                // .multiDrawIndirect = VK_TRUE,         // So that vkCmdDrawIndexedIndirect() can be called with a >1 drawCount. (@NOTE: not happening with current setup)
                .depthClamp = VK_TRUE,				  // For shadow maps, this is really nice.
                .fillModeNonSolid = VK_TRUE,          // To render wireframes.
                .samplerAnisotropy = VK_TRUE,
                .fragmentStoresAndAtomics = VK_TRUE,  // For the picking buffer! @TODO: If a release build then disable.
            })
            .select()
            .value()
    };
    out_physical_device = physical_device.physical_device;
    out_physical_device_properties = physical_device.properties;

    // Print phsyical device properties.
    constexpr uint32_t k_built_sdk_version{ VK_HEADER_VERSION_COMPLETE };
    std::cout << "-=-=- Chosen Physical Device Properties -=-=-" << std::endl;
    std::cout << "BUILT_SDK_VERSION                 : " << VK_API_VERSION_MAJOR(k_built_sdk_version) << "." << VK_API_VERSION_MINOR(k_built_sdk_version) << "." << VK_API_VERSION_PATCH(k_built_sdk_version) << "." << VK_API_VERSION_VARIANT(k_built_sdk_version) << std::endl;
    std::cout << "API_VERSION                       : " << VK_API_VERSION_MAJOR(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_MINOR(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_PATCH(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_VARIANT(out_physical_device_properties.apiVersion) << std::endl;
    std::cout << "DRIVER_VERSION(raw)               : " << out_physical_device_properties.driverVersion << std::endl;
    std::cout << "VENDOR_ID                         : " << out_physical_device_properties.vendorID << std::endl;
    std::cout << "DEVICE_ID                         : " << out_physical_device_properties.deviceID << std::endl;
    std::cout << "DEVICE_TYPE                       : " << out_physical_device_properties.deviceType << std::endl;
    std::cout << "DEVICE_NAME                       : " << out_physical_device_properties.deviceName << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_1D            : " << out_physical_device_properties.limits.maxImageDimension1D << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_2D            : " << out_physical_device_properties.limits.maxImageDimension2D << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_3D            : " << out_physical_device_properties.limits.maxImageDimension3D << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_CUBE          : " << out_physical_device_properties.limits.maxImageDimensionCube << std::endl;
    std::cout << "MAX_IMAGE_ARRAY_LAYERS            : " << out_physical_device_properties.limits.maxImageArrayLayers << std::endl;
    std::cout << "MAX_SAMPLER_ANISOTROPY            : " << out_physical_device_properties.limits.maxSamplerAnisotropy << std::endl;
    std::cout << "MAX_BOUND_DESCRIPTOR_SETS         : " << out_physical_device_properties.limits.maxBoundDescriptorSets << std::endl;
    std::cout << "MINIMUM_BUFFER_ALIGNMENT          : " << out_physical_device_properties.limits.minUniformBufferOffsetAlignment << std::endl;
    std::cout << "MAX_COLOR_ATTACHMENTS             : " << out_physical_device_properties.limits.maxColorAttachments << std::endl;
    std::cout << "MAX_DRAW_INDIRECT_COUNT           : " << out_physical_device_properties.limits.maxDrawIndirectCount << std::endl;
    std::cout << "MAX_DESCRIPTOR_SET_SAMPLED_IMAGES : " << out_physical_device_properties.limits.maxDescriptorSetSampledImages << std::endl;
    std::cout << "MAX_DESCRIPTOR_SET_SAMPLERS       : " << out_physical_device_properties.limits.maxDescriptorSetSamplers << std::endl;
    std::cout << "MAX_SAMPLER_ALLOCATION_COUNT      : " << out_physical_device_properties.limits.maxSamplerAllocationCount << std::endl;
    std::cout << std::endl;

    // Build Vulkan device.
    vkb::DeviceBuilder device_builder{ physical_device };
    VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };
    out_vkb_device =
        device_builder
            .add_pNext(&shader_draw_parameters_features)
            .build()
            .value();
    out_device = out_vkb_device.device;

    return true;
}

bool build_vulkan_renderer__allocator(VkInstance instance,
                                      VkPhysicalDevice physical_device,
                                      VkDevice device,
                                      VmaAllocator& out_allocator)
{
    // Initialize VMA.
    VmaAllocatorCreateInfo vma_allocator_info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,  // To access GPU pointers.
        .physicalDevice = physical_device,
        .device = device,
        .instance = instance,
    };
    vmaCreateAllocator(&vma_allocator_info, &out_allocator);

    return true;
}

bool build_vulkan_renderer__swapchain(VkSurfaceKHR surface,
                                      VkPhysicalDevice physical_device,
                                      VkDevice device,
                                      int32_t window_width,
                                      int32_t window_height,
                                      VkSwapchainKHR& out_swapchain,
                                      std::vector<VkImage>& out_swapchain_images,
                                      std::vector<VkImageView>& out_swapchain_image_views,
                                      VkFormat& out_swapchain_image_format,
                                      VkExtent2D& out_swapchain_extent)
{
    // Build swapchain.
    // @TODO: Make swapchain rebuilding a thing.
    vkb::SwapchainBuilder swapchain_builder{ physical_device, device, surface };
    vkb::Swapchain swapchain{
        swapchain_builder
            .use_default_format_selection()
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)  // Mailbox (G-Sync/Freesync compatible).
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)  // FIFO (V-Sync).
            .set_desired_extent(window_width, window_height)
            // @TODO: TRANSFER_DST image usage added below. Try removing once renderer is finished
            // (assuming you're not gonna have some kind of image transfer as the last step into the swapchain image).
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value()
    };
    out_swapchain = swapchain.swapchain;
    out_swapchain_images = swapchain.get_images().value();
    out_swapchain_image_views = swapchain.get_image_views().value();
    out_swapchain_image_format = swapchain.image_format;
    out_swapchain_extent.width = window_width;
    out_swapchain_extent.height = window_height;

    return true;
}

bool build_vulkan_renderer__hdr_image(VmaAllocator allocator,
                                      VkDevice device,
                                      int32_t window_width,
                                      int32_t window_height,
                                      vk_image::Allocated_image& out_hdr_image,
                                      VkExtent2D& out_hdr_image_extent)
{
    // Create HDR draw image.
    VkExtent3D extent{
        static_cast<uint32_t>(window_width),
        static_cast<uint32_t>(window_height),
        1
    };

    out_hdr_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    out_hdr_image.image_extent = extent;

    VkImageUsageFlags usages{};
    usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT
              | VK_IMAGE_USAGE_TRANSFER_DST_BIT
              | VK_IMAGE_USAGE_STORAGE_BIT
              | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo image_info{
        vk_util::image_create_info(out_hdr_image.image_format,
                                   usages,
                                   extent)
    };

    VmaAllocationCreateInfo image_alloc_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags{ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT },
    };
    vmaCreateImage(allocator,
                   &image_info,
                   &image_alloc_info,
                   &out_hdr_image.image,
                   &out_hdr_image.allocation,
                   nullptr);

    VkImageViewCreateInfo image_view_info{
        vk_util::image_view_create_info(out_hdr_image.image_format,
                                        out_hdr_image.image,
                                        VK_IMAGE_ASPECT_COLOR_BIT)
    };

    VkResult err{
        vkCreateImageView(device, &image_view_info, nullptr, &out_hdr_image.image_view)
    };
    if (err)
    {
        std::cerr << "ERROR: Create `hdr_image` image view failed." << std::endl;
        assert(false);
    }

    // Set 2D extent.
    out_hdr_image_extent.width = out_hdr_image.image_extent.width;
    out_hdr_image_extent.height = out_hdr_image.image_extent.height;

    return true;
}

bool build_vulkan_renderer__retrieve_queues(vkb::Device& vkb_device,
                                            VkQueue& out_graphics_queue,
                                            uint32_t& out_graphics_queue_family_idx)
{
    // Retrieve graphics queue.
    out_graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    out_graphics_queue_family_idx = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
    // @TODO: implement this: https://vkguide.dev/docs/new_chapter_1/vulkan_command_flow/#:~:text=It%20is%20common%20to%20see%20engines%20using%203%20queue%20families.%20One%20for%20drawing%20the%20frame%2C%20other%20for%20async%20compute%2C%20and%20other%20for%20data%20transfer.%20In%20this%20tutorial%2C%20we%20use%20a%20single%20queue%20that%20will%20run%20all%20our%20commands%20for%20simplicity.

    return true;
}

bool build_vulkan_renderer__cmd_structures(uint32_t graphics_queue_family_idx,
                                           VkDevice device,
                                           Frame_data out_frames[])
{
    VkResult err;

    VkCommandPoolCreateInfo cmd_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_queue_family_idx
    };

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateCommandPool(device, &cmd_pool_info, nullptr, &out_frames[i].command_pool);
        if (err)
        {
            std::cerr << "ERROR: Vulkan command pool creation failed for frame #" << i << std::endl;
            return false;
        }

        // Allocate default cmd buffer for rendering.
        VkCommandBufferAllocateInfo cmd_alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = out_frames[i].command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        err = vkAllocateCommandBuffers(device, &cmd_alloc_info, &out_frames[i].main_command_buffer);
        if (err)
        {
            std::cerr << "ERROR: Vulkan command pool allocation failed for frame #" << i << std::endl;
            return false;
        }
    }

    return true;
}

bool build_vulkan_renderer__sync_structures(VkDevice device, Frame_data out_frames[])
{
    VkResult err;

    // Create fence to sync when gpu has finished rendering the frame.
    VkFenceCreateInfo fence_create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,  // For waiting on it for the first frame.
    };

    // Create semaphores for syncing swapchain rendering.
    VkSemaphoreCreateInfo semaphore_create_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateFence(device, &fence_create_info, nullptr, &out_frames[i].render_fence);
        if (err)
        {
            std::cerr << "ERROR: Vulkan render fence creation failed for frame #" << i << std::endl;
            return false;
        }

        err = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &out_frames[i].swapchain_semaphore);
        if (err)
        {
            std::cerr << "ERROR: Vulkan swapchain semaphore creation failed for frame #" << i << std::endl;
            return false;
        }

        err = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &out_frames[i].render_semaphore);
        if (err)
        {
            std::cerr << "ERROR: Vulkan render semaphore creation failed for frame #" << i << std::endl;
            return false;
        }
    }

    return true;
}

bool build_vulkan_renderer__descriptors(VkDevice device,
                                        VkImageView hdr_image_view,
                                        vk_desc::Descriptor_allocator& out_descriptor_alloc,
                                        VkDescriptorSetLayout& out_descriptor_layout,
                                        VkDescriptorSet& out_descriptor_set)
{
    // Init allocator pool.
    std::vector<vk_desc::Descriptor_allocator::Pool_size_ratio> sizes{
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };

    out_descriptor_alloc.init_pool(device, 10, sizes);

    // Build layout.
    vk_desc::Descriptor_layout_builder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    out_descriptor_layout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

    // Allocate descriptor set.
    out_descriptor_set = out_descriptor_alloc.allocate(device, out_descriptor_layout);

    VkDescriptorImageInfo image_info{
        .imageView = hdr_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet image_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = out_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(device, 1, &image_write, 0, nullptr);

    return true;
}

struct GPU_geometry_culling_push_constants
{
    float_t         z_near;
    float_t         z_far;
    float_t         frustum_x_x;
    float_t         frustum_x_z;
    float_t         frustum_y_y;
    float_t         frustum_y_z;
    uint32_t        culling_enabled;
    uint32_t        num_instances;
    VkDeviceAddress instance_buffer_address;
    VkDeviceAddress visible_result_buffer_address;
};

struct GPU_write_draw_cmds_push_constants
{
    uint32_t        num_primitives;
    VkDeviceAddress visible_result_buffer_address;
    VkDeviceAddress base_indices_buffer_address;
    VkDeviceAddress count_buffer_indices_buffer_address;
    VkDeviceAddress draw_commands_input_buffer_address;
    VkDeviceAddress draw_commands_output_buffer_address;
    VkDeviceAddress draw_command_counts_buffer_address;
};

using Geometry_graphics_pass = Monolithic_renderer::Impl::Geometry_graphics_pass;
bool build_vulkan_renderer__geometry_graphics_pass(VkDevice device,
                                                   VmaAllocator allocator,
                                                   vk_desc::Descriptor_allocator& descriptor_alloc,
                                                   Frame_data out_frames[],
                                                   Geometry_graphics_pass& out_geom_graphics_pass)
{
    // Per-frame datas.
    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        auto& camera_buffer{ out_frames[i].camera_buffer };
        auto& frame{ out_geom_graphics_pass.per_frame_datas[i] };
        const auto& frame_buffers{
            out_frames[i].geo_per_frame_buffer };

        // Camera descriptor set.
        camera_buffer =
            vk_buffer::create_buffer(allocator,
                                     sizeof(camera::GPU_camera),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);
        
        // Build layout.
        vk_desc::Descriptor_layout_builder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        frame.camera_data.descriptor_layout =
            builder.build(device, VK_SHADER_STAGE_VERTEX_BIT);
        
        // Build and allocate descriptor set.
        frame.camera_data.descriptor_set =
            descriptor_alloc.allocate(device, frame.camera_data.descriptor_layout);

        VkDescriptorBufferInfo camera_buffer_info{
            .buffer = camera_buffer.buffer,
            .offset = 0,
            .range = sizeof(camera::GPU_camera),
        };
        VkWriteDescriptorSet camera_buffer_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = frame.camera_data.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &camera_buffer_info,
        };
        vkUpdateDescriptorSets(device, 1, &camera_buffer_write, 0, nullptr);
    }

    // Material param sets data.
    auto& material_param_sets_data{ out_geom_graphics_pass.material_param_sets_data };

    // Build layout.
    vk_desc::Descriptor_layout_builder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    material_param_sets_data.descriptor_layout =
        builder.build(device, VK_SHADER_STAGE_VERTEX_BIT);

    // Build and allocate descriptor set.
    material_param_sets_data.descriptor_set =
        descriptor_alloc.allocate(device, material_param_sets_data.descriptor_layout);

    // @NOTE: Defer write buffers to descriptor set until once they are created.
    //        `write_material_param_sets_to_descriptor_sets()`

    // Bounding spheres data.
    auto& bounding_spheres_data{ out_geom_graphics_pass.bounding_spheres_data };

    // Build layout.
    builder.clear();
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    bounding_spheres_data.descriptor_layout =
        builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

    // Build and allocate descriptor set.
    bounding_spheres_data.descriptor_set =
        descriptor_alloc.allocate(device, bounding_spheres_data.descriptor_layout);

    // @NOTE: Defer write buffers to descriptor set until once they are created.
    //        `write_bounding_spheres_to_descriptor_sets()`

    // Material param definition desc layout.
    builder.clear();
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    out_geom_graphics_pass.material_param_definition_descriptor_layout =
        builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);

    {
        // Build culling pipeline layout.
        VkPushConstantRange pc_range{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(GPU_geometry_culling_push_constants),
        };

        VkDescriptorSetLayout desc_layouts[]{
            out_geom_graphics_pass.per_frame_datas.front().camera_data.descriptor_layout,
            out_geom_graphics_pass.bounding_spheres_data.descriptor_layout,
        };

        VkPipelineLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 2,
            .pSetLayouts = desc_layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pc_range,
        };
        VkResult err{
            vkCreatePipelineLayout(device,
                                   &layout_info,
                                   nullptr,
                                   &out_geom_graphics_pass.culling_pipeline_layout) };
        if (err)
        {
            std::cerr << "ERROR: Pipeline layout creation failed." << std::endl;
            assert(false);
        }

        // Build culling pipeline.
        VkShaderModule compute_draw_shader;
        if (!vk_pipeline::load_shader_module(("assets/shaders/geom_culling.comp.spv"),
                                             device,
                                             compute_draw_shader))
        {
            std::cerr << "ERROR: Shader module loading failed." << std::endl;
            assert(false);
        }

        VkComputePipelineCreateInfo pipeline_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .stage = vk_util::pipeline_shader_stage_info(VK_SHADER_STAGE_COMPUTE_BIT,
                                                         compute_draw_shader),
            .layout = out_geom_graphics_pass.culling_pipeline_layout,
        };

        err = vkCreateComputePipelines(device,
                                       VK_NULL_HANDLE,
                                       1, &pipeline_info,
                                       nullptr,
                                       &out_geom_graphics_pass.culling_pipeline);
        if (err)
        {
            std::cerr << "ERROR: Create compute pipeline failed." << std::endl;
        }

        // Clean up shader modules.
        vkDestroyShaderModule(device, compute_draw_shader, nullptr);
    }

    {
        // Build write draw cmds pipeline layout.
        VkPushConstantRange pc_range{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(GPU_write_draw_cmds_push_constants),
        };

        VkPipelineLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pc_range,
        };
        VkResult err{
            vkCreatePipelineLayout(device,
                                   &layout_info,
                                   nullptr,
                                   &out_geom_graphics_pass.write_draw_cmds_pipeline_layout) };
        if (err)
        {
            std::cerr << "ERROR: Pipeline layout creation failed." << std::endl;
            assert(false);
        }

        // Build write draw cmds pipeline.
        VkShaderModule compute_draw_shader;
        if (!vk_pipeline::load_shader_module(("assets/shaders/geom_write_draw_cmds.comp.spv"),
                                             device,
                                             compute_draw_shader))
        {
            std::cerr << "ERROR: Shader module loading failed." << std::endl;
            assert(false);
        }

        VkComputePipelineCreateInfo pipeline_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .stage = vk_util::pipeline_shader_stage_info(VK_SHADER_STAGE_COMPUTE_BIT,
                                                         compute_draw_shader),
            .layout = out_geom_graphics_pass.write_draw_cmds_pipeline_layout,
        };

        err = vkCreateComputePipelines(device,
                                       VK_NULL_HANDLE,
                                       1, &pipeline_info,
                                       nullptr,
                                       &out_geom_graphics_pass.write_draw_cmds_pipeline);
        if (err)
        {
            std::cerr << "ERROR: Create compute pipeline failed." << std::endl;
        }

        // Clean up shader modules.
        vkDestroyShaderModule(device, compute_draw_shader, nullptr);
    }

    return true;
}

bool build_vulkan_renderer__pipelines(VkDevice device,
                                      VkDescriptorSetLayout descriptor_layout,
                                      VkPipelineLayout& out_pipeline_layout,
                                      VkPipeline& out_pipeline)
{
    // Create pipeline layout.
    VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_layout,
    };
    VkResult err{
        vkCreatePipelineLayout(device, &layout_info, nullptr, &out_pipeline_layout) };
    if (err)
    {
        std::cerr << "ERROR: Pipeline layout creation failed." << std::endl;
        assert(false);
    }

    // Create pipeline.
    VkShaderModule compute_draw_shader;
    if (!vk_pipeline::load_shader_module(("assets/shaders/gradient.comp.spv"),
                                         device,
                                         compute_draw_shader))
    {
        std::cerr << "ERROR: Shader module loading failed." << std::endl;
        assert(false);
    }

    VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = vk_util::pipeline_shader_stage_info(VK_SHADER_STAGE_COMPUTE_BIT,
                                                     compute_draw_shader),
        .layout = out_pipeline_layout,
    };

    err = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &out_pipeline);
    if (err)
    {
        std::cerr << "ERROR: Create compute pipeline failed." << std::endl;
    }

    // Clean up shader modules.
    vkDestroyShaderModule(device, compute_draw_shader, nullptr);

    return true;
}

bool build_vulkan_renderer__triangle_graphic_pipeline(VkDevice device,
                                                      VkFormat draw_format,
                                                      VkPipelineLayout& out_pipeline_layout,
                                                      VkPipeline& out_pipeline)
{
    // ------------------------------------------------------------------------
    // Create Graphics pipeline for colored triangle
    // @NOCHECKIN: @THEA
    // ------------------------------------------------------------------------
    VkShaderModule triangle_vert_shader;
    if (!vk_pipeline::load_shader_module(("assets/shaders/colored_triangle.vert.spv"),
                                         device,
                                         triangle_vert_shader))
    {
        std::cerr << "ERROR: Shader module loading failed." << std::endl;
        assert(false);
    }
    VkShaderModule triangle_frag_shader;
    if (!vk_pipeline::load_shader_module(("assets/shaders/colored_triangle.frag.spv"),
                                         device,
                                         triangle_frag_shader))
    {
        std::cerr << "ERROR: Shader module loading failed." << std::endl;
        assert(false);
    }

    // Create pipeline layout.
    VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
    };
    VkResult err{
        vkCreatePipelineLayout(device, &layout_info, nullptr, &out_pipeline_layout) };
    if (err)
    {
        std::cerr << "ERROR: Pipeline layout creation failed." << std::endl;
        assert(false);
    }

    // Create pipeline.
    vk_pipeline::Graphics_pipeline_builder builder;
    builder.set_pipeline_layout(out_pipeline_layout);
    builder.set_shaders(triangle_vert_shader, triangle_frag_shader);
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    builder.set_multisampling_none();
    builder.disable_blending();
    builder.disable_depthtest();
    builder.set_color_attachment_format(draw_format);
    builder.set_depth_format(VK_FORMAT_UNDEFINED);
    out_pipeline = builder.build_pipeline(device);

    // Clean up.
    vkDestroyShaderModule(device, triangle_vert_shader, nullptr);
    vkDestroyShaderModule(device, triangle_frag_shader, nullptr);

    return true;
}

bool Monolithic_renderer::Impl::build_vulkan_renderer()
{
    bool result{ true };

    TIMING_REPORT_START(build_vulkan_renderer);

    vkb::Device vkb_device;
    result &= build_vulkan_renderer__vulkan(m_window,
                                            m_v_instance,
#if _DEBUG
                                            m_v_debug_utils_messenger,
#endif
                                            m_v_surface,
                                            m_v_physical_device,
                                            m_v_physical_device_properties,
                                            m_v_device,
                                            vkb_device);
    result &= build_vulkan_renderer__allocator(m_v_instance,
                                               m_v_physical_device,
                                               m_v_device,
                                               m_v_vma_allocator);
    result &= build_vulkan_renderer__swapchain(m_v_surface,
                                               m_v_physical_device,
                                               m_v_device,
                                               m_window_width,
                                               m_window_height,
                                               m_v_swapchain.swapchain,
                                               m_v_swapchain.images,
                                               m_v_swapchain.image_views,
                                               m_v_swapchain.image_format,
                                               m_v_swapchain.extent);
    result &= build_vulkan_renderer__hdr_image(m_v_vma_allocator,
                                               m_v_device,
                                               m_window_width,
                                               m_window_height,
                                               m_v_HDR_draw_image.image,
                                               m_v_HDR_draw_image.extent);
    result &= build_vulkan_renderer__retrieve_queues(vkb_device,
                                                     m_v_graphics_queue,
                                                     m_v_graphics_queue_family_idx);
    vk_util::init_immediate_submit_support(m_immediate_submit_support,
                                           m_v_device,
                                           m_v_graphics_queue_family_idx);
    result &= build_vulkan_renderer__cmd_structures(m_v_graphics_queue_family_idx,
                                                    m_v_device,
                                                    m_frames);
    result &= build_vulkan_renderer__sync_structures(m_v_device,
                                                     m_frames);
    result &= build_vulkan_renderer__descriptors(m_v_device,
                                                 m_v_HDR_draw_image.image.image_view,
                                                 m_v_descriptor_alloc,
                                                 m_v_sample_pass.descriptor_layout,
                                                 m_v_sample_pass.descriptor_set);
    for (size_t i = 0; i < k_frame_overlap; i++)
    {
        // @TODO: add into a process func.
        //        For `__geometry_graphics_pass()`
        vk_buffer::initialize_base_sized_per_frame_buffer(m_v_device,
                                                          m_v_vma_allocator,
                                                          m_frames[i].geo_per_frame_buffer);
    }
    result &= build_vulkan_renderer__geometry_graphics_pass(m_v_device,
                                                            m_v_vma_allocator,
                                                            m_v_descriptor_alloc,
                                                            m_frames,
                                                            m_v_geometry_graphics_pass);
    result &= build_vulkan_renderer__pipelines(m_v_device,
                                               m_v_sample_pass.descriptor_layout,
                                               m_v_sample_pass.pipeline_layout,
                                               m_v_sample_pass.pipeline);
    result &= build_vulkan_renderer__triangle_graphic_pipeline(m_v_device,
                                                               m_v_HDR_draw_image.image.image_format,
                                                               m_v_sample_graphics_pass.pipeline_layout,
                                                               m_v_sample_graphics_pass.pipeline);

    TIMING_REPORT_END_AND_PRINT(build_vulkan_renderer, "Build Vulkan Renderer: ");

    return result;
}

bool teardown_vulkan_renderer__vulkan(VkInstance instance,
#if _DEBUG
                                      VkDebugUtilsMessengerEXT debug_utils_messenger,
#endif
                                      VkSurfaceKHR surface,
                                      VkDevice device)
{
    vkDestroyDevice(device, nullptr);
    // @NOTE: Vulkan surface created from GLFW must be destroyed with vk func.
    vkDestroySurfaceKHR(instance, surface, nullptr);
#if _DEBUG
    vkb::destroy_debug_utils_messenger(instance, debug_utils_messenger);
#endif
    vkDestroyInstance(instance, nullptr);

    return true;
}

bool teardown_vulkan_renderer__allocator(VmaAllocator allocator)
{
    vmaDestroyAllocator(allocator);
    return true;
}

bool teardown_vulkan_renderer__swapchain(VkDevice device,
                                         VkSwapchainKHR swapchain,
                                         std::vector<VkImageView>& swapchain_image_views)
{
    for (auto image_view : swapchain_image_views)
        vkDestroyImageView(device, image_view, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    return true;
}

bool teardown_vulkan_renderer__hdr_image(VmaAllocator allocator,
                                         VkDevice device,
                                         const vk_image::Allocated_image& hdr_image)
{
    vkDestroyImageView(device, hdr_image.image_view, nullptr);
    vmaDestroyImage(allocator, hdr_image.image, hdr_image.allocation);
    return true;
}

bool teardown_vulkan_renderer__cmd_structures(VkDevice device, Frame_data frames[])
{
    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        vkDestroyCommandPool(device, frames[i].command_pool, nullptr);
    }
    return true;
}

bool teardown_vulkan_renderer__sync_structures(VkDevice device, Frame_data frames[])
{
    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        vkDestroyFence(device, frames[i].render_fence, nullptr);
        vkDestroySemaphore(device, frames[i].render_semaphore, nullptr);
        vkDestroySemaphore(device, frames[i].swapchain_semaphore, nullptr);
    }
    return true;
}

bool teardown_vulkan_renderer__descriptors(VkDevice device,
                                           vk_desc::Descriptor_allocator& descriptor_alloc,
                                           VkDescriptorSetLayout descriptor_layout)
{
    descriptor_alloc.destroy_pool(device);
    vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
    return true;
}

bool teardown_vulkan_renderer__pipelines(VkDevice device,
                                         VkPipelineLayout pipeline_layout,
                                         VkPipeline pipeline)
{
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    return true;
}

bool Monolithic_renderer::Impl::teardown_vulkan_renderer()
{
    bool result{ true };
    result &= teardown_vulkan_renderer__pipelines(m_v_device,
                                                  m_v_sample_graphics_pass.pipeline_layout,
                                                  m_v_sample_graphics_pass.pipeline);
    result &= teardown_vulkan_renderer__pipelines(m_v_device,
                                                  m_v_sample_pass.pipeline_layout,
                                                  m_v_sample_pass.pipeline);
    result &= teardown_vulkan_renderer__descriptors(m_v_device,
                                                    m_v_descriptor_alloc,
                                                    m_v_sample_pass.descriptor_layout);
    result &= teardown_vulkan_renderer__sync_structures(m_v_device, m_frames);
    result &= teardown_vulkan_renderer__cmd_structures(m_v_device, m_frames);
    vk_util::destroy_immediate_submit_support(m_immediate_submit_support,
                                              m_v_device);
    result &= teardown_vulkan_renderer__hdr_image(m_v_vma_allocator,
                                                  m_v_device,
                                                  m_v_HDR_draw_image.image);
    result &= teardown_vulkan_renderer__swapchain(m_v_device,
                                                  m_v_swapchain.swapchain,
                                                  m_v_swapchain.image_views);
    result &= teardown_vulkan_renderer__allocator(m_v_vma_allocator);
    result &= teardown_vulkan_renderer__vulkan(m_v_instance,
#if _DEBUG
                                               m_v_debug_utils_messenger,
#endif
                                               m_v_surface,
                                               m_v_device);
    return result;
}

bool Monolithic_renderer::Impl::wait_for_renderer_idle()
{
    return static_cast<bool>(vkDeviceWaitIdle(m_v_device));
}

// Dear Imgui setup/run/teardown.
bool Monolithic_renderer::Impl::build_imgui()
{
    bool result{ true };

    VkDescriptorPoolSize pool_sizes[]{
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes)),
        .pPoolSizes = pool_sizes,
    };

    VkResult err{
        vkCreateDescriptorPool(m_v_device, &pool_info, nullptr, &m_v_imgui_pool) };
    if (err)
    {
        std::cerr << "ERROR: Create imgui descriptor pool failed." << std::endl;
        result = false;
    }

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    ImGui_ImplVulkan_InitInfo init_info{
        .Instance = m_v_instance,
        .PhysicalDevice = m_v_physical_device,
        .Device = m_v_device,
        .QueueFamily = m_v_graphics_queue_family_idx,
        .Queue = m_v_graphics_queue,
        .DescriptorPool = m_v_imgui_pool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &m_v_swapchain.image_format,
        },
    };

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    return result;
}

bool Monolithic_renderer::Impl::teardown_imgui()
{
    bool result{ true };

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m_v_device, m_v_imgui_pool, nullptr);

    return result;
}

// Setup jobs.
bool Monolithic_renderer::Impl::setup_initial_camera_props()
{
    camera::set_aspect_ratio(m_pimpl.m_window_width,
                             m_pimpl.m_window_height);
    camera::set_fov(glm_rad(70.0f));
    camera::set_near_far(0.1f, 1000.0f);
    camera::set_view(vec3{ 0.0f, 1.0f, -5.0f },
                     glm_rad(0.0f),
                     glm_rad(-30.0f));
    return true;
}

// Misc??????
bool Monolithic_renderer::Impl::write_material_param_sets_to_descriptor_sets()
{
    auto& material_param_sets_data{
        m_v_geometry_graphics_pass.material_param_sets_data };

    // Write buffers to descriptor sets from `build_vulkan_renderer__geometry_graphics_pass()`.
    VkDescriptorBufferInfo material_param_sets_buffer_info{
        .buffer = m_v_geo_passes_resource_buffer.material_param_set_buffer.buffer,
        .offset = 0,
        .range = m_v_geo_passes_resource_buffer.material_param_set_buffer_size,
    };
    VkWriteDescriptorSet material_param_sets_buffer_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = material_param_sets_data.descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &material_param_sets_buffer_info,
    };

    VkDescriptorBufferInfo material_params_buffer_info{
        .buffer = m_v_geo_passes_resource_buffer.material_param_index_buffer.buffer,
        .offset = 0,
        .range = m_v_geo_passes_resource_buffer.material_param_index_buffer_size,
    };
    VkWriteDescriptorSet material_params_buffer_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = material_param_sets_data.descriptor_set,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &material_params_buffer_info,
    };

    VkWriteDescriptorSet writes[]{ material_param_sets_buffer_write, material_params_buffer_write };
    vkUpdateDescriptorSets(m_v_device, 2, writes, 0, nullptr);

    return true;
}

bool Monolithic_renderer::Impl::write_bounding_spheres_to_descriptor_sets()
{
    auto& bounding_spheres_data{ m_v_geometry_graphics_pass.bounding_spheres_data };

    // Write buffers to descriptor sets from `build_vulkan_renderer__geometry_graphics_pass()`.
    VkDescriptorBufferInfo bounding_spheres_data_buffer_info{
        .buffer = m_v_geo_passes_resource_buffer.bounding_sphere_buffer.buffer,
        .offset = 0,
        .range = m_v_geo_passes_resource_buffer.bounding_sphere_buffer_size,
    };
    VkWriteDescriptorSet bounding_spheres_data_buffer_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = bounding_spheres_data.descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &bounding_spheres_data_buffer_info,
    };

    vkUpdateDescriptorSets(m_v_device, 1, &bounding_spheres_data_buffer_write, 0, nullptr);

    return true;
}

// Tick procedures.
bool Monolithic_renderer::Impl::update_window()
{
    glfwPollEvents();

    // @TODO

    return true;
}

bool Monolithic_renderer::Impl::update_and_upload_render_data()
{
    // Update.
    TIMING_REPORT_START(rebucket);
    std::vector<vk_buffer::GPU_geo_per_frame_buffer*> all_per_frame_buffers;
    all_per_frame_buffers.reserve(k_frame_overlap);
    for (size_t i = 0; i < k_frame_overlap; i++)
        all_per_frame_buffers.emplace_back(&m_frames[i].geo_per_frame_buffer);
    geo_instance::rebuild_bucketed_instance_list_array(all_per_frame_buffers);
    TIMING_REPORT_END_AND_PRINT(rebucket, "Rebucket Instance Data: ");

    // Upload.
    TIMING_REPORT_START(upload_per_frame);
    vk_buffer::upload_changed_per_frame_data(m_immediate_submit_support,
                                             m_v_device,
                                             m_v_graphics_queue,
                                             m_v_vma_allocator,
                                             get_current_frame().geo_per_frame_buffer);
    TIMING_REPORT_END_AND_PRINT(upload_per_frame, "Upload Changed Per-frame Data: ");

    return true;
}

void render__wait_until_current_frame_is_ready_to_render(VkDevice device,
                                                         VkSwapchainKHR swapchain,
                                                         const Frame_data& current_frame,
                                                         uint32_t& out_swapchain_image_idx)
{
    VkResult err;

    // Wait until GPU has finished rendering last frame.
    constexpr uint64_t k_10sec_as_ns{ 10000000000 };

    err = vkWaitForFences(device, 1, &current_frame.render_fence, true, k_10sec_as_ns);
    if (err)
    {
        std::cerr << "ERROR: wait for render fence timed out." << std::endl;
        assert(false);
    }

    err = vkResetFences(device, 1, &current_frame.render_fence);
    if (err)
    {
        std::cerr << "ERROR: reset render fence failed." << std::endl;
        assert(false);
    }

    // Request image from swapchain.
    err = vkAcquireNextImageKHR(device,
                                swapchain,
                                k_10sec_as_ns,
                                current_frame.swapchain_semaphore,
                                nullptr,
                                &out_swapchain_image_idx);
    if (err)
    {
        std::cerr << "ERROR: Acquire next swapchain image failed." << std::endl;
        assert(false);
    }
}

void render__begin_command_buffer(VkCommandBuffer cmd)
{

    VkResult err{ vkResetCommandBuffer(cmd, 0) };
    if (err)
    {
        std::cerr << "ERROR: Resetting command buffer failed." << std::endl;
        assert(false);
    }

    VkCommandBufferBeginInfo cmd_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    err = vkBeginCommandBuffer(cmd, &cmd_begin_info);
    if (err)
    {
        std::cerr << "ERROR: Begin command buffer failed." << std::endl;
        assert(false);
    }
}

void render__clear_background(VkCommandBuffer cmd,
                              const vk_image::Allocated_image& hdr_image)
{
    VkClearColorValue clear_value{
        .float32{ 0.0f, 0.5f, 0.1f, 1.0f }
    };
    VkImageSubresourceRange clear_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    vkCmdClearColorImage(cmd,
                         hdr_image.image,
                         VK_IMAGE_LAYOUT_GENERAL,
                         &clear_value,
                         1,
                         &clear_range);
}

void render__run_sample_pass(VkCommandBuffer cmd,
                             VkDescriptorSet descriptor_set,
                             VkPipeline pipeline,
                             VkPipelineLayout pipeline_layout,
                             VkExtent2D draw_extent)
{
    vkCmdBindPipeline(cmd,
                      VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_layout,
                            0,
                            1, &descriptor_set,
                            0, nullptr);
    vkCmdDispatch(cmd,
                  std::ceil(draw_extent.width / 16.0f),
                  std::ceil(draw_extent.height / 16.0f),
                  1);
}

void render__run_sunlight_shadow_cascades_pass()
{
}

void render__run_camera_view_geometry_culling(VkCommandBuffer cmd,
                                              VkDescriptorSet camera_desc_set,
                                              VkDescriptorSet bounding_sphere_desc_set,
                                              const GPU_geometry_culling_push_constants& params,
                                              VkPipeline geom_culling_pipeline,
                                              VkPipelineLayout geom_culling_pipeline_layout,
                                              VkBuffer visible_result_data_buffer,
                                              VkDeviceSize visible_result_data_buffer_size,
                                              uint32_t graphics_queue_family_idx)
{
    assert(params.num_instances > 0);  // @TODO: Add in just clearing and no drawing if nothing to render.

    // @NOTE: Visibility is calculated at a per-instance level here.
    vkCmdBindPipeline(cmd,
                      VK_PIPELINE_BIND_POINT_COMPUTE,
                      geom_culling_pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            geom_culling_pipeline_layout,
                            0,
                            1, &camera_desc_set,
                            0, nullptr);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            geom_culling_pipeline_layout,
                            1,
                            1, &bounding_sphere_desc_set,
                            0, nullptr);
    vkCmdPushConstants(cmd,
                       geom_culling_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(GPU_geometry_culling_push_constants),
                       &params);
    vkCmdDispatch(cmd,
                  std::ceil(params.num_instances / 128.0f),
                  1,
                  1);

    // Memory barrier for `geom_write_draw_cmds.comp`.
    VkBufferMemoryBarrier visibility_results_buffer_barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = graphics_queue_family_idx,
        .dstQueueFamilyIndex = graphics_queue_family_idx,
        .buffer = visible_result_data_buffer,
        .offset = 0,
        .size = visible_result_data_buffer_size,
    };
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0, nullptr,
                         1, &visibility_results_buffer_barrier,
                         0, nullptr);
}

void render__run_write_camera_view_geometry_draw_cmds(
    VkCommandBuffer cmd,
    const GPU_write_draw_cmds_push_constants& params,
    uint32_t num_primitive_render_groups,
    VkPipeline geom_write_draw_cmds_pipeline,
    VkPipelineLayout geom_write_draw_cmds_pipeline_layout,
    VkBuffer indirect_draw_cmds_buffer,
    VkBuffer indirect_draw_cmd_counts_buffer,
    uint32_t graphics_queue_family_idx)
{
    // @TODO: Figure out if you wanna move the write draw cmds step to
    //        its own thing so that the resulting buffer can be reused
    //        for other rendering steps.

    assert(num_primitive_render_groups > 0);  // @TODO: Add in just clearing and no drawing if nothing to render.

    // Reset count buffer to 0.
    vkCmdFillBuffer(cmd,
                    indirect_draw_cmd_counts_buffer,
                    0,
                    sizeof(uint32_t) * num_primitive_render_groups,
                    0);

    // Write draw commands, pulling from instance visibility buffer.
    vkCmdBindPipeline(cmd,
                      VK_PIPELINE_BIND_POINT_COMPUTE,
                      geom_write_draw_cmds_pipeline);
    vkCmdPushConstants(cmd,
                       geom_write_draw_cmds_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(GPU_write_draw_cmds_push_constants),
                       &params);
    vkCmdDispatch(cmd,
                  std::ceil(params.num_primitives / 128.0f),
                  1,
                  1);

    // Memory buffer barrier to make sure indirect commands are written before vertex shaders run.
    VkBufferMemoryBarrier buffer_barriers[]{
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            .srcQueueFamilyIndex = graphics_queue_family_idx,
            .dstQueueFamilyIndex = graphics_queue_family_idx,
            .buffer = indirect_draw_cmds_buffer,
            .offset = 0,
            .size = sizeof(VkDrawIndexedIndirectCommand) * params.num_primitives,
        },
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            .srcQueueFamilyIndex = graphics_queue_family_idx,
            .dstQueueFamilyIndex = graphics_queue_family_idx,
            .buffer = indirect_draw_cmd_counts_buffer,
            .offset = 0,
            .size = sizeof(uint32_t) * num_primitive_render_groups,
            // @NOTE: ^^ Instead of instances or primitives, these are primitive render
            //   groups, grouped by shader idx.
        }
    };
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                         0,
                         0, nullptr,
                         2, buffer_barriers,
                         0, nullptr);
}

void render__run_opaque_geometry_pass(VkCommandBuffer cmd,
                                      VkImageView image_view,
                                      VkExtent2D draw_extent,
                                      VkDescriptorSet main_view_camera_descriptor_set,
                                      VkDeviceAddress instance_data_buffer_address,
                                      VkBuffer indirect_draw_buffer,
                                      VkBuffer indirect_draw_count_buffer)
{
    VkRenderingAttachmentInfo color_attachment{
        vk_util::attachment_info(image_view,
                                 nullptr,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) };
    VkRenderingInfo render_info{
        vk_util::rendering_info(draw_extent, &color_attachment, nullptr) };
    
    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = static_cast<float_t>(draw_extent.width),
        .height = static_cast<float_t>(draw_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor{
        .offset{ .x = 0, .y = 0 },
        .extent{ draw_extent },
    };

    auto grouped_primitives{
        geo_instance::get_pipeline_grouped_primitives(geo_instance::Geo_render_pass::OPAQUE)
    };

    vkCmdBeginRendering(cmd, &render_info);

    // Set initial values.
    gltf_loader::bind_combined_mesh(cmd);
    uint32_t prev_pipeline_cidx{ (uint32_t)-1 };

    // Draw all opaque primitives.
    enum : uint8_t {
        DRAW_Z_PREPASS = 0,
        DRAW_MATERIAL_BASED_PASS,
        NUM_PASSES
    };
    for (uint8_t pass = 0; pass < NUM_PASSES; pass++)
    {
        // Z prepass and then Material-based draw.
        VkDeviceSize drawn_primitive_count{ 0 };
        VkDeviceSize render_primitive_group_idx{ 0 };
        for (auto& it : grouped_primitives)
        {
            auto pipeline_idx{ it.first };
            auto& primitive_ptr_list{ it.second };

            const material_bank::GPU_pipeline* pipeline{ nullptr };
            switch (pass)
            {
            case DRAW_Z_PREPASS:
                pipeline =
                    material_bank::get_pipeline(pipeline_idx)
                        .z_prepass_pipeline;
                break;

            case DRAW_MATERIAL_BASED_PASS:
                pipeline =
                    &material_bank::get_pipeline(pipeline_idx);
                break;

            default:
                assert(false);
                break;
            }

            // Skip drawing if no pipeline was selected
            // (could be ignored z prepass texture or something).
            assert(pipeline != nullptr);

            // Bind material pipeline.
            if (pipeline->calculated.pipeline_creation_idx != prev_pipeline_cidx)
            {
                pipeline->bind_pipeline(cmd,
                                        viewport,
                                        scissor,
                                        &main_view_camera_descriptor_set,
                                        nullptr,
                                        instance_data_buffer_address);
                prev_pipeline_cidx = pipeline->calculated.pipeline_creation_idx;
            }

            // 描け～！！
            vkCmdDrawIndexedIndirectCount(cmd,
                                          indirect_draw_buffer,
                                          drawn_primitive_count,
                                          indirect_draw_count_buffer,
                                          render_primitive_group_idx,
                                          static_cast<uint32_t>(primitive_ptr_list.size()),
                                          sizeof(VkDrawIndexedIndirectCommand));
            drawn_primitive_count += primitive_ptr_list.size();
            render_primitive_group_idx++;
        }
    }

    vkCmdEndRendering(cmd);
}

void render__run_sample_geometry_pass(VkCommandBuffer cmd,
                                      VkImageView image_view,
                                      VkExtent2D draw_extent,
                                      VkPipeline pipeline)
{
    VkRenderingAttachmentInfo color_attachment{
        vk_util::attachment_info(image_view,
                                 nullptr,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) };
    VkRenderingInfo render_info{
        vk_util::rendering_info(draw_extent, &color_attachment, nullptr) };
    
    vkCmdBeginRendering(cmd, &render_info);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = static_cast<float_t>(draw_extent.width),
        .height = static_cast<float_t>(draw_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{
        .offset{ .x = 0, .y = 0 },
        .extent{ draw_extent },
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

void render__blit_HDR_image_to_swapchain(VkCommandBuffer cmd,
                                         VkImage hdr_image,
                                         VkExtent2D hdr_image_extent,
                                         VkImage swapchain_image,
                                         VkExtent2D swapchain_extent)
{
    // Transition swapchain image for presentation.
    vk_util::transition_image(cmd,
                              hdr_image,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vk_util::transition_image(cmd,
                              swapchain_image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_util::blit_image_to_image(cmd,
                                 hdr_image,
                                 swapchain_image,
                                 hdr_image_extent,
                                 swapchain_extent,
                                 VK_FILTER_NEAREST);
}

void render__prep_swapchain_image_for_draw_imgui(VkCommandBuffer cmd,
                                                 VkImage swapchain_image)
{
    vk_util::transition_image(cmd,
                              swapchain_image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void render__draw_imgui_draw_data(VkCommandBuffer cmd,
                                  VkExtent2D render_extent,
                                  VkImageView target_image_view)
{
    VkRenderingAttachmentInfo color_attachment{
        vk_util::attachment_info(target_image_view,
                                 nullptr,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    };
    VkRenderingInfo render_info{
        vk_util::rendering_info(render_extent,
                                &color_attachment,
                                nullptr)
    };

    vkCmdBeginRendering(cmd, &render_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void render__prep_swapchain_image_for_presentation(VkCommandBuffer cmd,
                                                   VkImage swapchain_image,
                                                   VkImageLayout old_layout)
{
    vk_util::transition_image(cmd,
                              swapchain_image,
                              old_layout,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void render__end_command_buffer(VkCommandBuffer cmd)
{
    // End command buffer.
    VkResult err{ vkEndCommandBuffer(cmd) };
    if (err)
    {
        std::cerr << "ERROR: End command buffer failed." << std::endl;
        assert(false);
    }
}

void render__submit_commands_to_queue(VkCommandBuffer cmd,
                                      VkQueue graphics_queue,
                                      Frame_data& current_frame)
{
    // Prep submission to the queue.
    VkCommandBufferSubmitInfo cmd_info{ vk_util::command_buffer_submit_info(cmd) };
    VkSemaphoreSubmitInfo wait_info{
        vk_util::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                       current_frame.swapchain_semaphore)
    };
    VkSemaphoreSubmitInfo signal_info{
        vk_util::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                       current_frame.render_semaphore)
    };
    VkSubmitInfo2 submit_info{ vk_util::submit_info(&cmd_info, &signal_info, &wait_info) };

    // Submit command buffer to queue and execute it.
    VkResult err{
        vkQueueSubmit2(graphics_queue, 1, &submit_info, current_frame.render_fence)
    };
    if (err)
    {
        std::cerr << "ERROR: Submit command buffer failed." << std::endl;
        assert(false);
    }
}

void render__present_image(VkSwapchainKHR swapchain,
                           VkQueue graphics_queue,
                           const uint32_t& swapchain_image_idx,
                           const Frame_data& current_frame,
                           std::atomic_bool& out_is_swapchain_out_of_date)
{
    // Present image.
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_frame.render_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapchain_image_idx,
    };
    VkResult err{
        vkQueuePresentKHR(graphics_queue, &present_info) };
    if (err)
    {
        // Check if swapchain is out of date when presenting, due to window
        // being minimized or hidden.
        // @NOTE: only a check needed for desktop apps afaik.
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            std::cout << "NOTE: window minimized. Pausing renderer." << std::endl;
            out_is_swapchain_out_of_date = true;
        }
        else
        {
            std::cerr << "ERROR: Queue present KHR failed." << std::endl;
            assert(false);
        }
    }
}

bool Monolithic_renderer::Impl::render()
{
    // Recreate swapchain.
    if (m_request_swapchain_creation)
    {
        std::cerr << "TODO: This would be where you recreate the swapchain. But that functionality isn't created yet. So heh haha" << std::endl;
        // m_is_swapchain_out_of_date = false;  // @TODO: uncomment when finish the swapchain recreation.
    }

    // Do not render unless window is shown.
    if (m_is_swapchain_out_of_date)
        return true;

    // Ready command buffer and swapchain for this frame.
    auto& current_frame{ get_current_frame() };
    uint32_t swapchain_image_idx;
    render__wait_until_current_frame_is_ready_to_render(m_v_device,
                                                        m_v_swapchain.swapchain,
                                                        current_frame,
                                                        swapchain_image_idx);
    auto& v_current_swapchain_image{ m_v_swapchain.images[swapchain_image_idx] };
    auto& v_current_swapchain_image_view{ m_v_swapchain.image_views[swapchain_image_idx] };

    // Upload camera information.
    {
        mat4 projection;
        std::vector<mat4s> shadow_cascades;
        camera::GPU_camera camera_data;
        camera::fetch_matrices(projection,
                               camera_data.view,
                               camera_data.projection_view,
                               shadow_cascades);

        void* data;
        vmaMapMemory(m_v_vma_allocator, current_frame.camera_buffer.allocation, &data);
        memcpy(data, &camera_data, sizeof(camera::GPU_camera));
        vmaUnmapMemory(m_v_vma_allocator, current_frame.camera_buffer.allocation);
        
        assert(false);  // @TODO: GET A MOVABLE CAMERA GOING!!!
    }


    // Render Imgui.
    const bool display_imgui{ m_imgui_enabled && m_imgui_visible };
    if (display_imgui)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        render__imgui();
        ImGui::Render();
    }

    // Write commands.
    VkCommandBuffer cmd{ current_frame.main_command_buffer };
    render__begin_command_buffer(cmd);
    {
        // General rendering.
        vk_util::transition_image(cmd,
                                  m_v_HDR_draw_image.image.image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_GENERAL);
        render__clear_background(cmd, m_v_HDR_draw_image.image);
        // render__run_sample_pass(cmd,
        //                         m_v_sample_pass.descriptor_set,
        //                         m_v_sample_pass.pipeline,
        //                         m_v_sample_pass.pipeline_layout,
        //                         m_v_HDR_draw_image.extent);
        
        // Geometry rendering.
        vk_util::transition_image(cmd,
                                  m_v_HDR_draw_image.image.image,
                                  VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        render__run_sunlight_shadow_cascades_pass();

        auto& current_per_frame_data{ get_current_geom_per_frame_data() };

        constexpr bool k_culling_enabled{ true };

        const auto& current_geo_frame{ current_frame.geo_per_frame_buffer };

        GPU_geometry_culling_push_constants geom_culling_pc{
            .z_near = 0.0f,  // @TODO: Calculate frustum!
            .z_far = 0.0f,
            .frustum_x_x = 0.0f,
            .frustum_x_z = 0.0f,
            .frustum_y_y = 0.0f,
            .frustum_y_z = 0.0f,
            .culling_enabled = (k_culling_enabled ? 1 : 0),
            .num_instances = geo_instance::get_unique_instances_count(),
            .instance_buffer_address = current_geo_frame.instance_data_buffer_address,
            .visible_result_buffer_address = current_geo_frame.visible_result_buffer_address,
        };

        render__run_camera_view_geometry_culling(cmd,
            current_per_frame_data.camera_data.descriptor_set,
            m_v_geometry_graphics_pass.bounding_spheres_data.descriptor_set,
            geom_culling_pc,
            m_v_geometry_graphics_pass.culling_pipeline,
            m_v_geometry_graphics_pass.culling_pipeline_layout,
            current_geo_frame.visible_result_buffer.buffer,
            sizeof(uint32_t) * current_geo_frame.num_visible_result_elems,
            m_v_graphics_queue_family_idx);

        GPU_write_draw_cmds_push_constants write_draw_cmds_pc{
            .num_primitives =  // @TODO: @CHECK: I wonder if this should be for all primitives instead of just opaque ones.
                geo_instance::get_number_primitives(geo_instance::Geo_render_pass::OPAQUE),
            .visible_result_buffer_address = current_geo_frame.visible_result_buffer_address,
            .base_indices_buffer_address = current_geo_frame.primitive_group_base_index_buffer_address,
            .count_buffer_indices_buffer_address = current_geo_frame.count_buffer_index_buffer_address,
            .draw_commands_input_buffer_address = current_geo_frame.indirect_command_buffer_address,
            .draw_commands_output_buffer_address = current_geo_frame.culled_indirect_command_buffer_address,
            .draw_command_counts_buffer_address = current_geo_frame.indirect_counts_buffer_address,
        };

        render__run_write_camera_view_geometry_draw_cmds(cmd,
                                                         write_draw_cmds_pc,
                                                         geo_instance::get_num_primitive_render_groups(
                                                            geo_instance::Geo_render_pass::OPAQUE),
                                                         m_v_geometry_graphics_pass.write_draw_cmds_pipeline,
                                                         m_v_geometry_graphics_pass.write_draw_cmds_pipeline_layout,
                                                         current_geo_frame.culled_indirect_command_buffer.buffer,
                                                         current_geo_frame.indirect_counts_buffer.buffer,
                                                         m_v_graphics_queue_family_idx);
        render__run_opaque_geometry_pass(cmd,
                                         m_v_HDR_draw_image.image.image_view,
                                         m_v_HDR_draw_image.extent,
                                         current_per_frame_data.camera_data.descriptor_set,
                                         current_geo_frame.instance_data_buffer_address,
                                         current_geo_frame.culled_indirect_command_buffer.buffer,
                                         current_geo_frame.indirect_counts_buffer.buffer);

        // render__run_sample_geometry_pass(cmd,
        //                                  m_v_HDR_draw_image.image.image_view,
        //                                  m_v_HDR_draw_image.extent,
        //                                  m_v_sample_graphics_pass.pipeline);

        // Swapchain.
        render__blit_HDR_image_to_swapchain(cmd,
                                            m_v_HDR_draw_image.image.image,
                                            m_v_HDR_draw_image.extent,
                                            v_current_swapchain_image,
                                            m_v_swapchain.extent);
        if (display_imgui)
        {
            render__prep_swapchain_image_for_draw_imgui(cmd,
                                                        v_current_swapchain_image);
            render__draw_imgui_draw_data(cmd,
                                         m_v_swapchain.extent,
                                         v_current_swapchain_image_view);
            render__prep_swapchain_image_for_presentation(cmd,
                                                          v_current_swapchain_image,
                                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
        else
        {
            render__prep_swapchain_image_for_presentation(cmd,
                                                          v_current_swapchain_image,
                                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
    }
    render__end_command_buffer(cmd);

    // Finish frame.
    render__submit_commands_to_queue(cmd,
                                     m_v_graphics_queue,
                                     current_frame);
    render__present_image(m_v_swapchain.swapchain,
                          m_v_graphics_queue,
                          swapchain_image_idx,
                          current_frame,
                          m_is_swapchain_out_of_date);

    // End frame.
    m_frame_number++;

    // Check if window should close.
    if (is_requesting_close() || m_shutdown_flag)
    {
        //glfwHideWindow(m_window);
        m_stage = Stage::TEARDOWN;
    }

    return true;
}

bool Monolithic_renderer::Impl::render__imgui()
{
    ImGui::ShowDemoWindow();
    return true;
}

#endif  // _WIN64
