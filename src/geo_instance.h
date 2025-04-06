#pragma once

#include <cinttypes>
#include <unordered_map>
#include <utility>  // For `std::pair`.
#include <vector>
#include "cglm/cglm.h"
#include "geo_render_pass.h"
#include "gltf_loader.h"
#include "gpu_geo_data.h"
#include "renderer_win64_vk_buffer.h"
namespace world_sim { class Transform_read_ifc; }


namespace geo_instance
{

struct Geo_instance
{
    uint32_t model_idx{ (uint32_t)-1 };
    Geo_render_pass render_pass{ Geo_render_pass::OPAQUE };
    // @NOTE: WATER_TRANSPARENT is never a shadow caster.
    bool is_shadow_caster{ true };

    // Transform reader handle.
    // @NOTE: If nullptr, then this geo instance will not pull from a transform holder.
    // @TODO: @THEA: I feel like at a certain point, when there's not a transform
    //   reader handle there should definitely be a lock or a something for making
    //   sure that the changed data is visible.
    world_sim::Transform_read_ifc* transform_reader_handle{ nullptr };

    // Position in instance buffer where this instance
    // is uploaded on the GPU.
    uint32_t cooked_buffer_instance_id;

    gpu_geo_data::GPU_geo_instance_data gpu_instance_data;
};

// Primitive w/ corresponding ptr to instance.
struct Instance_primitive  // @TODO: rename to Primitive_wc_instance
{
    const gltf_loader::Primitive* primitive;
    const Geo_instance* instance;
};

using Geo_instance_key_t = uint32_t;

Geo_instance_key_t register_geo_instance(Geo_instance&& new_instance);

void unregister_geo_instance(Geo_instance_key_t key);

void rebuild_bucketed_instance_list_array(std::vector<vk_buffer::GPU_geo_per_frame_buffer*>& all_per_frame_buffers);

std::vector<Geo_instance*> get_all_unique_instances();

uint32_t get_unique_instances_count();

using Primitive_ptr_list_t = std::vector<Instance_primitive*>;
Primitive_ptr_list_t get_all_primitives();

using Pipeline_id_t = uint32_t;
using Per_pipeline_primitive_ptr_list_t = std::unordered_map<uint32_t, Primitive_ptr_list_t>;
Per_pipeline_primitive_ptr_list_t get_pipeline_grouped_primitives(Geo_render_pass render_pass_id);

using Base_primitive_idx_t = uint32_t;
using Primitive_render_group_t = std::pair<Pipeline_id_t, Base_primitive_idx_t>;
using Primitive_render_group_list_t = std::vector<Primitive_render_group_t>;
Primitive_render_group_list_t get_all_base_primitive_indices();  // @TODO: reorder to higher up.

uint32_t get_number_primitives(Geo_render_pass render_pass_id);

uint32_t get_num_primitive_render_groups(Geo_render_pass render_pass_id);

}  // namespace geo_instance
