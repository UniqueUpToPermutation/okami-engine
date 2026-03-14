#version 410 core

// Cascaded Shadow Map geometry shader.
// Receives each triangle once and emits it into every cascade layer of the
// shadow map array, so the entire scene is rasterised into all cascades in a
// single instanced draw call.

#include "scene.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out; // 3 verts * NUM_SHADOW_CASCADES(4)

in vec3 vs_worldPos[];

layout(std140) uniform CascadeBlock {
    mat4 u_cascadeViewProj[NUM_SHADOW_CASCADES];
};

void main() {
    for (int layer = 0; layer < NUM_SHADOW_CASCADES; ++layer) {
        for (int v = 0; v < 3; ++v) {
            gl_Layer    = layer;
            gl_Position = u_cascadeViewProj[layer] * vec4(vs_worldPos[v], 1.0);
            EmitVertex();
        }
        EndPrimitive();
    }
}
