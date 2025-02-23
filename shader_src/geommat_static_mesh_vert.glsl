// Matches `gltf_loader::GPU_vertex::get_static_vertex_description()`.
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;
layout (location = 3) in vec4 in_color;
layout (location = 4) in uint in_primitive_idx;

// Matches `GPU_camera`.
layout(set = 0, binding = 0) uniform Camera_buffer
{
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

// Matches `GPU_bounding_sphere`.
struct Bounding_sphere
{
    vec4 origin_xyz_radius_w;
};

layout(std140, set = 0, binding = 2) readonly buffer Bounding_sphere_buffer
{
    Bounding_sphere bounding_spheres[];
} bounding_sphere_buffer;

// See `GPU_material_set`.
struct Material_param_set
{
    // @NOTE: Use `in_primitive_idx` to offset from this
    //        index to access the material param buffer.
    uint material_param_buffer_start_idx;
};

layout(std140, set = 0, binding = 3) readonly buffer Material_param_sets_buffer
{
    Material_param_set sets[];
} material_param_sets_buffer;

// See `GPU_material_set`.
struct Material_param
{
    // @NOTE: This is used in the fragment shader.
    uint material_param_idx;
};

layout(std140, set = 0, binding = 4) readonly buffer Material_param_buffer
{
    Material_param params[];
} material_param_buffer;

// Helper functions.
uint get_material_param_idx()
{
    uint mat_param_set_idx =
        geo_instance_buffer
            .instances[gl_BaseInstance]
            .material_param_set_idx;
    uint mat_param_buffer_idx =
        material_param_sets_buffer
            .sets[mat_param_set_idx]
            .material_param_buffer_start_idx +
                in_primitive_idx;
    return material_param_buffer.params[mat_param_buffer_idx].material_param_idx;
}
