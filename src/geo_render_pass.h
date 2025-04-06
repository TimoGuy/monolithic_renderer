#pragma once

#include <cinttypes>


namespace geo_instance
{

enum class Geo_render_pass : uint8_t
{
    OPAQUE = 0,
    WATER_TRANSPARENT,
    TRANSPARENT,
    NUM_GEO_RENDER_PASSES
};

}  // namespace geo_instance
