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
