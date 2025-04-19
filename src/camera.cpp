#include "camera.h"

// For camera_rig namespace.
#include <array>
#include <GLFW/glfw3.h>
#include <iostream>
#include "input_handling_public.h"
////////////////////////////
#include <atomic>
using atomic_bool = std::atomic_bool;

namespace camera
{

static mat4 s_calculated_projection_matrix;
static atomic_bool s_projection_cache_invalid{ true };
static mat4 s_calculated_view_matrix;
static atomic_bool s_view_cache_invalid{ true };
static mat4 s_calculated_projection_view_matrix;
static std::vector<mat4s> s_calculated_shadow_cascade_matrices;

static float_t s_aspect_ratio{ 1.0f };
static float_t s_fov{ glm_rad(90.0f) };
static float_t s_z_near{ 0.1f };
static float_t s_z_far{ 100.0f };
static vec3 s_cam_position{ 0.0f, 0.0f, 0.0f };
static vec3 s_cam_rot_axes{ 0.0f, 0.0f, 0.0f };
static vec3 s_cam_view_direction{ 0.0f, 0.0f, 1.0f };

void internal__set_view_direction_and_rotation_axes(vec3 view_direction,
                                                    float_t cam_pan__rot_y,
                                                    float_t cam_tilt__rot_x,
                                                    float_t cam_roll__rot_z);

}  // namespace camera


void camera::set_aspect_ratio(uint32_t screen_width, uint32_t screen_height)
{
    set_aspect_ratio_float(
        static_cast<float_t>(screen_width) / static_cast<float_t>(screen_height));
}

void camera::set_aspect_ratio_float(float_t aspect_ratio)
{
    s_aspect_ratio = aspect_ratio;
    s_projection_cache_invalid = true;
}

void camera::set_fov(float_t radians)
{
    s_fov = radians;
    s_projection_cache_invalid = true;
}

void camera::set_near_far(float_t near, float_t far)
{
    s_z_near = near;
    s_z_far = far;
    s_projection_cache_invalid = true;
}

void camera::set_view(vec3 cam_position, float_t cam_pan__rot_y, float_t cam_tilt__rot_x)
{
    set_view_position(cam_position);
    set_view_direction(cam_pan__rot_y, cam_tilt__rot_x);
}

void camera::set_view_position(vec3 cam_position)
{
    glm_vec3_copy(cam_position, s_cam_position);
    s_view_cache_invalid = true;
}

void camera::set_view_direction(float_t cam_pan__rot_y, float_t cam_tilt__rot_x)
{
    vec3 view_direction{ 0.0f, 0.0f, 1.0f };
    mat4 cam_rotation;
    glm_euler_zyx(vec3{ cam_tilt__rot_x, cam_pan__rot_y, 0.0f }, cam_rotation);
    glm_mat4_mulv3(cam_rotation, view_direction, 0.0f, view_direction);
    internal__set_view_direction_and_rotation_axes(view_direction,
                                                   cam_pan__rot_y,
                                                   cam_tilt__rot_x,
                                                   s_cam_rot_axes[2]);
}

void camera::set_view_direction_vec3(vec3 view_direction)
{
    float_t cam_pan__rot_y{
        -atan2f(view_direction[1],
                glm_vec2_norm(vec2{
                    view_direction[0],
                    view_direction[2]
                })) };
    float_t cam_tilt__rot_x{ atan2f(view_direction[0], view_direction[2]) };
    internal__set_view_direction_and_rotation_axes(view_direction,
                                                   cam_pan__rot_y,
                                                   cam_tilt__rot_x,
                                                   s_cam_rot_axes[2]);
}

void camera::fetch_matrices(mat4& out_projection,
                            mat4& out_view,
                            mat4& out_projection_view,
                            std::vector<mat4s>& out_shadow_cascades)
{
    bool recalc_proj_view_matrix_and_shadows{ false };
    if (s_projection_cache_invalid)
    {
        // Calculate projection matrix.
        glm_perspective(s_fov,
                        s_aspect_ratio,
                        s_z_near,
                        s_z_far,
                        s_calculated_projection_matrix);
        s_calculated_projection_matrix[1][1] *= -1.0f;

        s_projection_cache_invalid = false;
        recalc_proj_view_matrix_and_shadows = true;
    }

    if (s_view_cache_invalid)
    {
        // Calculate view matrix.
        vec3 up{ 0.0f, 1.0f, 0.0f };
        if (std::abs(s_cam_view_direction[0]) < 1e-6f &&
            std::abs(s_cam_view_direction[1]) > 1e-6f &&
            std::abs(s_cam_view_direction[2]) < 1e-6f)
        {
            glm_vec3_copy(vec3{ 0.0f, 0.0f, 1.0f }, up);
        }

        vec3 center;
        glm_vec3_add(s_cam_position, s_cam_view_direction, center);
        glm_lookat(s_cam_position, center, up, s_calculated_view_matrix);

        s_view_cache_invalid = false;
        recalc_proj_view_matrix_and_shadows = true;
    }

    if (recalc_proj_view_matrix_and_shadows)
    {
        // Calculate projection view matrix.
        glm_mat4_mul(s_calculated_projection_matrix,
                     s_calculated_view_matrix,
                     s_calculated_projection_view_matrix);

        // Calculate shadow cascades.
        s_calculated_shadow_cascade_matrices.clear();
        // for (uint32_t i = 0; i < s_num_shadow_cascades; i++)
        // {
        //     // @TODO
        // }
    }

    glm_mat4_copy(s_calculated_projection_matrix, out_projection);
    glm_mat4_copy(s_calculated_view_matrix, out_view);
    glm_mat4_copy(s_calculated_projection_view_matrix, out_projection_view);
    out_shadow_cascades = s_calculated_shadow_cascade_matrices;
}

// Details for Imgui.
camera::Imgui_requesting_data camera::get_imgui_data()
{
    Imgui_requesting_data data{
        .aspect_ratio = s_aspect_ratio,
        .fov_deg = glm_deg(s_fov),
        .near = s_z_near,
        .far = s_z_far,

        // .position = ,
        .pan_deg = glm_deg(s_cam_rot_axes[1]),
        .tilt_deg = glm_deg(s_cam_rot_axes[0]),
    };
    glm_vec3_copy(s_cam_position, data.position);

    return data;
}

void camera::set_imgui_data(Imgui_requesting_data&& changed_data)
{
    set_aspect_ratio_float(changed_data.aspect_ratio);
    set_fov(glm_rad(changed_data.fov_deg));
    set_near_far(changed_data.near, changed_data.far);
    set_view(changed_data.position,
             glm_rad(changed_data.pan_deg),
             glm_rad(changed_data.tilt_deg));
}

// Internal functions.
void camera::internal__set_view_direction_and_rotation_axes(vec3 view_direction,
                                                            float_t cam_pan__rot_y,
                                                            float_t cam_tilt__rot_x,
                                                            float_t cam_roll__rot_z)
{
#if _DEBUG
    // Assert that already normalized.
    float_t norm2_deviation{ std::abs(glm_vec3_norm2(view_direction) - 1.0f) };
    assert(norm2_deviation < 1e-6f);
#endif
    glm_vec3_copy(view_direction, s_cam_view_direction);
    s_cam_rot_axes[0] = cam_tilt__rot_x;
    s_cam_rot_axes[1] = cam_pan__rot_y;
    s_cam_rot_axes[2] = cam_roll__rot_z;

    s_view_cache_invalid = true;
}


// Camera rig.
// @TODO: See `camera.h`
namespace camera_rig
{

static std::atomic_bool s_initialized{ false };
static Camera_rig_type s_current_type{ Camera_rig_type::CAMERA_RIG_TYPE_INVALID };
static std::function<void()> s_lock_cursor_to_window_callback;
static std::function<void()> s_unlock_cursor_from_window_callback;

struct Event_handler_set
{
    void (*enter_fn)();
    void (*exit_fn)();
    void (*update_fn)(float_t);
};
static std::array<Event_handler_set, NUM_VALID_CAMERA_RIG_TYPES> s_event_handlers;

// Event handlers.
// `CAMERA_RIG_TYPE_FIXED`
void fixed_enter()
{
    assert(false);
}

void fixed_exit()
{
    assert(false);
}

void fixed_update(float_t delta_time)
{
    // @TODO: Get requested view and update the view to it.
    assert(false);
}

// `CAMERA_RIG_TYPE_ORBIT`
void orbit_enter()
{
    assert(false);
}

void orbit_exit()
{
    assert(false);
}

void orbit_update(float_t delta_time)
{
    // @TODO: Get requested view and update the view to it.
    assert(false);
}

// `CAMERA_RIG_TYPE_FREECAM`
namespace camera_rig_type_freecam
{
    vec3 position;
    vec3 view_direction;
    bool prev_camera_move;
    bool is_cursor_captured;  // @NOTE: Really only on desktop.

    // Settings.
    float_t sensitivity{ 0.1f };
    float_t speed{ 20.0f };
}

void freecam_enter()
{
    // Read from camera.
    glm_vec3_copy(camera::s_cam_position, camera_rig_type_freecam::position);
    glm_vec3_copy(camera::s_cam_view_direction,
                  camera_rig_type_freecam::view_direction);
    camera_rig_type_freecam::prev_camera_move = false;
    camera_rig_type_freecam::is_cursor_captured = false;
}

void freecam_exit()
{
    assert(false);
}

void freecam_update(float_t delta_time)
{
    auto& ihle{ input_handling::get_state_set_reading_handle(0).level_editor };
    if (ihle.camera_move && !camera_rig_type_freecam::prev_camera_move)
    {
        // Rising edge.
        s_lock_cursor_to_window_callback();
        camera_rig_type_freecam::is_cursor_captured = true;
    }
    else if (!ihle.camera_move && camera_rig_type_freecam::prev_camera_move)
    {
        // Falling edge.
        s_unlock_cursor_from_window_callback();
        camera_rig_type_freecam::is_cursor_captured = false;
    }
    camera_rig_type_freecam::prev_camera_move = ihle.camera_move;

    if (ihle.camera_move)
    {
        // Move camera with camera delta.
        vec2 cooked_cam_delta;
        glm_vec2_scale(const_cast<float_t*>(ihle.camera_delta),
                       camera_rig_type_freecam::sensitivity,
                       cooked_cam_delta);

        vec3 world_up{ 0.0f, 1.0f, 0.0f };
        vec3 world_down{ 0.0f, -1.0f, 0.0f };

        // Update camera view direction with input.
        vec3 facing_direction_right;
        glm_cross(camera_rig_type_freecam::view_direction,
                  world_up,
                  facing_direction_right);
        glm_normalize(facing_direction_right);

        mat4 rotation = GLM_MAT4_IDENTITY_INIT;
        glm_rotate(rotation, glm_rad(-cooked_cam_delta[1]), facing_direction_right);

        vec3 new_view_direction;
        glm_mat4_mulv3(rotation,
                       camera_rig_type_freecam::view_direction,
                       0.0f,
                       new_view_direction);

        if (glm_vec3_angle(new_view_direction, world_up) > glm_rad(5.0f) &&
            glm_vec3_angle(new_view_direction, world_down) > glm_rad(5.0f))
        {
            glm_vec3_copy(new_view_direction, camera_rig_type_freecam::view_direction);
        }

        glm_mat4_identity(rotation);
        glm_rotate(rotation, glm_rad(-cooked_cam_delta[0]), world_up);
        glm_mat4_mulv3(rotation,
                       camera_rig_type_freecam::view_direction,
                       0.0f,
                       camera_rig_type_freecam::view_direction);

        // @NOTE: Need a normalization step at the end from float inaccuracy over time.
        glm_vec3_normalize(camera_rig_type_freecam::view_direction);

        // Apply new view to camera.
        camera::set_view_direction_vec3(camera_rig_type_freecam::view_direction);

        vec2 cooked_mvt;
        glm_vec2_scale(const_cast<float_t*>(ihle.movement),
                       camera_rig_type_freecam::speed * delta_time,
                       cooked_mvt);

        glm_vec3_muladds(camera_rig_type_freecam::view_direction,
                         cooked_mvt[1],
                         camera_rig_type_freecam::position);
        glm_vec3_muladds(facing_direction_right,
                         cooked_mvt[0],
                         camera_rig_type_freecam::position);

        // Update camera position with input.
        camera_rig_type_freecam::position[1] +=
            ihle.move_world_y_axis * camera_rig_type_freecam::speed * delta_time;

        camera::set_view_position(camera_rig_type_freecam::position);
    }
}

}  // namespace camera_rig


void camera_rig::initialize(Camera_rig_type initial_type,
                            std::function<void()>&& lock_cursor_to_window_callback,
                            std::function<void()>&& unlock_cursor_from_window_callback)
{
    assert(!s_initialized);

    s_event_handlers[CAMERA_RIG_TYPE_FIXED] = {
        .enter_fn = fixed_enter,
        .exit_fn = fixed_exit,
        .update_fn = fixed_update,
    };
    s_event_handlers[CAMERA_RIG_TYPE_ORBIT] = {
        .enter_fn = orbit_enter,
        .exit_fn = orbit_exit,
        .update_fn = orbit_update,
    };
    s_event_handlers[CAMERA_RIG_TYPE_FREECAM] = {
        .enter_fn = freecam_enter,
        .exit_fn = freecam_exit,
        .update_fn = freecam_update,
    };

    s_initialized = true;

    // Set initial camera rig type.
    assert(initial_type != CAMERA_RIG_TYPE_INVALID);
    set_camera_rig_type(initial_type);

    // Set callbacks.
    s_lock_cursor_to_window_callback = std::move(lock_cursor_to_window_callback);
    s_unlock_cursor_from_window_callback = std::move(unlock_cursor_from_window_callback);
}

void camera_rig::set_camera_rig_type(Camera_rig_type type)
{
    assert(s_initialized);

    // Execute exit function.
    if (s_current_type != CAMERA_RIG_TYPE_INVALID)
    {
        s_event_handlers[s_current_type].exit_fn();
    }

    s_current_type = type;

    // Execute enter function.
    s_event_handlers[s_current_type].enter_fn();
}

void camera_rig::update(float_t delta_time)
{
    assert(s_initialized);
    s_event_handlers[s_current_type].update_fn(delta_time);
}
