#pragma once

#include <cinttypes>
#include <vector>
#include "cglm/cglm.h"
#include "cglm/types-struct.h"


namespace camera
{

void set_aspect_ratio(uint32_t screen_width, uint32_t screen_height);
void set_fov(float_t radians);
void set_near_far(float_t near, float_t far);

void set_view(vec3 cam_position, float_t cam_pan__rot_y, float_t cam_tilt__rot_x);
void set_view_position(vec3 cam_position);
void set_view_direction(float_t cam_pan__rot_y, float_t cam_tilt__rot_x);
void set_view_direction_vec3(vec3 view_direction);

void fetch_matrices(mat4& out_projection,
                    mat4& out_view,
                    mat4& out_projection_view,
                    std::vector<mat4s>& out_shadow_cascades);

// @TODO: Add camera shake stuff.

}  // namespace camera
