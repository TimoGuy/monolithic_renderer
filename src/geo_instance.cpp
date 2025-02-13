#include "geo_instance.h"

#include <array>
#include <atomic>
#include <cassert>
#include <unordered_map>
#include <vector>
#include "gltf_loader.h"
#include "material_bank.h"


// @THEA: This should be enough information to get to the point where you have a functioning material system.
//        I think that the necessary things left are content essentially. Get some shaders, some materials, and then create material sets.
//        I think another thing that's necessary to think about and plan is @TODO how to get the material params inputted into the shaders and stuff like that. That will be cumbersome and a pain I feel like.
// @TODO: START HERE!!!! READ ^^


namespace geo_instance
{

// Access the bucket by: Geo render pass and then Pipeline idx.
struct Instance_primitive
{
    Geo_instance* instance;
    gltf_loader::Primitive* primitive;
};

using Primitive_list_t = std::vector<Instance_primitive>;
using Pipeline_primitive_list_map_t = std::unordered_map<uint32_t, Primitive_list_t>;
constexpr size_t k_num_render_passes{ static_cast<uint8_t>(Geo_render_pass::NUM_GEO_RENDER_PASSES) };
using Geo_render_pass_bucketed_primitive_list_array_t =
    std::array<Pipeline_primitive_list_map_t, k_num_render_passes>;

static Geo_render_pass_bucketed_primitive_list_array_t s_bucketed_instance_primitives_list;
static std::atomic_bool s_flag_rebucketing{ false };
#if _DEBUG
static std::atomic_bool s_currently_rebucketing{ false };
#endif  // _DEBUG

// @INCOMPLETE: Use a pool instead of a array here but that's for the future. //
constexpr size_t k_num_instances{ 1024 };
static std::array<Geo_instance, k_num_instances> s_all_instances;
static std::atomic_uint32_t s_current_register_idx{ 0 };
/////////////////////////////////////////////////////////////////////////////////

}  // namespace geo_instance


geo_instance::Geo_instance_key_t geo_instance::register_geo_instance(Geo_instance&& new_instance)
{
    // @NOTE: registering a geo instance should never happen at
    //        the same time as rebucketing.
    assert(!s_currently_rebucketing);

    // @INCOMPLETE: change the implementation to a geo instance pool.
    Geo_instance_key_t new_instance_idx{ s_current_register_idx++ };
    assert(new_instance_idx < k_num_instances);
    s_all_instances[new_instance_idx] = std::move(new_instance);
    /////////////////////////////////////////////////////////////////

    s_flag_rebucketing = true;
    return new_instance_idx;
}

void geo_instance::unregister_geo_instance(Geo_instance_key_t key)
{
    // @INCOMPLETE: change the implementation to a geo instance pool. But first get the pool out into its own component.
    assert(false);
}

void geo_instance::rebuild_bucketed_instance_list_array()
{
    if (!s_flag_rebucketing)
        return;
#if _DEBUG
    s_currently_rebucketing = true;
#endif  // _DEBUG

    // Rebucket.
    for (auto& instance_list_map : s_bucketed_instance_primitives_list)  // @CHECK: does a mem leak happen here?????
    {
        instance_list_map.clear();
    }

    // @INCOMPLETE: asdfasdfasdfasdfasdf
    uint32_t count{ s_current_register_idx };
    for (uint32_t i = 0; i < count; i++)
    {
        auto& instance{ s_all_instances[i] };
        auto& model{ gltf_loader::get_model(instance.model_idx) };
        auto& material_set{
            material_bank::get_material_set(instance.gpu_instance_data.material_param_set_idx) };
        
        assert(model.primitives.size() == material_set.material_indexes.size());

        for (size_t i = 0; i < model.primitives.size(); i++)
        {
            auto& primitive{ model.primitives[i] };
            auto& material_idx{ material_set.material_indexes[i] };
            auto& pipeline_idx{ material_bank::get_material(material_idx).pipeline_idx };
            s_bucketed_instance_primitives_list
                [static_cast<uint8_t>(instance.render_pass)]
                [pipeline_idx]
                    .emplace_back(&instance, &primitive);
        }
    }
    ////////////////////////////////////

    // End process.
#if _DEBUG
    s_currently_rebucketing = false;
#endif  // _DEBUG
    s_flag_rebucketing = false;
}
