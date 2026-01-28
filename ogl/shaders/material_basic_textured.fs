#version 410 core

#include "vs_outputs.glsl"

// Input from vertex shader
in MESH_VS_OUT vs_out;

uniform sampler2D u_diffuseMap;

out vec4 FragColor;

void main() {
    vec3 diffuseColor = texture(u_diffuseMap, vs_out.uv).rgb;
    FragColor = vec4(diffuseColor, 1.0);
}