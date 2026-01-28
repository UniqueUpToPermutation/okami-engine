#version 410 core

#include "vs_outputs.glsl"

// Input from vertex shader
in MESH_VS_OUT vs_out;

// Output
layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(vs_out.normal * 0.5 + 0.5, 1.0);
}
