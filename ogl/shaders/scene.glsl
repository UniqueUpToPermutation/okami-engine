#pragma once
#include "common.glsl"

#define MAX_LIGHTS 4
#define NUM_SHADOW_CASCADES 4

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#ifdef __cplusplus
namespace glsl {
#endif

struct CameraGlobals {
    mat4 u_view;
    mat4 u_proj;
    mat4 u_viewProj;
    mat4 u_invView;
    mat4 u_invProj;
    mat4 u_invViewProj;

    vec4 u_viewport;
    vec4 u_cameraPosition;
    vec4 u_cameraDirection;
};

struct Light {
    vec4 u_direction;
    vec4 u_positionRadius;
    vec4 u_color;
    uvec4 u_type;
};

struct LightingGlobals {
    vec4 u_ambientLightColor;
    uvec4 u_lightCount;         // .x = number of active lights in u_lights
    Light u_lights[MAX_LIGHTS];
};

struct TonemapGlobals {
    vec4 u_tonemapABCD;         // (shoulder, linearStr, linearAngle, toeStr)
    vec4 u_tonemapEFExposureW;  // (toeNum, toeDenom, exposure, whitePoint)
};

struct ShadowGlobals {
    float u_shadowBiasBase;   // constant depth bias
    float u_shadowBiasSlope;  // slope-scaled bias multiplier
    float u_shadowBiasMax;    // clamp ceiling to prevent Peter-Panning
    float u_shadowPad;        // padding to 16-byte alignment
    mat4  u_cascadeViewProj[NUM_SHADOW_CASCADES]; // light-space VP per cascade
    vec4  u_cascadeSplits;    // view-space far Z boundary of each cascade (positive)
};

// UBO block used exclusively by the depth-pass geometry shader.
// Contains one VP matrix per cascade so a single draw call can render all layers.
struct ShadowCascadesBlock {
    mat4 u_cascadeViewProj[NUM_SHADOW_CASCADES];
};

struct SceneGlobals {
    CameraGlobals   u_camera;
    LightingGlobals u_lighting;
    TonemapGlobals  u_tonemap;
    ShadowGlobals   u_shadow;
};

#ifdef __cplusplus
} // namespace glsl
#endif