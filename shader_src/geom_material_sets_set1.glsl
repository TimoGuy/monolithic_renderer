// See `GPU_material_set`.
struct Material_param_set
{
    // @NOTE: Use `in_primitive_idx` to offset from this
    //        index to access the material param buffer.
    uint material_param_buffer_start_idx;
};

layout(std140, set = 1, binding = 0) readonly buffer Material_param_sets_buffer
{
    Material_param_set sets[];
} material_param_sets_buffer;

// See `GPU_material_set`.
struct Material_param
{
    // @NOTE: This is used in the fragment shader.
    uint material_param_idx;
};

layout(std140, set = 1, binding = 1) readonly buffer Material_param_buffer
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
