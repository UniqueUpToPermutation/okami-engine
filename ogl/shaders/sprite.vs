#version 410 core

#include "sprite.glsl"

uniform mat4 u_viewProj;

// Output to fragment shader
out vec2 v_texCoord;
out vec4 v_color;

void main() {
    vec2 positions[4] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2(-0.5,  0.5),
        vec2( 0.5,  0.5)
    );

    vec2 uvs[4] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );

    int idx = gl_VertexID % 4;

    // Pass sprite data to fragment shader
    //v_texCoord = a_spriteRect.xy + uvs[idx] * a_spriteRect.zw;
    v_texCoord = uvs[idx];
    v_color = a_color;

    gl_Position = u_viewProj * vec4(positions[idx], 0.0, 1.0);
}
