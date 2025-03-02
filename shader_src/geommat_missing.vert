#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require

#include "geom_static_mesh_vert.glsl"
#include "geom_camera_set0.glsl"
#include "geom_instance_data_br.glsl"
#include "geom_material_sets_set1.glsl"

layout (location = 0) out vec3 out_normal;

layout (push_constant) uniform Params
{
    Geo_instance_buffer geo_instance_buffer;
} params;

#include "geom_vert_helper_functions.glsl"
#include "geom_material_sets_helper_functions.glsl"


void main()
{
    vec3 world_pos = calc_world_position();
    gl_Position = calc_projection_view_position(world_pos);
    out_normal = calc_normal();
}
