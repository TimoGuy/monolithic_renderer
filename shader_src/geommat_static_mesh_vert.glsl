// Matches `gltf_loader::GPU_vertex::get_static_vertex_description()`.
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;
layout (location = 3) in vec4 in_color;
layout (location = 4) in uint in_primitive_idx;

// Per frame data (set = 0).
#include "geom_per_frame_datas_set0.glsl"

// Helper functions.
vec3 calc_world_position()
{
    vec4 local_pos =
        geo_instance_buffer.instances[gl_BaseInstance].transform *
            vec4(in_position, 1.0);
    return local_pos.xyz / local_pos.w;
}

vec4 calc_projection_view_position(vec3 world_pos)
{
    return camera.projection_view * vec4(world_pos, 1.0);
}

vec3 calc_view_position(vec3 world_pos)
{
    return (camera.view * vec4(world_pos, 1.0)).xyz;
}

vec3 calc_normal()
{
    return
        normalize(
            transpose(inverse(
                mat3(geo_instance_buffer.instances[gl_BaseInstance].transform)
            )) * in_normal
        );
}
