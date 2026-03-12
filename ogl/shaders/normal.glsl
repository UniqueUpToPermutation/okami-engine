#pragma once

// Requires vs_outputs.glsl to be included first (MESH_VS_OUT).

// Tangent-space normal map sampler.
// When no normal map is provided by the material, the material manager binds
// a 1x1 flat-normal texture (0.5, 0.5, 1.0) which decodes to (0, 0, 1) —
// the unperturbed geometric normal.
uniform sampler2D u_normalMap;

// Returns the world-space shading normal for the current fragment.
//
// Builds a TBN matrix from the interpolated vertex tangent frame, re-
// orthogonalises T against the geometric normal to remove interpolation drift,
// then transforms the tangent-space sample from u_normalMap into world space.
//
// 'uv'      – texture coordinate used to sample u_normalMap.
// 'normal'  – interpolated world-space vertex normal   (need not be unit length).
// 'tangent' – interpolated world-space vertex tangent  (need not be unit length).
vec3 sampleNormalMap(vec2 uv, vec3 normal, vec3 tangent) {
    vec3 Ng = normalize(normal);
    vec3 T  = normalize(tangent);
    T = normalize(T - dot(T, Ng) * Ng); // Gram-Schmidt re-orthogonalise
    vec3 B  = cross(Ng, T);
    mat3 TBN = mat3(T, B, Ng);

    vec3 ts = texture(u_normalMap, uv).rgb * 2.0 - 1.0;
    return normalize(TBN * ts);
}
