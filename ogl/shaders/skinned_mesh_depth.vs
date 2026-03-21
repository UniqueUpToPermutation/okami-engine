#version 410 core

#include "skinned_mesh.glsl"

// Joint skinning matrices (same block as forward pass, shared binding point).
#define MAX_JOINTS 256

layout(std140) uniform JointMatricesBlock {
    mat4 u_jointMatrices[MAX_JOINTS];
};

// Output world-space position; the geometry shader applies per-cascade VP matrices.
out vec3 vs_worldPos;

void main() {
    mat4 u_model = mat4(a_instanceModel_col0, a_instanceModel_col1,
                        a_instanceModel_col2, a_instanceModel_col3);

    ivec4 joints  = ivec4(a_joints);
    vec4  weights = a_weights;

    mat4 skinMatrix =
        weights.x * u_jointMatrices[joints.x] +
        weights.y * u_jointMatrices[joints.y] +
        weights.z * u_jointMatrices[joints.z] +
        weights.w * u_jointMatrices[joints.w];

    vs_worldPos = vec3(u_model * skinMatrix * vec4(a_position, 1.0));
}
