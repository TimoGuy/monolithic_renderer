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
#include "multithreaded_job_system_public.h"
#include "renderer_win64_vulkan_util.h"


// For extern symbol.
std::atomic<Monolithic_renderer*> s_mr_singleton_ptr{ nullptr };

// Jobs.
int32_t Monolithic_renderer::Impl::Build_job::execute()
{
    bool success{ true };
    success &= m_pimpl.build_window();
    success &= m_pimpl.build_vulkan_renderer();
    return success ? 0 : 1;
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
    success &= m_pimpl.render();
    return success ? 0 : 1;
}

int32_t Monolithic_renderer::Impl::Teardown_job::execute()
{
    bool success{ true };
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
bool build_vulkan_renderer__build_vulkan(GLFWwindow* window,
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
        // For MIN/MAX sampler when creating mip chains.
        .samplerFilterMinmax = VK_TRUE,
        // @TODO: @CHECK: for new vkguide.
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
                .multiDrawIndirect = VK_TRUE,         // So that vkCmdDrawIndexedIndirect() can be called with a >1 drawCount.
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
    std::cout << "[Chosen Physical Device Properties]" << std::endl;
    std::cout << "API_VERSION                          " << VK_API_VERSION_MAJOR(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_MINOR(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_PATCH(out_physical_device_properties.apiVersion) << "." << VK_API_VERSION_VARIANT(out_physical_device_properties.apiVersion) << std::endl;
    std::cout << "DRIVER_VERSION                       " << out_physical_device_properties.driverVersion << std::endl;
    std::cout << "VENDOR_ID                            " << out_physical_device_properties.vendorID << std::endl;
    std::cout << "DEVICE_ID                            " << out_physical_device_properties.deviceID << std::endl;
    std::cout << "DEVICE_TYPE                          " << out_physical_device_properties.deviceType << std::endl;
    std::cout << "DEVICE_NAME                          " << out_physical_device_properties.deviceName << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_1D               " << out_physical_device_properties.limits.maxImageDimension1D << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_2D               " << out_physical_device_properties.limits.maxImageDimension2D << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_3D               " << out_physical_device_properties.limits.maxImageDimension3D << std::endl;
    std::cout << "MAX_IMAGE_DIMENSION_CUBE             " << out_physical_device_properties.limits.maxImageDimensionCube << std::endl;
    std::cout << "MAX_IMAGE_ARRAY_LAYERS               " << out_physical_device_properties.limits.maxImageArrayLayers << std::endl;
    std::cout << "MAX_SAMPLER_ANISOTROPY               " << out_physical_device_properties.limits.maxSamplerAnisotropy << std::endl;
    std::cout << "MAX_BOUND_DESCRIPTOR_SETS            " << out_physical_device_properties.limits.maxBoundDescriptorSets << std::endl;
    std::cout << "MINIMUM_BUFFER_ALIGNMENT             " << out_physical_device_properties.limits.minUniformBufferOffsetAlignment << std::endl;
    std::cout << "MAX_COLOR_ATTACHMENTS                " << out_physical_device_properties.limits.maxColorAttachments << std::endl;
    std::cout << "MAX_DRAW_INDIRECT_COUNT              " << out_physical_device_properties.limits.maxDrawIndirectCount << std::endl;
    std::cout << "MAX_DESCRIPTOR_SET_SAMPLED_IMAGES    " << out_physical_device_properties.limits.maxDescriptorSetSampledImages << std::endl;
    std::cout << "MAX_DESCRIPTOR_SET_SAMPLERS          " << out_physical_device_properties.limits.maxDescriptorSetSamplers << std::endl;
    std::cout << "MAX_SAMPLER_ALLOCATION_COUNT         " << out_physical_device_properties.limits.maxSamplerAllocationCount << std::endl;
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

bool build_vulkan_renderer__build_allocator(VkInstance instance,
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

bool build_vulkan_renderer__build_swapchain(VkSurfaceKHR surface,
                                            VkPhysicalDevice physical_device,
                                            VkDevice device,
                                            int32_t window_width,
                                            int32_t window_height,
                                            VkSwapchainKHR& out_swapchain,
                                            std::vector<VkImage>& out_swapchain_images,
                                            std::vector<VkImageView>& out_swapchain_image_views,
                                            VkFormat& out_swapchain_image_format)
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

bool build_vulkan_renderer__build_cmd_structures(uint32_t m_v_graphics_queue_family_idx,
                                                 VkDevice m_v_device,
                                                 FrameData out_frames[])
{
    VkResult err;

    VkCommandPoolCreateInfo cmd_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_v_graphics_queue_family_idx
    };

    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        err = vkCreateCommandPool(m_v_device, &cmd_pool_info, nullptr, &out_frames[i].command_pool);
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

        err = vkAllocateCommandBuffers(m_v_device, &cmd_alloc_info, &out_frames[i].main_command_buffer);
        if (err)
        {
            std::cerr << "ERROR: Vulkan command pool allocation failed for frame #" << i << std::endl;
            return false;
        }
    }

    return true;
}

bool build_vulkan_renderer__build_sync_structures(VkDevice device, FrameData out_frames[])
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

bool Monolithic_renderer::Impl::build_vulkan_renderer()
{
    bool result{ true };
    vkb::Device vkb_device;
    result &= build_vulkan_renderer__build_vulkan(m_window,
                                                  m_v_instance,
#if _DEBUG
                                                  m_v_debug_utils_messenger,
#endif
                                                  m_v_surface,
                                                  m_v_physical_device,
                                                  m_v_physical_device_properties,
                                                  m_v_device,
                                                  vkb_device);
    result &= build_vulkan_renderer__build_allocator(m_v_instance,
                                                     m_v_physical_device,
                                                     m_v_device,
                                                     m_v_vma_allocator);
    result &= build_vulkan_renderer__build_swapchain(m_v_surface,
                                                     m_v_physical_device,
                                                     m_v_device,
                                                     m_window_width,
                                                     m_window_height,
                                                     m_v_swapchain,
                                                     m_v_swapchain_images,
                                                     m_v_swapchain_image_views,
                                                     m_v_swapchain_image_format);
    result &= build_vulkan_renderer__retrieve_queues(vkb_device,
                                                     m_v_graphics_queue,
                                                     m_v_graphics_queue_family_idx);
    result &= build_vulkan_renderer__build_cmd_structures(m_v_graphics_queue_family_idx,
                                                          m_v_device,
                                                          m_frames);
    result &= build_vulkan_renderer__build_sync_structures(m_v_device,
                                                           m_frames);
    return result;
}

bool teardown_vulkan_renderer__teardown_vulkan(VkInstance instance,
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

bool teardown_vulkan_renderer__teardown_allocator(VmaAllocator allocator)
{
    vmaDestroyAllocator(allocator);
    return true;
}

bool teardown_vulkan_renderer__teardown_swapchain(VkDevice device, VkSwapchainKHR swapchain)
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    return true;
}

bool teardown_vulkan_renderer__teardown_cmd_structures(VkDevice device, FrameData frames[])
{
    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        vkDestroyCommandPool(device, frames[i].command_pool, nullptr);
    }
    return true;
}

bool teardown_vulkan_renderer__teardown_sync_structures(VkDevice device, FrameData frames[])
{
    for (uint32_t i = 0; i < k_frame_overlap; i++)
    {
        vkDestroyFence(device, frames[i].render_fence, nullptr);
        vkDestroySemaphore(device, frames[i].render_semaphore, nullptr);
        vkDestroySemaphore(device, frames[i].swapchain_semaphore, nullptr);
    }
    return true;
}

bool Monolithic_renderer::Impl::teardown_vulkan_renderer()
{
    bool result{ true };
    result &= static_cast<bool>(vkDeviceWaitIdle(m_v_device));
    result &= teardown_vulkan_renderer__teardown_sync_structures(m_v_device, m_frames);
    result &= teardown_vulkan_renderer__teardown_cmd_structures(m_v_device, m_frames);
    result &= teardown_vulkan_renderer__teardown_swapchain(m_v_device, m_v_swapchain);
    result &= teardown_vulkan_renderer__teardown_allocator(m_v_vma_allocator);
    result &= teardown_vulkan_renderer__teardown_vulkan(m_v_instance,
#if _DEBUG
                                                        m_v_debug_utils_messenger,
#endif
                                                        m_v_surface,
                                                        m_v_device);
    return result;
}

// Tick procedures.
bool Monolithic_renderer::Impl::update_window()
{
    glfwPollEvents();

    // @TODO

    return true;
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

    VkResult err;

    // Wait until GPU has finished rendering last frame.
    constexpr uint64_t k_10sec_as_ns{ 10000000000 };
    auto& current_frame{ get_current_frame() };

    err = vkWaitForFences(m_v_device, 1, &current_frame.render_fence, true, k_10sec_as_ns);
    if (err)
    {
        std::cerr << "ERROR: wait for render fence timed out." << std::endl;
        assert(false);
    }

    err = vkResetFences(m_v_device, 1, &current_frame.render_fence);
    if (err)
    {
        std::cerr << "ERROR: reset render fence failed." << std::endl;
        assert(false);
    }

    // Request image from swapchain.
    uint32_t swapchain_image_idx;
    err = vkAcquireNextImageKHR(m_v_device,
                                m_v_swapchain,
                                k_10sec_as_ns,
                                current_frame.swapchain_semaphore,
                                nullptr,
                                &swapchain_image_idx);
    if (err)
    {
        std::cerr << "ERROR: Acquire next swapchain image failed." << std::endl;
        assert(false);
    }
    auto& v_current_swapchain_image{ m_v_swapchain_images[swapchain_image_idx] };

    // Begin command buffer.
    VkCommandBuffer cmd{ current_frame.main_command_buffer };

    err = vkResetCommandBuffer(cmd, 0);
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

    // Transition swapchain image for modification.
    vk_util::transition_image(cmd,
                              v_current_swapchain_image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);
    
    // Clear image.
    VkClearColorValue clear_value{
        //.float32{ 0.0f, 0.0f, std::abs(std::sin(m_frame_number / 120.0f)), 1.0f }
        .float32{ 0.0f, 0.0f, std::abs(0.0f), 1.0f }
    };
    VkImageSubresourceRange clear_range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    vkCmdClearColorImage(cmd,
                         v_current_swapchain_image,
                         VK_IMAGE_LAYOUT_GENERAL,
                         &clear_value,
                         1,
                         &clear_range);

    // Transition swapchain image for presentation.
	vk_util::transition_image(cmd,
                              v_current_swapchain_image,
                              VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// End command buffer.
	err = vkEndCommandBuffer(cmd);
    if (err)
    {
        std::cerr << "ERROR: End command buffer failed." << std::endl;
        assert(false);
    }

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
    err = vkQueueSubmit2(m_v_graphics_queue, 1, &submit_info, current_frame.render_fence);
    if (err)
    {
        std::cerr << "ERROR: Submit command buffer failed." << std::endl;
        assert(false);
    }

    // Present image.
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_frame.render_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_v_swapchain,
        .pImageIndices = &swapchain_image_idx,
    };
    err = vkQueuePresentKHR(m_v_graphics_queue, &present_info);
    if (err)
    {
        // Check if swapchain is out of date when presenting, due to window
        // being minimized or hidden.
        // @NOTE: only a check needed for desktop apps afaik.
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            std::cout << "NOTE: window minimized. Pausing renderer." << std::endl;
            m_is_swapchain_out_of_date = true;
        }
        else
        {
            std::cerr << "ERROR: Queue present KHR failed." << std::endl;
            assert(false);
        }
    }

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

#endif  // _WIN64
