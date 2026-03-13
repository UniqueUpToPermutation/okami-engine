#version 410 core

// Depth-only pass: fragment colour is unused.
// The GPU writes gl_FragCoord.z to the depth buffer automatically.

void main() {}
