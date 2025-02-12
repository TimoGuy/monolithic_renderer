#include "geo_instance.h"

#include <array>
#include <vector>


namespace geo_instance
{

using Instance_list_t = std::vector<Geo_instance>;
using Geo_render_pass_sorted_instance_list_array_t = std::array<Instance_list_t, static_cast<uint8_t>(Geo_render_pass::NUM_GEO_RENDER_PASSES)>;

static Geo_render_pass_sorted_instance_list_array_t s_bucketed_instance_list;

}  // namespace geo_instance
