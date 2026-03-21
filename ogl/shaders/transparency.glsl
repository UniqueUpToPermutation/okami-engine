#pragma once

// ---------------------------------------------------------------------------
//  stochastic_transparency.glsl
//
//  Stochastic transparency via ordered dithering.
//
//  Instead of blending, alpha is used as a per-pixel discard probability:
//  if alpha < dither_threshold(pixel), the fragment is discarded.
//  The resulting binary coverage, averaged over a neighbourhood, integrates
//  to the correct opacity without needing sorted draw calls or OIT buffers.
//
//  Usage (pick one pattern and call the matching discard helper):
//
//      #include "stochastic_transparency.glsl"
//
//      // Inside main():
//      float alpha = texture(u_diffuseMap, uv).a;
//
//  ── Screen-space variants (fast but swim when the camera moves) ──────────
//
//      // Option A – Bayer 4×4 (cheapest, 4-pixel tile):
//      stochasticDiscardBayer4x4(alpha, gl_FragCoord.xy);
//
//      // Option B – Bayer 8×8 (finer tile, less pattern banding):
//      stochasticDiscardBayer8x8(alpha, gl_FragCoord.xy);
//
//      // Option C – Interleaved Gradient Noise (smooth, TAA-friendly):
//      stochasticDiscardIGN(alpha, gl_FragCoord.xy);
//
//      // Option C with temporal animation (pass u_frameIndex as a uniform):
//      stochasticDiscardIGN(alpha, gl_FragCoord.xy, u_frameIndex);
//
//  ── World-space variants (pattern anchored to geometry, no swimming) ─────
//
//  Pass the interpolated world-space fragment position (e.g. from a
//  `in vec3 v_worldPos;` varying) and a cellSize that controls how large
//  each dither cell is in world units.  Because the pattern is tied to the
//  surface rather than the screen it does not move when the camera moves.
//
//      // Option A – Bayer 4×4 world-space:
//      stochasticDiscardBayer4x4(alpha, v_worldPos, 0.05);
//
//      // Option B – Bayer 8×8 world-space:
//      stochasticDiscardBayer8x8(alpha, v_worldPos, 0.05);
//
//      // Option C – IGN world-space (static):
//      stochasticDiscardIGN(alpha, v_worldPos);
//
//      // Option C – IGN world-space with temporal animation:
//      stochasticDiscardIGN(alpha, v_worldPos, u_frameIndex);
//
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
//  Bayer ordered-dither matrices
// ---------------------------------------------------------------------------

// Returns a threshold in [0, 1) for screen pixel at integer position pos.
// Tile size: 4×4.
float bayerThreshold4x4(ivec2 pos) {
    // Row-major Bayer 4×4 matrix scaled to [0, 16).
    const float m[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0
    );
    return m[(pos.x & 3) + (pos.y & 3) * 4] / 16.0;
}

// World-space variant: dither cells are world-space cubes of side cellSize.
// All three axes are mixed into the 2D index so the pattern is correct on
// walls and ceilings as well as floors.  View-independent — no swimming.
float bayerThreshold4x4(vec3 worldPos, float cellSize) {
    ivec3 g = ivec3(floor(worldPos / cellSize));
    return bayerThreshold4x4(ivec2(g.x ^ (g.y * 7), g.z ^ (g.y * 13)));
}

// Returns a threshold in [0, 1) for screen pixel at integer position pos.
// Tile size: 8×8.
float bayerThreshold8x8(ivec2 pos) {
    // Row-major Bayer 8×8 matrix scaled to [0, 64).
    const float m[64] = float[64](
         0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
        48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
        12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
        60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
         3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
        51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
        15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
        63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
    );
    return m[(pos.x & 7) + (pos.y & 7) * 8] / 64.0;
}

// World-space variant (see bayerThreshold4x4(vec3, float) for notes).
float bayerThreshold8x8(vec3 worldPos, float cellSize) {
    ivec3 g = ivec3(floor(worldPos / cellSize));
    return bayerThreshold8x8(ivec2(g.x ^ (g.y * 7), g.z ^ (g.y * 13)));
}


// ---------------------------------------------------------------------------
//  Interleaved Gradient Noise (IGN)
//
//  Produces a spatially smooth, aperiodic threshold that avoids the
//  structured banding visible with Bayer patterns.  Animating the position
//  offset by a per-frame constant removes temporal correlations when used
//  with TAA / temporal accumulation.
//
//  Reference: "Next Generation Post Processing in Call of Duty: Advanced
//  Warfare", Jimenez et al., SIGGRAPH 2014.
// ---------------------------------------------------------------------------

// Returns a threshold in [0, 1) at screen position screenPos.
float ignThreshold(vec2 screenPos) {
    return fract(52.9829189 * fract(dot(screenPos, vec2(0.06711056, 0.00583715))));
}

// Temporally-animated variant.  Advance frameIndex every frame (wraps every
// 64 frames).  Pair with TAA or a temporal filter for best results.
float ignThreshold(vec2 screenPos, uint frameIndex) {
    screenPos += 5.588238 * float(frameIndex & 63u);
    return ignThreshold(screenPos);
}

// World-space IGN: hashes the 3D world position so the threshold is anchored
// to the surface and does not swim as the camera moves.  All three axes
// contribute via distinct magic constants so there are no stripe artifacts
// on walls or ceilings.
float ignThreshold(vec3 worldPos, float cellSize) {
    ivec3 g = ivec3(floor(worldPos / cellSize));
    // Combine all three axes with coprime multipliers to break symmetry
    vec2 p = vec2(
        float(g.x ^ (g.y * 127) ^ (g.z * 311)),
        float(g.z ^ (g.y * 7)   ^ (g.x * 1049))
    );
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}


// ---------------------------------------------------------------------------
//  Coverage-preserving alpha correction
//
//  When the GPU samples a higher mip level each texel's alpha becomes the
//  average of many base-level texels.  For cutout / stochastic content this
//  biases alpha toward intermediate values, making distant geometry appear
//  more transparent than intended.
//
//  This function sharpens alpha contrast around `referenceAlpha` by a factor
//  that grows with the mip level, approximating the effect of offline
//  coverage-preserving mipmap generation.
//
//  Formula: adjusted = referenceAlpha + (alpha - referenceAlpha) * 2^mip
//    - At mip 0 the alpha is unchanged.
//    - Alphas above referenceAlpha are pushed toward 1 at higher mips.
//    - Alphas below referenceAlpha are pushed toward 0 at higher mips.
//    - The fraction of pixels above referenceAlpha (coverage) is preserved.
//
//  `referenceAlpha` should match the effective discard midpoint:
//    - For hard cutout geometry (leaves, fences): 0.5
//    - For softer semi-transparent surfaces:      the mean alpha of the texture
// ---------------------------------------------------------------------------

float alphaCoveragePreserved(sampler2D tex, vec2 uv, float referenceAlpha) {
    float alpha = texture(tex, uv).a;
    float mip   = textureQueryLod(tex, uv).x;
    float scale = exp2(mip);
    return clamp(referenceAlpha + (alpha - referenceAlpha) * scale, 0.0, 1.0);
}

// Overload with referenceAlpha = 0.5 (suitable for most cutout textures).
float alphaCoveragePreserved(sampler2D tex, vec2 uv) {
    return alphaCoveragePreserved(tex, uv, 0.5);
}


// ---------------------------------------------------------------------------
//  Convenience discard helpers
// ---------------------------------------------------------------------------

// Discard if alpha is below the Bayer 4×4 threshold at this pixel.
void stochasticDiscardBayer4x4(float alpha, vec2 screenPos) {
    if (alpha < bayerThreshold4x4(ivec2(screenPos)))
        discard;
}

// Discard if alpha is below the Bayer 8×8 threshold at this pixel.
void stochasticDiscardBayer8x8(float alpha, vec2 screenPos) {
    if (alpha < bayerThreshold8x8(ivec2(screenPos)))
        discard;
}

// Discard if alpha is below the IGN threshold at this pixel (static).
void stochasticDiscardIGN(float alpha, vec2 screenPos) {
    if (alpha < ignThreshold(screenPos))
        discard;
}

// Discard if alpha is below the IGN threshold at this pixel (animated).
void stochasticDiscardIGN(float alpha, vec2 screenPos, uint frameIndex) {
    if (alpha < ignThreshold(screenPos, frameIndex))
        discard;
}

// ---------------------------------------------------------------------------
//  World-space discard helpers  (no swimming when the camera moves)
// ---------------------------------------------------------------------------

// Discard if alpha is below the Bayer 4×4 threshold at this world position.
void stochasticDiscardBayer4x4(float alpha, vec3 worldPos, float cellSize) {
    if (alpha < bayerThreshold4x4(worldPos, cellSize))
        discard;
}

// Discard if alpha is below the Bayer 8×8 threshold at this world position.
void stochasticDiscardBayer8x8(float alpha, vec3 worldPos, float cellSize) {
    if (alpha < bayerThreshold8x8(worldPos, cellSize))
        discard;
}

// Discard if alpha is below the IGN threshold at this world position (static).
void stochasticDiscardIGN(float alpha, vec3 worldPos, float cellSize) {
    if (alpha < ignThreshold(worldPos, cellSize))
        discard;
}


// Discard if alpha is below the IGN threshold at this world position
void discardIfTransparent(float alpha) {
    if (alpha < 0.5)
        discard;
}