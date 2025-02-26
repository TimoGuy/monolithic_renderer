















-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

@TODO: okay so figuring out the geom_culling.comp shader is what you need to do next.
    - @REPLY: okay so just essentially copying the input->output draw indirect command buffers scheme as before works! I'll just do that.

@THOUGHT: honestly having a buffer and a list of descriptor sets that connect to the buffer would be nice, so when the buffer resizes the descriptor set gets reassigned to the buffer at the same time.



- @TODO: Do some research into buffer device addresses (vulkan 1.3 core), and see where it can be used. Also, is using it faster than going thru native vertex input pipeline????? (https://vkguide.dev/docs/new_chapter_3/mesh_buffers/)

- Split the culling into two compute calls:
    - Get the instance-level culling done.
        - Visibility layers (bitwise AND with the visibility layer marked in the uint)
        - Frustum (just do return true for now).
        - Occlusion (just do return true for now).
        - Write the result into an instance-level buffer.
    - Write the primitive-level indirect draw calls.
        - Retrieve the culling result of its parent instance.
        - If visible, then write the indirect draw command to the output and incrementing the draw count.
            - Essentially just boiling down the `is_visible()` func to just look up the result in the culling results.

    OKAY: so I split them into two compute shaders:
    - `geom_culling.comp`: instance-level culling write to visible result buffer.
    - `geom_write_draw_cmds.comp`: primitive-level lookup instance visibility lookup and write draw commands.


Render steps:
1. Shadow Cascades
    For each cascade:
    1. `geom_culling.comp`
    1. `geom_write_draw_cmds.comp` -> cascade_cmds
    1. `geom_depth_only.vert/.frag(cascade_cmds)` for only one material.
1. Main camera
    1. `geom_culling.comp`
    1. `geom_write_draw_cmds.comp` -> opaque_z_prepass_cmds
    1. `geom_depth_only.vert/.frag(opaque_z_prepass_cmds)` for only one material for everything (Opaque Z prepass).
    1. `geom_write_draw_cmds.comp` -> per_material_cmds
    1. `geommat_XXX(opaque-type).vert/.frag(per_material_cmds)` for each opaque material.

    1. `geom_write_draw_cmds.comp` -> water_z_prepass_cmds
    1. `geom_depth_only.vert/.frag` for only one material for everything (Water Z prepass).
    1. `geom_extract_water_refraction.comp` -> water_refracted_texture
    1. `geommat_XXX(water-type).vert/.frag(per_material_cmds)` for each water material.

    1. `geommat_XXX(transparent-type).vert/.frag(DEPTH_ORDERED_DIRECT_DRAW)` for each transparent material.
        > @NOTE: likely this will have to be directly drawn to make sure that there is the transparency order preserved. If you kept the ngnb
        > @IDEA: have only one transparent shader. Have it be an uber shader. Sort all the instances in the order that they come in (might actually have to be primitive level) back to front and then do the same culling and rendering process. 








I think that material params can be used in the uniform buffer if the max number of material params is set to 128 or so. Looking at `struct MaterialParam` from the old solanine_vulkan pbr shader, (wo padding) the struct takes 76 bytes of memory, so best case scenario it would be able to have ~215 structs inside the 16kb of guaranteed uniform buffer memory.
    - Is this limit feasible? Especially in the case of doing something like a pbr-only game.
    - I don't think it is. I think that material collections should be general storage buffers.

============
@THOUGHT: I think that just having a map that points to a corresponding set of subscribed descriptor sets would be really good! And then just notifying everything there when the buffer resizes using the old buffer's pointer as the key would be really good!
    - I think that it would be just a tad more complicated bc there would be a fixed way of doing the descriptor set writes ig. That would be fiiiiine but I feel like it'll require a bit more wait-and-see to see how I wanna do the descriptor set stuff.
=============

And then after that is doing the whole first scene drawing!!
How exciting!

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-