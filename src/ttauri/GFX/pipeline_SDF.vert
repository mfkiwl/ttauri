#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform push_constants {
    vec2 window_extent;
    vec2 viewport_scale;
    int subpixel_orientation;
} pushConstants;

// In position is in window pixel position, with left-bottom origin.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_clipping_rectangle;
layout(location = 2) in vec3 in_texture_coord;
layout(location = 3) in vec4 in_color;

layout(location = 0) out flat vec4 out_clipping_rectangle;
layout(location = 1) out vec3 out_texture_coord;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec4 out_color_sqrt;

#include "utils.glsl"

vec4 convert_position_to_viewport(vec3 window_position)
{
    float x = window_position.x * pushConstants.viewport_scale.x - 1.0;
    float y = (pushConstants.window_extent.y - window_position.y) * pushConstants.viewport_scale.y - 1.0;
    // Get reverse-z to work, where windows_position.z = 0.0 is far.
    return vec4(x, y, 1.0 - window_position.z * 0.01, 1.0);
}

vec4 convert_clipping_rectangle_to_screen(vec4 clipping_rectangle)
{
    return vec4(
        clipping_rectangle.x,
        pushConstants.window_extent.y - clipping_rectangle.w,
        clipping_rectangle.z,
        pushConstants.window_extent.y - clipping_rectangle.y
    );
}

void main() {
    gl_Position = convert_position_to_viewport(in_position);
    out_clipping_rectangle = convert_clipping_rectangle_to_screen(in_clipping_rectangle);
    out_texture_coord = in_texture_coord;

    // Pre-multiply the alpha. 
    vec4 color = vec4(in_color.rgb * in_color.a, in_color.a);

    float luminance = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;

    out_color = color;
    out_color_sqrt = sqrt(vec4(color.rgb, luminance));
}

