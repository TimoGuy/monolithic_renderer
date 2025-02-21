#include "camera.h"

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
    s_aspect_ratio =
        static_cast<float_t>(screen_width) / static_cast<float_t>(screen_height);
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

// Internal functions.
void camera::internal__set_view_direction_and_rotation_axes(vec3 view_direction,
                                                            float_t cam_pan__rot_y,
                                                            float_t cam_tilt__rot_x,
                                                            float_t cam_roll__rot_z)
{
    assert(std::abs(glm_vec3_norm2(view_direction) - 1.0f) < 1e-6f);  // Assert that already normalized.
    glm_vec3_copy(view_direction, s_cam_view_direction);
    s_cam_rot_axes[0] = cam_tilt__rot_x;
    s_cam_rot_axes[1] = cam_pan__rot_y;
    s_cam_rot_axes[2] = cam_roll__rot_z;

    s_view_cache_invalid = true;
}
