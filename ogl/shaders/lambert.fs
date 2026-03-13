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

layout(std140) uniform ShadowCameraBlock {
    CameraGlobals shadowCamera;
};

uniform sampler2D u_diffuseMap;

// Unit 2: shadow depth map from the directional light.
uniform sampler2D u_shadowMap;

out vec4 FragColor;

void main() {
    vec4 diffuseSample = texture(u_diffuseMap, vs_out.uv);

    // Coverage-preserved alpha: counteracts the alpha-averaging that occurs
    // at higher mip levels, preventing distant geometry from looking too
    // transparent.
    float alpha = alphaCoveragePreserved(u_diffuseMap, vs_out.uv, 0.5);

    // Stochastic transparency: discard fragments based on the corrected alpha.
    stochasticDiscardIGN(alpha, gl_FragCoord.xy);

    // Decode sRGB albedo to linear light before shading.
    vec3 albedo = sRGBToLinear(diffuseSample.rgb);

    vec3 N = sampleNormalMap(vs_out.uv, vs_out.normal, vs_out.tangent);

    // Ambient term
    vec3 color = albedo * sceneGlobals.u_lighting.u_ambientLightColor.rgb;

    // Lambertian direct lighting from all active lights
    uint lightCount = sceneGlobals.u_lighting.u_lightCount.x;
    for (uint i = 0u; i < lightCount; ++i) {
        LightIncidence inc  = getLightIncidence(sceneGlobals.u_lighting.u_lights[i], vs_out.position);
        float          nDotL = max(dot(N, inc.direction), 0.0);

        // Apply PCF shadow test to directional lights.
        float shadow = 1.0;
        if (sceneGlobals.u_lighting.u_lights[i].u_type.x == uint(DIRECTIONAL_LIGHT)) {
            shadow = sampleShadow(u_shadowMap, shadowCamera, sceneGlobals, vs_out.position, N, inc.direction);
        }

        color += albedo * inc.radiance * nDotL * shadow;
    }

    // Tonemap from HDR linear to [0, 1], then encode to sRGB for display.
    FragColor = vec4(linearToSRGB(applyTonemap(color, sceneGlobals.u_tonemap)), 1.0);
}
