#pragma once

#include <cinttypes>
#include "cglm/cglm.h"
#include "gpu_geo_data.h"


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
    Geo_render_pass render_pass{ Geo_render_pass::OPAQUE };
    // @NOTE: WATER_TRANSPARENT is never a shadow caster.
    bool is_shadow_caster{ true };

    gpu_geo_data::GPU_instance_data gpu_instance_data;
};

}  // namespace geo_instance
