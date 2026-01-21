#version 410 core

// Input from vertex shader
in VS_OUT {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
} vs_out;

// Output
layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(vs_out.normal * 0.5 + 0.5, 1.0);
}
