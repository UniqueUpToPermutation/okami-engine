#version 410 core

layout(location = 0) in vec4 out_position;
layout(location = 1) in vec4 out_color;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = out_color; // Use the interpolated color from vertex shader
}