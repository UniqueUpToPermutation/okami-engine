#version 330 core

// Full-screen triangle vertex shader using gl_VertexID
// Positions are generated in clip space so no vertex buffer is required

out vec2 v_uv;

void main() {
    // Map gl_VertexID (0,1,2) to full-screen triangle positions
    // using a common pattern that covers the screen with one triangle
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0), // lower-left
        vec2( 3.0, -1.0), // lower-right beyond to cover
        vec2(-1.0,  3.0)  // top-left beyond to cover
    );

    vec2 pos = positions[gl_VertexID];
    gl_Position = vec4(pos, 0.0, 1.0);

    // Compute UV in 0..1 from clip-space position
    v_uv = pos * 0.5 + 0.5;
}
