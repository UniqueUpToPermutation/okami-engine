#pragma once
#include "common.glsl"

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

    vec2 u_viewport;
    vec3 u_cameraPosition;
    vec3 u_cameraDirection;
};

struct SceneGlobals {
    CameraGlobals u_camera;
};

#ifdef __cplusplus
} // namespace glsl
#endif