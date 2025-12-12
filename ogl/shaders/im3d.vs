#version 410 core

#include "im3d.glsl"
#include "scene.glsl"

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;  // Use the struct inside the block
};

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_color;

void main() {
    out_position = sceneGlobals.u_camera.u_viewProj * vec4(a_positionSize.xyz, 1.0);
    out_color = a_color.wzyx;

    gl_Position = out_position;
}
