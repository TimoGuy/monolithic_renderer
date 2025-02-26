#version 460
#extension GL_GOOGLE_include_directive : enable

#include "geommat_static_mesh_vert.glsl"
#include "geom_per_frame_datas_set0.glsl"


void main()
{
    vec3 world_pos = calc_world_position();
    gl_Position = calc_projection_view_position(world_pos);
}
