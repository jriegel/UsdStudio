#version 440 core

// Tonemap (ACES approx) + linear -> sRGB encode of the HDR Hydra color AOV.
// QRhi's Vulkan backbuffer is a plain UNORM target, so the sRGB encode has
// to happen here rather than in the format.

layout(location = 0) out vec4 fragColor;
layout(location = 0) in vec2 v_uv;

layout(binding = 1) uniform sampler2D screenTexture;

vec3 acesTonemap(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 encodeSRGB(vec3 c)
{
    vec3 lo = c * 12.92;
    vec3 hi = 1.055 * pow(max(c, vec3(1e-5)), vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), c));
}

void main()
{
    vec4 c = texture(screenTexture, v_uv);
    vec3 rgb = encodeSRGB(acesTonemap(max(c.rgb, vec3(0.0))));
    fragColor = vec4(rgb, 1.0);
}
