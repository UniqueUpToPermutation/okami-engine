$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    vec3 position;
    vec3 color;

    if (gl_VertexID == 0)
    {
        position = vec3(0.0f, sqrt(3.0) / 4.0f, 0.0f);
        color = vec3(1.0f, 0.0f, 0.0f); // Red vertex
    }
    else if (gl_VertexID == 1)
    {
        position = vec3(-0.5f, -sqrt(3.0) / 4.0f, 0.0f);
        color = vec3(0.0f, 0.0f, 1.0f); // Blue
    }
    else // gl_VertexID == 2
    {
        position = vec3(0.5f, -sqrt(3.0) / 4.0f, 0.0f);
        color = vec3(0.0f, 1.0f, 0.0f); // Green
    }

	gl_Position = mul(vec4(position, 1.0), u_modelViewProj);
    //gl_Position = vec4(position, 1.0);
	v_color0 = vec4(color, 1.0);
}