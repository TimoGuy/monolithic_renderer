#pragma once

#include <cinttypes>
#include <functional>
#include <vector>
#include "cglm/cglm.h"
#include "cglm/types-struct.h"


namespace camera
{

// Matches `geom_static_mesh_vert.glsl`.
struct GPU_camera
{
    mat4 view;
    mat4 projection_view;
};

void set_aspect_ratio(uint32_t screen_width, uint32_t screen_height);
void set_aspect_ratio_float(float_t aspect_ratio);
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

// Details for Imgui.
struct Imgui_requesting_data
{
    float_t aspect_ratio;
    float_t fov_deg;
    float_t near;
    float_t far;

    vec3 position;
    float_t pan_deg;
    float_t tilt_deg;
};
Imgui_requesting_data get_imgui_data();
void set_imgui_data(Imgui_requesting_data&& changed_data);

}  // namespace camera


// @TODO: @REFACTOR: Look into moving the camera system or information into its
//   own section. Or make just the virtual camera rig public but the actual
//   rendering camera private.  -Thea 2025/04/16
// @AMEND: Perhaps this could also be some kind of a class that acts as a decorator or smth?  -Thea 2025/04/16
namespace camera_rig
{

enum Camera_rig_type : size_t
{
    CAMERA_RIG_TYPE_INVALID = (size_t)-1,
    CAMERA_RIG_TYPE_FIXED = 0,  // No camera movement, fully manual view setting.
    CAMERA_RIG_TYPE_ORBIT,      // Focuses on a certain position and calculates view.
    CAMERA_RIG_TYPE_FREECAM,    // Fully unlocked by just pressing right click for user.
    NUM_VALID_CAMERA_RIG_TYPES
};

void initialize(Camera_rig_type initial_type,
                std::function<void()>&& lock_cursor_to_window_callback,
                std::function<void()>&& unlock_cursor_from_window_callback);

void set_camera_rig_type(Camera_rig_type type);

void update(float_t delta_time);

}  // namespace camera_rig
