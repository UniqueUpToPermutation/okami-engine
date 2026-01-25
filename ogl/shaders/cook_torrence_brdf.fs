#version 410 core

#include "constants.glsl"
#include "brdf.glsl"

const uint NumSamples = 1024;
const float InvNumSamples = 1.0 / float(NumSamples);

const float kEpsilon = 0.001; // This program needs larger eps.

// Output LUT
uniform ivec2 u_LUTSize; // pass the LUT size as a uniform (width, height)

// Ouput LUT
layout(location = 0) out vec4 FragColor;

void main()
{
	vec2 fragCoord = gl_FragCoord.xy;                // fragment coordinate (1-based pixel center)
    vec2 uvTexel = (fragCoord - 0.5) / vec2(u_LUTSize); // normalized texel-center coords in [0,1)
    float cosLo    = clamp(uvTexel.x, 0.0, 1.0);
    float roughness = clamp(uvTexel.y, 0.0, 1.0);     // flip with (1.0 - uvTexel.y) if needed

	// Make sure viewing angle is non-zero to avoid divisions by zero (and subsequently NaNs).
	cosLo = max(cosLo, kEpsilon);

	// Derive tangent-space viewing vector from angle to normal (pointing towards +Z in this reference frame).
	vec3 Lo = vec3(sqrt(1.0 - cosLo*cosLo), 0.0, cosLo);

	// We will now pre-integrate Cook-Torrance BRDF for a solid white environment and save results into a 2D LUT.
	// DFG1 & DFG2 are terms of split-sum approximation of the reflectance integral.
	// For derivation see: "Moving Frostbite to Physically Based Rendering 3.0", SIGGRAPH 2014, section 4.9.2.
	float DFG1 = 0;
	float DFG2 = 0;

	for(uint i=0; i<NumSamples; ++i) {
		vec2 u  = sampleHammersley(i, InvNumSamples);

		// Sample directly in tangent/shading space since we don't care about reference frame as long as it's consistent.
		vec3 Lh = sampleGGX(u.x, u.y, roughness);

		// Compute incident direction (Li) by reflecting viewing direction (Lo) around half-vector (Lh).
		vec3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;

		float cosLi   = Li.z;
		float cosLh   = Lh.z;
		float cosLoLh = max(dot(Lo, Lh), 0.0);

		if(cosLi > 0.0) {
			float G  = gaSchlickGGX_IBL(cosLi, cosLo, roughness);
			float Gv = G * cosLoLh / (cosLh * cosLo);
			float Fc = pow(1.0 - cosLoLh, 5);

			DFG1 += (1 - Fc) * Gv;
			DFG2 += Fc * Gv;
		}
	}

    FragColor = vec4(DFG1, DFG2, 0, 0) * InvNumSamples;
}