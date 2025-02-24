// Matches `GPU_camera`.
layout(set = 0, binding = 0) uniform Camera_buffer
{
    mat4 view;
	mat4 projection_view;
} camera;

// Matches `GPU_geo_instance_data`.
struct Geo_instance_data
{
    mat4 transform;
    uint bounding_sphere_idx;
    uint material_param_set_idx;
    uint render_layer;
    uint never_cull;
};

layout(std140, set = 0, binding = 1) readonly buffer Geo_instance_buffer
{
    Geo_instance_data instances[];
} geo_instance_buffer;
