#pragma once

#include <string>
#include <vector>
#if _WIN64
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#endif  // _WIN64
#include "cglm/cglm.h"
#include "gpu_geo_data.h"
#include "renderer_win64_vk_immediate_submit.h"


namespace gltf_loader
{

struct Vertex_input_description
{
#if _WIN64
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    VkPipelineVertexInputStateCreateFlags flags = 0;
#else
#error "`struct Vertex_input_description` requires new OS implementation."
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

    static Vertex_input_description get_static_vertex_description();
};

struct Primitive
{
    uint32_t start_index;
    uint32_t index_count;
};

struct Bounding_sphere
{
    // @NOTE: This gets uploaded into the GPU
    //        as simply a vec4.
    vec3 origin;
    float_t radius;
};

struct Model
{
    Bounding_sphere bounding_sphere;
    std::vector<Primitive> primitives;
};

bool load_gltf(const std::string& path_str);

bool upload_combined_mesh(const vk_util::Immediate_submit_support& support,
                          VkDevice device,
                          VkQueue queue,
                          VmaAllocator allocator);

const Model& get_model(uint32_t idx);

const std::vector<gpu_geo_data::GPU_bounding_sphere>& get_all_bounding_spheres();

bool teardown_all_meshes();

}  // namespace gltf_loader
