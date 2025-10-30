#version 410 core

#include "sprite.glsl"

// Output to geometry shader
out VS_OUT {
    vec3 position;
    float rotation;
    vec4 spriteRect;
    vec4 color;
    vec2 size;
} vs_out;

void main() {
    // Pass all sprite instance data to geometry shader
    vs_out.position = a_position;
    vs_out.rotation = a_rotation;
    vs_out.spriteRect = a_spriteRect;
    vs_out.color = a_color;
    vs_out.size = a_size;
    
    // Output the point at the sprite's center position
    gl_Position = vec4(a_position, 1.0);
}
