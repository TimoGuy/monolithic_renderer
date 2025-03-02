// Helper functions for material sets.
uint get_material_param_idx()
{
    uint mat_param_set_idx =
        params.geo_instance_buffer
            .instances[gl_BaseInstance]
            .material_param_set_idx;
    uint mat_param_buffer_idx =
        material_param_sets_buffer
            .sets[mat_param_set_idx]
            .material_param_buffer_start_idx +
                in_primitive_idx;
    return material_param_buffer.params[mat_param_buffer_idx].material_param_idx;
}
