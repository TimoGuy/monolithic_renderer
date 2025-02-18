#pragma once

#include <cinttypes>
#include "cglm/cglm.h"
#include "gltf_loader.h"
#include "gpu_geo_data.h"
#include "renderer_win64_vk_buffer.h"


namespace geo_instance
{

enum class Geo_render_pass : uint8_t
{
    OPAQUE = 0,
    WATER_TRANSPARENT,
    TRANSPARENT,
    NUM_GEO_RENDER_PASSES
};

struct Geo_instance
{
    uint32_t model_idx{ (uint32_t)-1 };
    Geo_render_pass render_pass{ Geo_render_pass::OPAQUE };
    // @NOTE: WATER_TRANSPARENT is never a shadow caster.
    bool is_shadow_caster{ true };

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

std::vector<Instance_primitive*> get_all_primitives();

}  // namespace geo_instance
