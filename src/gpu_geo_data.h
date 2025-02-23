#pragma once

#include <vector>
#include "cglm/cglm.h"


namespace gpu_geo_data
{

enum class Render_layer : uint32_t
{
    DEFAULT = 0,
    INVISIBLE,
    LEVEL_EDITOR,
    NUM_RENDER_LAYERS
};

struct GPU_geo_instance_data
{
    mat4 transform = GLM_MAT4_IDENTITY_INIT;
    uint32_t bounding_sphere_idx;
    uint32_t material_param_set_idx;
    uint32_t render_layer;
    uint32_t never_cull;
};

struct GPU_bounding_sphere
{
    vec4 origin_xyz_radius_w;
};

// Have compute shader compute culling with all the bounding spheres write all the draw commands.
// First, calculate if an instance's bounding sphere is included in the draw calls.

// COMPUTE SHADER
// - For every instance, take model idx and compute if the model is culled or not (render layer, never_cull, and bounding sphere (in the future occlusion cull))
// - Write results in a buffer.
//
// SETUP FOR DRAW CALLS
// - Just have each object as its own draw command (with instancecount as 1) (with firstinstance as the instance id), separated by material, and inserted into a buffer the right size for all of it.
// - @NOTE: recreate the buffer if stale.
// 
// COMPUTE SHADER (for each shader)
// - Compacts draw calls by reading each instance and its corresponding culling result and writing the indirect draw command to another buffer and incrementing the count buffer for the material.
// - @TODO: honestly, this shader can be combined with the culling compute shader. That would eliminate the need to have a culling result buffer.
//
// DRAW COMMANDS (for each shader)
// - Provide the count buffer and indirect command buffer and run `vkCmdDrawIndexedIndirectCount` for each shader.
// - Things should still be aligned, since nothing in the instance and material param buffers would've gotten mutated.
//
// AND.... THAT'S A WRAP FOR OPAQUE GEO (no shadows).





}  // namespace gpu_geo_data