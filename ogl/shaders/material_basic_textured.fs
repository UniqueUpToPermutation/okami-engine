#version 410 core

#include "vs_outputs.glsl"
#include "stochastic_transparency.glsl"

// Input from vertex shader
in MESH_VS_OUT vs_out;

uniform sampler2D u_diffuseMap;

out vec4 FragColor;

void main() {
    vec3 diffuseRGB = texture(u_diffuseMap, vs_out.uv).rgb;

    // Coverage-preserved alpha: counteracts the alpha-averaging that occurs
    // at higher mip levels, preventing distant geometry from looking too
    // transparent.  Adjust referenceAlpha to match the texture's midpoint.
    float alpha = alphaCoveragePreserved(u_diffuseMap, vs_out.uv, 0.5);

    // Stochastic transparency: discard fragments based on the corrected alpha.
    stochasticDiscardIGN(alpha, gl_FragCoord.xy);

    FragColor = vec4(diffuseRGB, 1.0);
}