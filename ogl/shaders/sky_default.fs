#version 330 core
#include "vs_outputs.glsl"

in SKY_VS_OUT sky_vs_out;

void main()
{
	gl_FragColor = vec4(normalize(sky_vs_out.direction) * 0.5 + 0.5, 1.0);
}