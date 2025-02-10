#pragma once

#include <string>
#include <vector>
#if _WIN64
#include <vulkan/vulkan.h>
#endif  // _WIN64
#include "cglm/cglm.h"
#include "renderer_win64_vk_immediate_submit.h"


namespace gltf_loader
{

struct VertexInputDescription
{
#if _WIN64
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    VkPipelineVertexInputStateCreateFlags flags = 0;
#else
#error "`struct VertexInputDescription` requires new OS implementation."
#endif  // _WIN64
};

struct GPU_vertex
{
    vec3     position;
    uint32_t primitive_idx;
	vec3     normal;
    uint32_t pad0;
	vec2     uv;
	vec4     color;

    static VertexInputDescription get_static_vertex_description();
};

// @TODO: @START HERE THEA
// So essentially, I was spending a bunch of time trying to figure out how the gltf model was structured so that I could think about the best way to render them.
// ~~I think that essentially having everything grouped by material first would be best.~~ Actually, I would say that the model artist should be in charge of material mesh optimization.
// Having a skinned version that goes into an intermediate vertex buffer would be super good too.
// Ummmmmm, I also think that having animations and stuff down to a tee would be good.
// @TODO: look at how the animations are structured inside the gltf thingo in the solanine_vulkan project.

struct Primitive
{
    uint32_t start_index;
    uint32_t index_count;
    uint32_t default_material_idx;
};

struct Model
{
    uint32_t base_index;
    std::vector<Primitive> primitives;
};

bool load_gltf(const std::string& path_str);

bool upload_combined_mesh(const vk_util::Immediate_submit_support& support,
                          VkDevice device,
                          VkQueue queue,
                          VmaAllocator allocator);

}  // namespace gltf_loader
