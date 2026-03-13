#pragma once

#include "scene.glsl"

// ---------------------------------------------------------------------------
//  shadow.glsl
//
//  PCF shadow-map sampling utility.
//
//  Usage in a fragment shader:
//      #include "shadow.glsl"
//
//      layout(std140) uniform ShadowCameraBlock {
//          CameraGlobals shadowCamera;
//      };
//      uniform sampler2D u_shadowMap;
//      ...
//      float shadow = sampleShadow(u_shadowMap, shadowCamera, worldPos, N, L);
//      color *= shadow;
// ---------------------------------------------------------------------------

// Returns 1.0 = fully lit, 0.0 = fully in shadow.
// Applies a slope-scaled depth bias to reduce surface acne, then does a 3x3
// PCF kernel over the shadow map.
float sampleShadow(
    sampler2D     shadowMap,
    CameraGlobals shadowCam,
    SceneGlobals  scene,
    vec3          worldPos,
    vec3          N,
    vec3          L)
{
    vec4  shadowClip = shadowCam.u_viewProj * vec4(worldPos, 1.0);
    vec3  proj       = shadowClip.xyz / shadowClip.w;
    proj             = proj * 0.5 + 0.5;

    // Fragments outside the shadow frustum are considered fully lit.
    if (any(lessThan(proj, vec3(0.0))) || any(greaterThan(proj, vec3(1.0))))
        return 1.0;

    // Slope-scaled bias: bias grows as the surface becomes more grazing to the
    // light, which is exactly when acne is worst.
    //   bias = baseBias + slopeBias * tan(acos(nDotL))
    //        = baseBias + slopeBias * sqrt(1 - nDotL²) / nDotL
    // Clamp to a maximum to avoid excessive Peter-Panning on steep slopes.
    float cosTheta = clamp(dot(N, L), 0.0001, 1.0);
    float slope    = sqrt(1.0 - cosTheta * cosTheta) / cosTheta; // tan(acos)
    float bias     = clamp(
        scene.u_shadow.u_shadowBiasBase +
        scene.u_shadow.u_shadowBiasSlope * slope,
        0.0, scene.u_shadow.u_shadowBiasMax);

    float currentDepth = proj.z - bias;

    // 3x3 PCF
    float  shadow    = 0.0;
    vec2   texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closest = texture(shadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth > closest) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}
