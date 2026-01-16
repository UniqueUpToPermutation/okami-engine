#version 410 core

in VS_OUT {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
} fs_in;

uniform sampler2D u_diffuseMap;
uniform vec4 u_color;

out vec4 FragColor;

void main() {
    vec3 diffuseColor = texture(u_diffuseMap, fs_in.uv).rgb;
    FragColor = vec4(diffuseColor, 1.0) * u_color;
}