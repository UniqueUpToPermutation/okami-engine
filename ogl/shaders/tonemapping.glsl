#pragma once
#include "scene.glsl"

// Uncharted 2 filmic tonemapping curve.
// Reference: http://filmicgames.com/archives/75
// Takes a linear HDR colour and the curve parameters from TonemapGlobals.
vec3 Uncharted2Curve(vec3 x, TonemapGlobals t) {
    float A = t.u_tonemapABCD.x;
    float B = t.u_tonemapABCD.y;
    float C = t.u_tonemapABCD.z;
    float D = t.u_tonemapABCD.w;
    float E = t.u_tonemapEFExposureW.x;
    float F = t.u_tonemapEFExposureW.y;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// Full Uncharted 2 tonemap: applies exposure, maps through the curve and
// normalises so that the white point maps to 1.
vec3 applyTonemap(vec3 color, TonemapGlobals t) {
    float exposure  = t.u_tonemapEFExposureW.z;
    float whitePoint = t.u_tonemapEFExposureW.w;
    vec3  mapped    = Uncharted2Curve(color * exposure, t);
    vec3  whiteScale = 1.0 / Uncharted2Curve(vec3(whitePoint), t);
    return mapped * whiteScale;
}