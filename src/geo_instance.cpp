#include "geo_instance.h"

#include <array>
#include <atomic>
#include <cassert>
#include "gltf_loader.h"
#include "material_bank.h"
#include "timing_reporter_public.h"


// @THEA: This should be enough information to get to the point where you have a functioning material system.
//        I think that the necessary things left are content essentially. Get some shaders, some materials, and then create material sets.
//        I think another thing that's necessary to think about and plan is @TODO how to get the material params inputted into the shaders and stuff like that. That will be cumbersome and a pain I feel like.
// @TODO: START HERE!!!! READ ^^


namespace geo_instance
{

// Access the bucket by: Geo render pass and then Pipeline idx.
using Primitive_list_t = std::vector<Instance_primitive>;
using Pipeline_primitive_list_map_t = std::unordered_map<Pipeline_id_t, Primitive_list_t>;
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
static std::vector<uint32_t> s_changed_inst_indices;
static std::atomic_uint32_t s_current_register_idx{ 0 };
////////////////////////////////////////////////////////////////////////////////

}  // namespace geo_instance


geo_instance::Geo_instance_key_t geo_instance::register_geo_instance(Geo_instance&& new_instance)
{
    // @NOTE: registering a geo instance should never happen at
    //        the same time as rebucketing.
    assert(!s_currently_rebucketing);

    new_instance.gpu_instance_data.bounding_sphere_idx = new_instance.model_idx;

    // @INCOMPLETE: change the implementation to a geo instance pool.
    Geo_instance_key_t new_instance_idx{ s_current_register_idx++ };
    assert(new_instance_idx < k_num_instances);
    s_all_instances[new_instance_idx] = std::move(new_instance);
    s_changed_inst_indices.emplace_back(new_instance_idx);
    /////////////////////////////////////////////////////////////////

    s_flag_rebucketing = true;
    return new_instance_idx;
}

void geo_instance::unregister_geo_instance(Geo_instance_key_t key)
{
    // @INCOMPLETE: change the implementation to a geo instance pool. But first get the pool out into its own component.
    assert(false);
}

void geo_instance::rebuild_bucketed_instance_list_array(std::vector<vk_buffer::GPU_geo_per_frame_buffer*>& all_per_frame_buffers)
{
    if (s_flag_rebucketing)
        return;
#if _DEBUG
    s_currently_rebucketing = true;
#endif  // _DEBUG

    TIMING_REPORT_START(rebucket);

    // Insert built up changed instance indices.
    // @TODO: perhaps include something like this in the future but for now just mark a rebuild.
    // vk_buffer::set_new_changed_indices(std::move(s_changed_inst_indices),
    //                                    all_per_frame_buffers);
    // s_changed_inst_indices = std::vector<uint32_t>();
    //
    // @TODO: improve performance with updating everything only when
    // the instance pool updates but only update the data in the position
    // of the instance if it's just the only thing(s) that have updated.  -Thea 2025/02/19
    vk_buffer::flag_update_all_instances(all_per_frame_buffers);

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
        
        // @NOTE: assert that the model's number of materials matches the number
        //        of materials in the material set.
        assert(model.primitives.size() == material_set.material_indexes.size());

        for (size_t i = 0; i < model.primitives.size(); i++)
        {
            auto& primitive{ model.primitives[i] };
            auto& material_idx{ material_set.material_indexes[i] };
            auto& pipeline_idx{ material_bank::get_material(material_idx).pipeline_idx };
            s_bucketed_instance_primitives_list
                [static_cast<uint8_t>(instance.render_pass)]
                [pipeline_idx]
                    .emplace_back(&primitive, &instance);
        }
    }
    ////////////////////////////////////

    TIMING_REPORT_END_AND_PRINT(rebucket, "Instance List Array Rebucket: ");

    // End process.
#if _DEBUG
    s_currently_rebucketing = false;
#endif  // _DEBUG
    s_flag_rebucketing = false;
}

std::vector<geo_instance::Geo_instance*> geo_instance::get_all_unique_instances()
{
    std::vector<Geo_instance*> instances;

    // @INCOMPLETE: THIS ISN't THE FINAL IMPLEMENTATION AFAIK //
    Geo_instance_key_t inst_count{ s_current_register_idx };
    assert(inst_count <= k_num_instances);

    instances.reserve(inst_count);
    for (Geo_instance_key_t k = 0; k < inst_count; k++)
    {
        instances.emplace_back(&s_all_instances[k]);
    }
    ////////////////////////////////////////////////////////////

    return instances;
}

geo_instance::Primitive_ptr_list_t geo_instance::get_all_primitives()
{
    Primitive_ptr_list_t inst_prims;

    if (!s_flag_rebucketing)
    {
        // @INCOMPLETE: asdfasdfasdfasdfasdf
        // @NOTE: this may be the exact way this is addressed since this is the bucket notation stuff.
        for (auto& render_pass : s_bucketed_instance_primitives_list)
        for (auto it = render_pass.begin(); it != render_pass.end(); it++)
        for (auto& instance_primitive : it->second)
        {
            inst_prims.emplace_back(&instance_primitive);
        }
        inst_prims.shrink_to_fit();
        ////////////////////////////////////
    }

    return inst_prims;
}

geo_instance::Per_pipeline_primitive_ptr_list_t geo_instance::get_pipeline_grouped_primitives(
    Geo_render_pass render_pass_id)
{
    Per_pipeline_primitive_ptr_list_t inst_prims_grouped;

    if (!s_flag_rebucketing)
    {
        // @INCOMPLETE: asdfasdfasdfasdfasdf
        // @NOTE: this may be the exact way this is addressed since this is the bucket notation stuff.
        auto& render_pass{
            s_bucketed_instance_primitives_list[static_cast<size_t>(render_pass_id)]
        };
        for (auto it = render_pass.begin(); it != render_pass.end(); it++)
        {
            for (auto& instance_primitive : it->second)
            {
                inst_prims_grouped[it->first].emplace_back(&instance_primitive);
            }
            inst_prims_grouped[it->first].shrink_to_fit();
        }
        ////////////////////////////////////
    }

    return inst_prims_grouped;
}

geo_instance::Primitive_render_group_list_t geo_instance::get_base_primitive_indices(Geo_render_pass render_pass_id)
{
    Primitive_render_group_list_t render_group_list;

    if (!s_flag_rebucketing)
    {
        // @INCOMPLETE: asdfasdfasdfasdfasdf
        // @NOTE: this may be the exact way this is addressed since this is the bucket notation stuff.
        auto& render_pass{
            s_bucketed_instance_primitives_list[static_cast<size_t>(render_pass_id)]
        };
        uint32_t current_primitive_idx{ 0 };
        for (auto it = render_pass.begin(); it != render_pass.end(); it++)
        {
            render_group_list.emplace_back(it->first, current_primitive_idx);
            current_primitive_idx += it->second.size();
        }
        render_group_list.shrink_to_fit();
        ////////////////////////////////////
    }

    return render_group_list;
}

uint32_t geo_instance::get_number_primitives(Geo_render_pass render_pass_id)
{
    uint32_t count{ 0 };

    if (!s_flag_rebucketing)
    {
        // @INCOMPLETE: asdfasdfasdfasdfasdf
        // @NOTE: this may be the exact way this is addressed since this is the bucket notation stuff.
        auto& render_pass{
            s_bucketed_instance_primitives_list[static_cast<size_t>(render_pass_id)]
        };
        for (auto it = render_pass.begin(); it != render_pass.end(); it++)
        {
            count += it->second.size();
        }
        ////////////////////////////////////
    }

    return count;
}
