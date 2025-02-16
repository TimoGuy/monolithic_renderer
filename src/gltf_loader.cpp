#include "gltf_loader.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>
#include "cglm/cglm.h"
// Fast gltf includes must be in this order. //
#include "fastgltf_support__cglm_element_traits.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
///////////////////////////////////////////////
#include "gpu_geo_data.h"
#include "material_bank.h"
#include "renderer_win64_vk_buffer.h"


namespace gltf_loader
{

// To fit all models into one buffer, using this vertex offset.
static std::atomic_uint32_t s_indices_base_vertex{ 0 };
static std::vector<uint32_t> s_staging_indices;  // Use `s_indices_base_vertex` for visibility.
static std::vector<GPU_vertex> s_staging_vertices;  // Use `s_indices_base_vertex` for visibility.
static std::vector<Model> s_staging_models;  // Accessor into all indices.

static vk_buffer::GPU_mesh_buffer s_static_mesh_buffer;  // Cooked buffer.
static std::vector<Model> s_cooked_models;  // Accessor into all indices of cooked buffer.
static std::vector<gpu_geo_data::GPU_bounding_sphere> s_cooked_bounding_spheres;

// glTF 2.0 attribute strings.
constexpr const char* k_position_str{ "POSITION" };
constexpr const char* k_normal_str{ "NORMAL" };
constexpr const char* k_uv_str{ "TEXCOORD_0" };
constexpr const char* k_color_str{ "COLOR_0" };
constexpr const char* k_joints_str{ "JOINTS_0" };
constexpr const char* k_weights_str{ "WEIGHTS_0" };

using fgltf_attributes_t = fastgltf::pmr::SmallVector<fastgltf::Attribute, 4Ui64>;
bool validate_vertex_attributes(const fgltf_attributes_t& attributes)
{
    // @THEA: no need for validation, will just do checks as accessors are checked.
    return true;  // @TODO: get rid of this function!

    struct String_occurance { std::string str;
                              uint32_t discovered_count{ 0 }; };
    std::vector<String_occurance> str_occurances{
        // @NOTE: DO NOT REPEAT ANY STRING ELEMS HERE.
        { k_position_str },
        { k_normal_str },
        { k_uv_str },
        { k_color_str },
        { k_joints_str },
        { k_weights_str },
    };

    // Count occurrances inside of primitive attributes.
    for (auto& attribute : attributes)
    for (auto& str_occurance : str_occurances)
    {
        if (str_occurance.str == std::string{ attribute.name })
            str_occurance.discovered_count++;
    }

    // Validate that all strings occur exactly once.
    bool is_valid{ true };
    for (const auto& str_occurance : str_occurances)
        is_valid &= (str_occurance.discovered_count == 1);
    
    return is_valid;
}

}  // namespace gltf_loader

gltf_loader::Vertex_input_description gltf_loader::GPU_vertex::get_static_vertex_description()
{
    // @NOTE: This is only used for static vertices.
    //        All skinned/alembic vertices will be run thru a compute
    //        shader and inserted into this same vertex description.
    //        Skinned/alembic vertices are placed in a storage buffer instead
    //        (WARNING: storage buffers will have different memory padding rules)
    static Vertex_input_description vertex_desc{
        .bindings{
            VkVertexInputBindingDescription{
                .binding = 0,
                .stride = sizeof(GPU_vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
        },
        .attributes{
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(GPU_vertex, position),
            },
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(GPU_vertex, normal),
            },
            VkVertexInputAttributeDescription{
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(GPU_vertex, uv),
            },
            VkVertexInputAttributeDescription{
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(GPU_vertex, color),
            },
            VkVertexInputAttributeDescription{
                .location = 4,
                .binding = 0,
                .format = VK_FORMAT_R32_UINT,
                .offset = offsetof(GPU_vertex, primitive_idx),
            },
        },
        .flags{ 0 },
    };
    return vertex_desc;
}

bool gltf_loader::load_gltf(const std::string& path_str)
{
    std::filesystem::path path{ path_str };
    if (!std::filesystem::exists(path))
    {
        std::cerr << "ERROR: Load gltf " << path << std::endl;
        return false;
    }

    static constexpr auto gltf_extensions{ fastgltf::Extensions::None };
    static constexpr auto gltf_options{ fastgltf::Options::DontRequireValidAssetMember |
                                        fastgltf::Options::LoadExternalBuffers };

    fastgltf::Parser parser{ gltf_extensions };
    auto gltf_file{ fastgltf::MappedGltfFile::FromPath(path) };
    if (!gltf_file)
    {
        std::cerr << "ERROR: Opening gltf file failed." << std::endl;
        return false;
    }

    auto asset_expect{ parser.loadGltf(gltf_file.get(), path.parent_path(), gltf_options) };
    if (asset_expect.error() != fastgltf::Error::None)
    {
        std::cerr << "ERROR: Loading gltf file asset failed." << std::endl;
        return false;
    }

    auto asset{ std::move(asset_expect.get()) };

    // Check that all desired vertex attributes are included.
    // @TODO: Remove this check section, since it will be checked below
    //        since most attribs (except for position) are optional.
    bool is_model_valid{ true };
    for (auto& mesh : asset.meshes)
    for (auto& primitive : mesh.primitives)
    {
        is_model_valid &= validate_vertex_attributes(primitive.attributes);
    }
    if (!is_model_valid)
    {
        std::cerr << "ERROR: loading gltf had incorrect vertex attributes." << std::endl;
        assert(false);
    }

    // Emit warnings in case some material names are missing.
    for (auto& mesh : asset.meshes)
    for (auto& primitive : mesh.primitives)
    {
        std::string mat_name{ asset.materials[primitive.materialIndex.value()].name };
        uint32_t material_idx{
            material_bank::get_mat_idx_from_name(mat_name) };
        if (material_idx == material_bank::k_invalid_material_idx)
        {
            std::cerr
                << "WARNING: Material \"" << mat_name << "\" not registered."
                << std::endl;
        }
    }

    // Acquire base vertex atomically and lock.
    uint32_t base_vertex;
    uint32_t base_vertex_load;
    do
    {   base_vertex_load = s_indices_base_vertex.load();
        if (base_vertex_load == (uint32_t)-1) base_vertex_load = 0;
        base_vertex = base_vertex_load;
    } while (!s_indices_base_vertex.compare_exchange_weak(base_vertex_load, (uint32_t)-1));

    // @NOTE: Primitive index is the primitive num inside of each individual model.
    //        Used for finding which material to access within a material set.
    //        See `GPU_vertex`.
    size_t primitive_index{ 0 };

    // Create a new model.
    // Also, create an AABB to convert into a bounding sphere.
    // @NOTE: The resulting bounding sphere is certainly going to be
    //        larger than the AABB due to it being circumscribed.
    Model new_model;
    constexpr float_t k_lowest{ std::numeric_limits<float_t>::min() };
    constexpr float_t k_highest{ std::numeric_limits<float_t>::max() };
    vec3 min_pos{ k_highest, k_highest, k_highest };
    vec3 max_pos{ k_lowest, k_lowest, k_lowest };

    // Load geometry into giant set of meshes.
    for (auto& mesh : asset.meshes)
    for (auto& primitive : mesh.primitives)
    {
        assert(primitive.type == fastgltf::PrimitiveType::Triangles);

        std::string material_name{
            asset.materials[primitive.materialIndex.value()].name };

        Primitive new_primitive{
            .start_index = static_cast<uint32_t>(s_staging_indices.size()),
        };

        // Load indices.
        {
            auto& accessor{ asset.accessors[primitive.indicesAccessor.value()] };
            s_staging_indices.reserve(s_staging_indices.size() + accessor.count);
            fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](uint32_t idx) {
                s_staging_indices.emplace_back(base_vertex + idx);
            });
        }

        // Load vertices.
        {
            // Position.
            auto& position_accessor{
                asset.accessors[primitive.findAttribute(k_position_str)->accessorIndex] };

            size_t num_new_vertices{ position_accessor.count };
            s_staging_vertices.resize(s_staging_vertices.size() + num_new_vertices);

            fastgltf::iterateAccessorWithIndex<vec3s>(asset,
                                                      position_accessor,
                                                      [&](vec3s vec, size_t index) {
                auto& vert{ s_staging_vertices[base_vertex + index] };
                glm_vec3_copy(vec.raw, vert.position);
                vert.primitive_idx = 0;
                glm_vec3_zero(vert.normal);
                vert.pad0 = 0;
                glm_vec2_zero(vert.uv);
                glm_vec4_zero(vert.color);

                // Work towards creating AABB.
                glm_vec3_minv(min_pos, vec.raw, min_pos);
                glm_vec3_maxv(max_pos, vec.raw, max_pos);
            });

            // Primitive index.
            for (size_t index = 0; index < num_new_vertices; index++)
                s_staging_vertices[base_vertex + index].primitive_idx = primitive_index;

            // Normal.
            auto normal_attribute{ primitive.findAttribute(k_normal_str) };
            if (normal_attribute != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<vec3s>(asset,
                                                        asset.accessors[normal_attribute->accessorIndex],
                                                        [&](vec3s vec, size_t index) {
                    auto& vert{ s_staging_vertices[base_vertex + index] };
                    glm_vec3_copy(vec.raw, vert.normal);
                });
            }

            // UV.
            auto uv_attribute{ primitive.findAttribute(k_uv_str) };
            if (uv_attribute != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<vec2s>(asset,
                                                        asset.accessors[uv_attribute->accessorIndex],
                                                        [&](vec2s vec, size_t index) {
                    auto& vert{ s_staging_vertices[base_vertex + index] };
                    glm_vec2_copy(vec.raw, vert.uv);
                });
            }

            // Vertex color.
            auto color_attribute{ primitive.findAttribute(k_color_str) };
            if (color_attribute != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<vec4s>(asset,
                                                        asset.accessors[color_attribute->accessorIndex],
                                                        [&](vec4s vec, size_t index) {
                    auto& vert{ s_staging_vertices[base_vertex + index] };
                    glm_vec4_copy(vec.raw, vert.color);
                });
            }

            // @TODO: @THEA: add joints and weights.
        }

        primitive_index++;
        base_vertex = s_staging_vertices.size();
        new_primitive.index_count =
            (s_staging_indices.size() - new_primitive.start_index);
        new_model.primitives.emplace_back(new_primitive);
    }

// @TODO: implement animations and stuff @THEA
#define IMPLEMENTED_ANIMATIONS 0
#if IMPLEMENTED_ANIMATIONS
    // If animated:

        // Load in animations and flag meshes into staging buffer.

    // If not animated:

        // Generate distance field if not generated.
        // (Do it by cpu or gpu??)

        // Save generated distance field.
#endif  // IMPLEMENTED_ANIMATIONS

    // Create bounding sphere for meshes.
    // @NOTE: simply circumscribes the resulting AABB.
    vec3 bs_origin;
    glm_vec3_add(min_pos, max_pos, bs_origin);
    glm_vec3_scale(bs_origin, 0.5f, new_model.bounding_sphere.origin);
    new_model.bounding_sphere.radius =
        glm_vec3_distance(new_model.bounding_sphere.origin, max_pos);

#if _DEBUG
    // @TODO: Give aabb score for mesh vs bounding sphere
    // (make sure all 3 dimensions are as similar as possible so that occlusion check is easiest).
    // This just goes into the console or in a stats sheet for improving the assets.
    // There could also be something like a warning if the score is bad enough.
#endif  // _DEBUG

    s_staging_models.emplace_back(new_model);

    // Release lock on loading new gltf models.
    s_indices_base_vertex.store(base_vertex);

    return true;
}

bool gltf_loader::upload_combined_mesh(const vk_util::Immediate_submit_support& support,
                                       VkDevice device,
                                       VkQueue queue,
                                       VmaAllocator allocator)
{
    // Acquire base vertex atomically and lock. (@COPYPASTA)
    uint32_t base_vertex;
    uint32_t base_vertex_load;
    do
    {
        base_vertex_load = s_indices_base_vertex.load();
        if (base_vertex_load == (uint32_t)-1) base_vertex_load = 0;
        base_vertex = base_vertex_load;
    } while (!s_indices_base_vertex.compare_exchange_weak(base_vertex_load, (uint32_t)-1));

    // Upload combined mesh to gpu.
    s_static_mesh_buffer =
        vk_buffer::upload_mesh_to_gpu(support,
                                      device,
                                      queue,
                                      allocator,
                                      std::move(s_staging_indices),
                                      std::move(s_staging_vertices));
    s_cooked_models = std::move(s_staging_models);

    // Move bounding spheres into bounding sphere structs.
    s_cooked_bounding_spheres.clear();
    s_cooked_bounding_spheres.reserve(s_cooked_models.size());
    for (auto& model : s_cooked_models)
    {
        gpu_geo_data::GPU_bounding_sphere bs{
            .origin_xyz_radius_w{
                model.bounding_sphere.origin[0],
                model.bounding_sphere.origin[1],
                model.bounding_sphere.origin[2],
                model.bounding_sphere.radius
            }
        };
        s_cooked_bounding_spheres.emplace_back(bs);
    }

    assert(s_cooked_models.size() == s_cooked_bounding_spheres.size());

    // Clear all cpu side buffers.
    s_staging_indices.clear();
    s_staging_vertices.clear();
    s_staging_models.clear();

    // Release lock on loading new gltf models.
    s_indices_base_vertex.store(0);

    return true;
}

const gltf_loader::Model& gltf_loader::get_model(uint32_t idx)
{
    if (s_indices_base_vertex == 0)
    {
        assert(!s_cooked_models.empty());
        assert(idx < s_cooked_models.size());
        return s_cooked_models[idx];
    }

    // If not finished cooking models, just vector.
    return s_cooked_models[(size_t)-1];
}

const std::vector<gpu_geo_data::GPU_bounding_sphere>& gltf_loader::get_all_bounding_spheres()
{
    if (s_indices_base_vertex == 0)
    {
        assert(!s_cooked_bounding_spheres.empty());
        return s_cooked_bounding_spheres;
    }

    // If not finished cooking models, just vector.
    assert(false);
}

bool gltf_loader::teardown_all_meshes()
{
    // @TODO: implement.
    assert(false);
    return true;
}
