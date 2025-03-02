// Matches `GPU_geo_instance_data`.
struct Geo_instance_data
{
    mat4 transform;
    uint bounding_sphere_idx;
    uint material_param_set_idx;
    uint render_layer;
    uint never_cull;
};
layout(buffer_reference, std140) readonly buffer Geo_instance_buffer
{
    Geo_instance_data instances[];
};
