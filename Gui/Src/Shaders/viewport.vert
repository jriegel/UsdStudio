#version 440 core

// Fullscreen composite pass for the Hydra AOV: draws a single clip-space
// triangle (generated in render()) carrying uv in v[0..1], with v=0 at the
// texture top row (matching the AOV buffer's row order).

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 v_uv;

void main()
{
    v_uv = in_uv;
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
