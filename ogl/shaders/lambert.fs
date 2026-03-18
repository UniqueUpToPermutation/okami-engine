#version 410 core

#include "vs_outputs.glsl"
#include "scene.glsl"
#include "color.glsl"
#include "tonemapping.glsl"
#include "stochastic_transparency.glsl"
#include "lights.glsl"
#include "normal.glsl"
#include "shadow.glsl"

// Input from vertex shader
in MESH_VS_OUT vs_out;

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;
};

uniform sampler2D u_diffuseMap;

// Unit 2: shadow depth map array (one layer per CSM cascade).
uniform sampler2DArray u_shadowMap;

out vec4 FragColor;

void main() {
    vec4 diffuseSample = texture(u_diffuseMap, vs_out.uv);

    // Coverage-preserved alpha: counteracts the alpha-averaging that occurs
    // at higher mip levels, preventing distant geometry from looking too
    // transparent.
    float alpha = alphaCoveragePreserved(u_diffuseMap, vs_out.uv, 0.5);

    // Stochastic transparency: discard fragments based on the corrected alpha.
    stochasticDiscardIGN(alpha, vs_out.position);

    // Decode sRGB albedo to linear light before shading.
    vec3 albedo = sRGBToLinear(diffuseSample.rgb);

    vec3 N = sampleNormalMap(vs_out.uv, vs_out.normal, vs_out.tangent);

    // Accumulate ambient and direct contributions separately so debug views
    // can inspect them individually.
    vec3  ambientLight = sceneGlobals.u_lighting.u_ambientLightColor.rgb;
    vec3  directLight  = vec3(0.0);
    float shadowFactor = 1.0; // shadow value for the first directional light

    uint lightCount = sceneGlobals.u_lighting.u_lightCount.x;
    for (uint i = 0u; i < lightCount; ++i) {
        LightIncidence inc  = getLightIncidence(sceneGlobals.u_lighting.u_lights[i], vs_out.position);
        float          nDotL = max(dot(N, inc.direction), 0.0);

        // Apply PCF shadow test to directional lights.
        float shadow = 1.0;
        if (sceneGlobals.u_lighting.u_lights[i].u_type.x == uint(DIRECTIONAL_LIGHT)) {
            shadow = sampleShadowCSM(u_shadowMap, sceneGlobals, vs_out.position, N, inc.direction);
            shadowFactor = shadow;
        }

        directLight += inc.radiance * nDotL * shadow;
    }

    vec3 color = albedo * (ambientLight + directLight);

    // Debug visualization modes
    uint debugMode = sceneGlobals.u_debug.x;
    if (debugMode == DEBUG_MODE_ALBEDO) {
        // Raw albedo in sRGB
        FragColor = vec4(linearToSRGB(albedo), 1.0);
    } else if (debugMode == DEBUG_MODE_NORMAL) {
        // World-space normal remapped from [-1, 1] to [0, 1]
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else if (debugMode == DEBUG_MODE_LIGHTING) {
        // Lighting contribution without albedo (tonemapped)
        FragColor = vec4(linearToSRGB(applyTonemap(ambientLight + directLight, sceneGlobals.u_tonemap)), 1.0);
    } else if (debugMode == DEBUG_MODE_SHADOW) {
        // Shadow factor: white = fully lit, black = fully shadowed
        FragColor = vec4(vec3(shadowFactor), 1.0);
    } else {
        // Full render (DEBUG_MODE_NONE)
        FragColor = vec4(linearToSRGB(applyTonemap(color, sceneGlobals.u_tonemap)), 1.0);
    }
}
