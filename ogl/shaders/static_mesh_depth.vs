#version 410 core

#include "static_mesh.glsl"

// Output world-space position; the geometry shader applies per-cascade VP matrices.
out vec3 vs_worldPos;

void main() {
    mat4 model = mat4(a_instanceModel_col0, a_instanceModel_col1,
                      a_instanceModel_col2, a_instanceModel_col3);
    vs_worldPos = vec3(model * vec4(a_position, 1.0));
}
