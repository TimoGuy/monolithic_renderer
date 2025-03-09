#include "imgui_system.h"

#include <atomic>
#include <cassert>
#include <string>
#include "imgui.h"

#if _WIN64
#include <iostream>
#include <set>  // For std::size
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "renderer_win64_vk_util.h"
#endif  // _WIN64

// Externally reaching systems.
#include "camera.h"
#include "input_handling_public.h"


namespace imgui_system
{

static std::atomic_bool s_imgui_setup{ false };

static std::atomic_bool s_imgui_enabled{ false };
static std::atomic_bool s_imgui_visible{ false };

#if _WIN64
static VkDevice s_v_device;
static VkDescriptorPool s_v_imgui_pool;
#endif  // _WIN64

}  // namespace imgui_system


// Setup/teardown.
#if _WIN64

bool imgui_system::build_imgui(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue graphics_queue,
    uint32_t graphics_queue_family_idx,
    VkFormat swapchain_image_format)
{
    bool result{ true };

    if (!s_imgui_enabled)
    {
        // Skip setting up imgui if disabled.
        return result;
    }

    s_v_device = device;

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
        vkCreateDescriptorPool(s_v_device, &pool_info, nullptr, &s_v_imgui_pool) };
    if (err)
    {
        std::cerr << "ERROR: Create imgui descriptor pool failed." << std::endl;
        result = false;
    }

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info{
        .Instance = instance,
        .PhysicalDevice = physical_device,
        .Device = s_v_device,
        .QueueFamily = graphics_queue_family_idx,
        .Queue = graphics_queue,
        .DescriptorPool = s_v_imgui_pool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchain_image_format,
        },
    };

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    s_imgui_setup = true;
    return result;
}

#endif  // _WIN64

bool imgui_system::teardown_imgui()
{
    bool result{ true };

    if (s_imgui_setup)
    {
#if _WIN64
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(s_v_device, s_v_imgui_pool, nullptr);
#endif  // _WIN64
    }

    s_imgui_setup = false;
    return result;
}

// Rendering.
void imgui_system::set_imgui_enabled(bool flag)
{
    s_imgui_enabled = flag;
}

void imgui_system::set_imgui_visible(bool flag)
{
    s_imgui_visible = flag;
}

bool render_imgui__demo_window()
{
    ImGui::ShowDemoWindow();
    return true;
}

bool render_imgui__camera_props()
{
    auto cam_data{ camera::get_imgui_data() };
    bool changed{ false };

    ImGui::Begin("Camera Properties");
    {
        changed |= ImGui::DragFloat("aspect_ratio", &cam_data.aspect_ratio);
        changed |= ImGui::DragFloat("fov_deg", &cam_data.fov_deg);
        changed |= ImGui::DragFloat("near", &cam_data.near);
        changed |= ImGui::DragFloat("far", &cam_data.far);

        ImGui::Separator();

        changed |= ImGui::DragFloat3("position", cam_data.position);
        changed |= ImGui::DragFloat("pan_deg", &cam_data.pan_deg);
        changed |= ImGui::DragFloat("tilt_deg", &cam_data.tilt_deg);
    }
    ImGui::End();

    // Process changes.
    if (changed)
    {
        camera::set_imgui_data(std::move(cam_data));
    }

    return true;
}

bool render_imgui__input_handling()
{
    ImGui::Begin("Input Handling Data");

    for (uint32_t i = 0; i < input_handling::get_num_state_sets(); i++)
    {
        auto& ih{  // Smelly smelly code smell. vv
            const_cast<input_handling::Input_state_set&>(
                input_handling::get_state_set(i)) };

        if (ImGui::CollapsingHeader(("Set " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::string suffix{ "## input handling data set " + std::to_string(i) };
            #define IMGUI_IS__VEC2(s, x) ImGui::InputFloat2((#x + suffix).c_str(), s.x)
            #define IMGUI_IS_IVEC2(s, x) ImGui::InputInt2((#x + suffix).c_str(), s.x)
            #define IMGUI_IS_FLOAT(s, x) ImGui::InputFloat((#x + suffix).c_str(), &s.x)
            #define IMGUI_IS__BOOL(s, x) ImGui::Checkbox((#x + suffix).c_str(), &s.x)

            IMGUI_IS__VEC2(ih, gameplay.camera_delta);
            IMGUI_IS__VEC2(ih, gameplay.movement);
            IMGUI_IS__BOOL(ih, gameplay.jump);
            IMGUI_IS__BOOL(ih, gameplay.dodge_sprint);
            IMGUI_IS__BOOL(ih, gameplay.switch_weapon);
            IMGUI_IS__BOOL(ih, gameplay.attack);
            IMGUI_IS__BOOL(ih, gameplay.deflect_block);
            IMGUI_IS__BOOL(ih, gameplay.toggle_lock);
            IMGUI_IS__BOOL(ih, gameplay.interact);

            ImGui::Separator();

            IMGUI_IS__VEC2(ih, ui.cursor_position);
            IMGUI_IS_FLOAT(ih, ui.scroll_delta);
            IMGUI_IS_IVEC2(ih, ui.navigate_movement);
            IMGUI_IS__BOOL(ih, ui.confirm);
            IMGUI_IS__BOOL(ih, ui.cancel);

            ImGui::Separator();

            IMGUI_IS__VEC2(ih, level_editor.camera_delta);
            IMGUI_IS__VEC2(ih, level_editor.movement);
            IMGUI_IS_FLOAT(ih, level_editor.move_world_y_axis);
            IMGUI_IS__BOOL(ih, level_editor.lshift_modifier);
            IMGUI_IS__BOOL(ih, level_editor.lctrl_modifier);
            IMGUI_IS__BOOL(ih, level_editor.camera_move);

            #undef IMGUI_IS__VEC2(s, x)
            #undef IMGUI_IS_IVEC2(s, x)
            #undef IMGUI_IS_FLOAT(s, x)
            #undef IMGUI_IS__BOOL(s, x)
        }
    }

    ImGui::End();  // "Input Handling Data"

    return true;
}

bool imgui_system::render_imgui()
{
    bool result{ true };

    if (s_imgui_enabled && s_imgui_visible)
    {
        assert(s_imgui_setup);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        result &= render_imgui__demo_window();
        result &= render_imgui__camera_props();
        result &= render_imgui__input_handling();
        ImGui::Render();
    }

    return result;
}

// Actual drawing.
#if _WIN64

bool imgui_system::render_imgui_onto_swapchain(
    VkCommandBuffer cmd,
    VkExtent2D render_extent,
    VkImageView target_image_view)
{
    if (s_imgui_enabled && s_imgui_visible)
    {
        assert(s_imgui_setup);

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

    return true;
}

#endif  // _WIN64
