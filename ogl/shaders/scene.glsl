#pragma once
#include "common.glsl"

#define MAX_LIGHTS 32

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
    vec4 u_directionalLightDirection;
    vec4 u_directionalLightColor;
    Light u_lights[MAX_LIGHTS];
};

struct SceneGlobals {
    CameraGlobals u_camera;
    LightingGlobals u_lighting;
};

#ifdef __cplusplus
} // namespace glsl
#endif