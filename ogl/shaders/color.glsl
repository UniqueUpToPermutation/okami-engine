#pragma once

// ---------------------------------------------------------------------------
//  color.glsl
//
//  sRGB <-> linear conversion helpers.
//
//  All lighting must be done in linear light.  Albedo textures are almost
//  always authored in sRGB, so they must be decoded before shading and
//  the final result re-encoded for display.
//
//  Two variants are provided:
//    - Accurate  : piecewise IEC 61966-2-1 (the actual sRGB standard)
//    - Fast      : gamma 2.2 power approximation (~10× cheaper on old HW,
//                  <1% perceptual error for values in [0.04, 1])
//
//  Prefer the accurate versions unless profiling shows a bottleneck.
//
//  Note: if your textures are uploaded as GL_SRGB8 / GL_SRGB8_ALPHA8 the GPU
//  decodes them automatically and sRGBToLinear() is not needed for samplers.
//  Likewise, enabling GL_FRAMEBUFFER_SRGB makes the driver encode the output
//  automatically, removing the need for linearToSRGB() on the final write.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Accurate (IEC 61966-2-1)
// ---------------------------------------------------------------------------

float sRGBToLinear(float c) {
    return c <= 0.04045
        ? c / 12.92
        : pow((c + 0.055) / 1.055, 2.4);
}

vec3 sRGBToLinear(vec3 c) {
    return vec3(sRGBToLinear(c.r), sRGBToLinear(c.g), sRGBToLinear(c.b));
}

vec4 sRGBToLinear(vec4 c) {
    return vec4(sRGBToLinear(c.rgb), c.a);
}

float linearToSRGB(float c) {
    c = clamp(c, 0.0, 1.0);
    return c <= 0.0031308
        ? c * 12.92
        : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

vec3 linearToSRGB(vec3 c) {
    return vec3(linearToSRGB(c.r), linearToSRGB(c.g), linearToSRGB(c.b));
}

vec4 linearToSRGB(vec4 c) {
    return vec4(linearToSRGB(c.rgb), c.a);
}

// ---------------------------------------------------------------------------
//  Fast approximation (gamma 2.2)
// ---------------------------------------------------------------------------

vec3 sRGBToLinearFast(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(2.2));
}

vec3 linearToSRGBFast(vec3 c) {
    return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}
