#pragma once

#include "scene.glsl"

// ---------------------------------------------------------------------------
//  shadow.glsl
//
//  Cascaded PCF shadow-map sampling utility.
//
//  Usage in a fragment shader:
//      #include "shadow.glsl"
//
//      uniform sampler2DArray u_shadowMap;
//      ...
//      float shadow = sampleShadowCSM(u_shadowMap, sceneGlobals, worldPos, N, L);
//      color *= shadow;
// ---------------------------------------------------------------------------

// Selects the tightest cascade whose far split is still beyond the fragment.
int selectCascade(SceneGlobals scene, vec3 worldPos) {
    float viewZ = abs((scene.u_camera.u_view * vec4(worldPos, 1.0)).z);
    for (int i = 0; i < NUM_SHADOW_CASCADES - 1; ++i) {
        if (viewZ < scene.u_shadow.u_cascadeSplits[i])
            return i;
    }
    return NUM_SHADOW_CASCADES - 1;
}

// Returns 1.0 = fully lit, 0.0 = fully in shadow.
// Picks the best cascade, applies a slope-scaled depth bias to reduce surface
// acne, then performs a 3x3 PCF kernel over that cascade layer.
float sampleShadowCSM(
    sampler2DArray shadowMap,
    SceneGlobals   scene,
    vec3           worldPos,
    vec3           N,
    vec3           L)
{
    int  cascade    = selectCascade(scene, worldPos);
    vec4 shadowClip = scene.u_shadow.u_cascadeViewProj[cascade] * vec4(worldPos, 1.0);
    vec3 proj       = shadowClip.xyz / shadowClip.w;
    proj            = proj * 0.5 + 0.5;

    // Fragments outside the shadow frustum are considered fully lit.
    if (any(lessThan(proj, vec3(0.0))) || any(greaterThan(proj, vec3(1.0))))
        return 1.0;

    // Slope-scaled bias: grows as the surface becomes more grazing to the light,
    // which is exactly when shadow acne is worst.
    float cosTheta = clamp(dot(N, L), 0.0001, 1.0);
    float slope    = sqrt(1.0 - cosTheta * cosTheta) / cosTheta;
    float bias     = clamp(
        scene.u_shadow.u_shadowBiasBase +
        scene.u_shadow.u_shadowBiasSlope * slope,
        0.0, scene.u_shadow.u_shadowBiasMax);

    float currentDepth = proj.z - bias;

    // 3x3 PCF kernel over the selected cascade layer.
    float shadow    = 0.0;
    vec2  texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closest = texture(shadowMap,
                vec3(proj.xy + vec2(x, y) * texelSize, float(cascade))).r;
            shadow += (currentDepth > closest) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}
