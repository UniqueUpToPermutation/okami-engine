#pragma once
#include "scene.glsl"
#include "color.glsl"
#include "tonemapping.glsl"

// Returns the fragment color for the active debug visualization mode.
// Falls through to the full render when debugMode == DEBUG_MODE_NONE.
vec4 debugFragColor(
    uint           debugMode,
    vec3           albedo,
    vec3           N,
    vec3           ambientLight,
    vec3           directLight,
    float          shadowFactor,
    vec3           color,
    TonemapGlobals tonemap
) {
    if (debugMode == DEBUG_MODE_ALBEDO) {
        // Raw albedo in sRGB
        return vec4(linearToSRGB(albedo), 1.0);
    } else if (debugMode == DEBUG_MODE_NORMAL) {
        // World-space normal remapped from [-1, 1] to [0, 1]
        return vec4(N * 0.5 + 0.5, 1.0);
    } else if (debugMode == DEBUG_MODE_LIGHTING) {
        // Lighting contribution without albedo (tonemapped)
        return vec4(linearToSRGB(applyTonemap(ambientLight + directLight, tonemap)), 1.0);
    } else if (debugMode == DEBUG_MODE_SHADOW) {
        // Shadow factor: white = fully lit, black = fully shadowed
        return vec4(vec3(shadowFactor), 1.0);
    } else {
        // Full render (DEBUG_MODE_NONE)
        return vec4(linearToSRGB(applyTonemap(color, tonemap)), 1.0);
    }
}
