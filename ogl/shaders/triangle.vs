#version 410 core

uniform mat4 u_world;
uniform mat4 u_viewProj;

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_color;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.0,  0.5)
    );  

    vec4 colors[3] = vec4[](
        vec4(1.0, 0.0, 0.0, 1.0),
        vec4(0.0, 1.0, 0.0, 1.0),
        vec4(0.0, 0.0, 1.0, 1.0)
    );

    out_position = u_viewProj * u_world * vec4(positions[gl_VertexID], 0.0, 1.0);
    out_color = colors[gl_VertexID];

    gl_Position = out_position;
}