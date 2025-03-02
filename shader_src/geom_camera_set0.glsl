// Matches `GPU_camera`.
layout(set = 0, binding = 0) uniform Camera_buffer
{
    mat4 view;
	mat4 projection_view;
} camera;
