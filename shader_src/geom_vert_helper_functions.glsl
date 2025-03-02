// Helper functions for static mesh vertex.
vec3 calc_world_position()
{
    vec4 local_pos =
        params.geo_instance_buffer.instances[gl_BaseInstance].transform *
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
                mat3(params.geo_instance_buffer.instances[gl_BaseInstance].transform)
            )) * in_normal
        );
}
