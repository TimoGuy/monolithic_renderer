#include "renderer.h"


// Platform-specific implementations.
#if _WIN64  // Windows implementation.

// @NOTE: Vulkan and GLFW have to be included in this order.
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <cinttypes>
#include <cstring>
#include <iostream>
#include "VkBootstrap.h"


constexpr int32_t k_window_width{ 1920 };
constexpr int32_t k_window_height{ 1080 };

static void key_callback(GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods)
{

}

class Monolithic_renderer::Impl
{
public:
    Impl(const std::string& name)
        : m_name(name)
    {
    }

    bool build()
    {
        bool success{ true };
        success &= build_window();
        success &= build_vulkan_renderer();
        return success;
    }

    bool teardown()
    {
        bool success{ true };
        success &= teardown_vulkan_renderer();
        success &= teardown_window();
        return success;
    }

    void tick()
    {
        update_window();
        render();
    }

    bool get_requesting_close()
    {
        return glfwWindowShouldClose(m_window);
    }

private:
    // Win64 window setup/teardown.
    bool build_window()
    {
        // Setup GLFW window.
        if (!glfwInit())
        {
            std::cerr << "ERROR: GLFW Init failed." << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // @TODO: Have change sizing functions (but not controllable from directly controlling the window).

        m_window = glfwCreateWindow(k_window_width, k_window_height, m_name.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            std::cerr << "ERROR: Window creation failed." << std::endl;
            glfwTerminate();
            return false;
        }

        glfwSetKeyCallback(m_window, key_callback);

        return true;
    }

    bool teardown_window()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return true;
    }

    // Vulkan renderer setup/teardown.
    bool build_vulkan_renderer()
    {
        VkResult err;

        // Build vulkan instance (targeting Vulkan 1.2).
        vkb::InstanceBuilder builder;
        vkb::Result<vkb::Instance> instance_build_result{
            builder
                .set_app_name("Hawsoo Monolithic Renderer")
                .require_api_version(1, 2, 0)
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
        m_v_instance = instance.instance;
#if _DEBUG
        m_v_debug_utils_messenger = instance.debug_messenger;
#endif

        // Build presentation surface.
        err = glfwCreateWindowSurface(m_v_instance, m_window, nullptr, &m_v_surface);
        if (err)
        {
            std::cerr << "ERROR: Vulkan surface creation failed." << std::endl;
            return false;
        }

        // Select physical device.
        vkb::PhysicalDeviceSelector selector{ instance };
        vkb::PhysicalDevice physical_device{
            selector
                .set_minimum_version(1, 2)
                .set_surface(m_v_surface)
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
        m_v_physical_device = physical_device.physical_device;
        m_v_physical_device_properties = physical_device.properties;

        // Print phsyical device properties.
        std::cout << "[Chosen Physical Device Properties]" << std::endl;
        std::cout << "API_VERSION                          " << VK_API_VERSION_MAJOR(m_v_physical_device_properties.apiVersion) << "." << VK_API_VERSION_MINOR(m_v_physical_device_properties.apiVersion) << "." << VK_API_VERSION_PATCH(m_v_physical_device_properties.apiVersion) << "." << VK_API_VERSION_VARIANT(m_v_physical_device_properties.apiVersion) << std::endl;
        std::cout << "DRIVER_VERSION                       " << m_v_physical_device_properties.driverVersion << std::endl;
        std::cout << "VENDOR_ID                            " << m_v_physical_device_properties.vendorID << std::endl;
        std::cout << "DEVICE_ID                            " << m_v_physical_device_properties.deviceID << std::endl;
        std::cout << "DEVICE_TYPE                          " << m_v_physical_device_properties.deviceType << std::endl;
        std::cout << "DEVICE_NAME                          " << m_v_physical_device_properties.deviceName << std::endl;
        std::cout << "MAX_IMAGE_DIMENSION_1D               " << m_v_physical_device_properties.limits.maxImageDimension1D << std::endl;
        std::cout << "MAX_IMAGE_DIMENSION_2D               " << m_v_physical_device_properties.limits.maxImageDimension2D << std::endl;
        std::cout << "MAX_IMAGE_DIMENSION_3D               " << m_v_physical_device_properties.limits.maxImageDimension3D << std::endl;
        std::cout << "MAX_IMAGE_DIMENSION_CUBE             " << m_v_physical_device_properties.limits.maxImageDimensionCube << std::endl;
        std::cout << "MAX_IMAGE_ARRAY_LAYERS               " << m_v_physical_device_properties.limits.maxImageArrayLayers << std::endl;
        std::cout << "MAX_SAMPLER_ANISOTROPY               " << m_v_physical_device_properties.limits.maxSamplerAnisotropy << std::endl;
        std::cout << "MAX_BOUND_DESCRIPTOR_SETS            " << m_v_physical_device_properties.limits.maxBoundDescriptorSets << std::endl;
        std::cout << "MINIMUM_BUFFER_ALIGNMENT             " << m_v_physical_device_properties.limits.minUniformBufferOffsetAlignment << std::endl;
        std::cout << "MAX_COLOR_ATTACHMENTS                " << m_v_physical_device_properties.limits.maxColorAttachments << std::endl;
        std::cout << "MAX_DRAW_INDIRECT_COUNT              " << m_v_physical_device_properties.limits.maxDrawIndirectCount << std::endl;
        std::cout << "MAX_DESCRIPTOR_SET_SAMPLED_IMAGES    " << m_v_physical_device_properties.limits.maxDescriptorSetSampledImages << std::endl;
        std::cout << "MAX_DESCRIPTOR_SET_SAMPLERS          " << m_v_physical_device_properties.limits.maxDescriptorSetSamplers << std::endl;
        std::cout << "MAX_SAMPLER_ALLOCATION_COUNT         " << m_v_physical_device_properties.limits.maxSamplerAllocationCount << std::endl;
        std::cout << std::endl;

        // Build Vulkan device.
        vkb::DeviceBuilder device_builder{ physical_device };
        VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
            .pNext = nullptr,
            .shaderDrawParameters = VK_TRUE,
        };
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
        };
        vkb::Device device{
            device_builder
                .add_pNext(&shader_draw_parameters_features)
                .add_pNext(&vulkan12_features)
                .build()
                .value()
        };
        m_v_device = device.device;

        // Retrieve graphics queue.
        m_v_graphics_queue = device.get_queue(vkb::QueueType::graphics).value();
        m_v_graphics_queue_family_idx = device.get_queue_index(vkb::QueueType::graphics).value();

        // Initialize VMA.
        VmaAllocatorCreateInfo vma_allocator_info{
            .physicalDevice = m_v_physical_device,
            .device = m_v_device,
            .instance = m_v_instance,
        };
        vmaCreateAllocator(&vma_allocator_info, &m_v_vma_allocator);

        // Build swapchain.
        // @TODO: Make swapchain rebuilding a thing.
        vkb::SwapchainBuilder swapchain_builder{ m_v_physical_device, m_v_device, m_v_surface };
        vkb::Swapchain swapchain{
            swapchain_builder
                .use_default_format_selection()
                .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)  // Mailbox (G-/Freesync compatible).
                .set_desired_extent(k_window_width, k_window_height)
                .build()
                .value()
        };
        m_v_swapchain = swapchain.swapchain;
        m_v_swapchain_images = swapchain.get_images().value();
        m_v_swapchain_image_views = swapchain.get_image_views().value();
        m_v_swapchain_image_format = swapchain.image_format;

        return true;
    }

    bool teardown_vulkan_renderer()
    {
        vkDestroySwapchainKHR(m_v_device, m_v_swapchain, nullptr);
        vmaDestroyAllocator(m_v_vma_allocator);
        vkDestroyDevice(m_v_device, nullptr);
        // @NOTE: Vulkan surface created from GLFW must be destroyed with vk func.
        vkDestroySurfaceKHR(m_v_instance, m_v_surface, nullptr);
#if _DEBUG
		vkb::destroy_debug_utils_messenger(m_v_instance, m_v_debug_utils_messenger);
#endif
        vkDestroyInstance(m_v_instance, nullptr);

        return true;
    }

    // Tick procedures.
    void update_window()
    {
        glfwPollEvents();

        // @TODO
    }

    void render()
    {
        // @TODO
    }

    std::string m_name;
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
};

#else  // Unsupported implementation.
#error "Unsupported OS"
#endif  // _WIN64, etc.


// Implementation wrapper.
Monolithic_renderer::Monolithic_renderer(const std::string& name)
    : m_pimpl(std::make_unique<Impl>(name))
{
}

// @NOTE: For smart pointer pimpl, must define destructor
//        in the .cpp file, even if it's `default`.
Monolithic_renderer::~Monolithic_renderer() = default;

bool Monolithic_renderer::build()
{
    return m_pimpl->build();
}

bool Monolithic_renderer::teardown()
{
    return m_pimpl->teardown();
}

void Monolithic_renderer::tick()
{
    m_pimpl->tick();
}

bool Monolithic_renderer::get_requesting_close()
{
    return m_pimpl->get_requesting_close();
}
