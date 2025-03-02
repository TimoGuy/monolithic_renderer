#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require

#include "geom_static_mesh_vert.glsl"
#include "geom_camera_set0.glsl"
#include "geom_instance_data_br.glsl"

layout (push_constant) uniform Params
{
    Geo_instance_buffer geo_instance_buffer;
} params;

#include "geom_vert_helper_functions.glsl"


void main()
{
    vec3 world_pos = calc_world_position();
    gl_Position = calc_projection_view_position(world_pos);
}
