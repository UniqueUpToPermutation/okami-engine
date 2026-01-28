#version 330 core

#include "scene.glsl"
#include "vs_outputs.glsl"

out SKY_VS_OUT sky_vs_out;

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;  // Use the struct inside the block
};

const float kDepth = 1.0;

void main() {
    // Map gl_VertexID (0,1,2) to full-screen triangle positions
    // using a common pattern that covers the screen with one triangle
    vec3 positions[4] = vec3[4](
        vec3(-1.0, -1.0, kDepth), // lower-left
        vec3( 1.0, -1.0, kDepth), // lower-right beyond to cover
        vec3(-1.0,  1.0, kDepth),  // top-left beyond to cover
        vec3(1.0,  1.0, kDepth)  // top-left beyond to cover
    );

    vec3 pos = positions[gl_VertexID];
    gl_Position = vec4(pos, 1.0);

    // Compute UV in 0..1 from clip-space position
    sky_vs_out.uv = pos.xy * 0.5 + 0.5;

    // Compute direction vector from camera position through the pixel
    sky_vs_out.direction = (sceneGlobals.u_camera.u_invView * vec4(pos, 0.0)).xyz;
}
