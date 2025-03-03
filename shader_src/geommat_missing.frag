#version 460

layout (location = 0) in vec3 in_normal;
layout (location = 1) in flat uint in_material_param_idx;

layout (location = 0) out vec4 out_frag_color;


struct Material_param_definition
{
    vec4 color;
};

// @TODO: PUT THIS INSIDE OF A HELPER THINGY.
layout (std140, set = 2, binding = 0) readonly buffer Material_param_definitions_buffer
{
    Material_param_definition definitions[];
} material_param_definitions_buffer;

Material_param_definition get_material_param()
{
    return material_param_definitions_buffer
               .definitions[in_material_param_idx];
}
/////////////////////////////////////////////


void main()
{
    Material_param_definition material = get_material_param();
    out_frag_color = vec4(in_normal * material.color.rgb, 1.0);
}
