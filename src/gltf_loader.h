#pragma once

#include <string>
#include <vector>
#if _WIN64
#include <vulkan/vulkan.h>
#endif  // _WIN64
#include "cglm/cglm.h"


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
//
// Also, I got a list of material names:
// - gold
// - slime_body
// - clothing_tights
// - slimegirl_eyebrows
// - slimegirl_eyes
// - slime_hair
// - suede_white
// - suede_gray
// - rubber_black
// - plastic_green
// - denim
// - leather
// - corduroy_white
// - ribbed_tan
// - knitting_green


struct Primitive
{
    uint32_t material_idx;
};

struct Mesh
{
    std::vector<Primitive> primitives;
};

struct Model
{
    std::vector<Mesh> meshes;
};

bool load_gltf(const std::string& path_str);

}  // namespace gltf_loader
