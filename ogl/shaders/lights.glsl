#pragma once

#include "scene.glsl"

// ---------------------------------------------------------------------------
//  lights.glsl
//
//  Utilities for evaluating the incoming radiance and direction from a Light
//  at any world-space position.
//
//  Usage:
//      LightIncidence inc = getLightIncidence(light, worldPos);
//      float nDotL = max(dot(N, inc.direction), 0.0);
//      vec3 reflected = albedo * inc.radiance * nDotL;   // Lambertian
// ---------------------------------------------------------------------------

// Incoming light at a surface point: radiance (W/m²) and the unit vector from
// the surface toward the light source.
struct LightIncidence {
    vec3 radiance;   // pre-attenuated radiance (light color * intensity * falloff)
    vec3 direction;  // unit vector from the surface point toward the light
};

// ---------------------------------------------------------------------------
//  Point-light attenuation
//
//  Uses the UE4 windowed inverse-square falloff so the light cleanly reaches
//  zero at its specified range with no hard cutoff.
//
//  Reference: "Real Shading in Unreal Engine 4", Karis, SIGGRAPH 2013.
// ---------------------------------------------------------------------------
float pointLightAttenuation(float dist, float range) {
    float d2 = dist * dist;
    float r2 = range * range;
    // Windowed falloff: smoothly fades to 0 at dist == range.
    float window = clamp(1.0 - pow(d2 / r2, 2.0), 0.0, 1.0);
    window *= window;
    // Inverse-square fall-off with a small bias to avoid division by zero.
    return window / max(d2, 0.0001);
}

// ---------------------------------------------------------------------------
//  getLightIncidence
//
//  Returns the incoming radiance and its direction at world-space position
//  worldPos for the given Light.  Handles POINT_LIGHT and DIRECTIONAL_LIGHT.
// ---------------------------------------------------------------------------
LightIncidence getLightIncidence(Light light, vec3 worldPos) {
    LightIncidence result;

    if (light.u_type.x == uint(POINT_LIGHT)) {
        vec3  delta      = light.u_positionRadius.xyz - worldPos;
        float dist       = length(delta);
        result.direction = delta / max(dist, 0.0001);

        float range      = light.u_positionRadius.w;
        float atten      = pointLightAttenuation(dist, range);
        result.radiance  = light.u_color.rgb * atten;
    } else {
        // Directional light: u_direction points *away* from the light source.
        result.direction = normalize(-light.u_direction.xyz);
        result.radiance  = light.u_color.rgb;
    }

    return result;
}
