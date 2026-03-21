#version 410 core

#include "skinned_mesh.glsl"
#include "scene.glsl"
#include "vs_outputs.glsl"

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;
};

// Joint skinning matrices UBO.
// 256 mat4s == 16384 bytes which is the minimum guaranteed GL_MAX_UNIFORM_BLOCK_SIZE
// in OpenGL 4.1, so this works on any conformant driver.
#define MAX_JOINTS 256

layout(std140) uniform JointMatricesBlock {
    mat4 u_jointMatrices[MAX_JOINTS];
};

out MESH_VS_OUT vs_out;

void main() {
    mat4 u_model = mat4(a_instanceModel_col0, a_instanceModel_col1,
                        a_instanceModel_col2, a_instanceModel_col3);
    mat4 u_normalMatrix = mat4(a_instanceNormal_col0, a_instanceNormal_col1,
                               a_instanceNormal_col2, a_instanceNormal_col3);

    // a_joints stores joint indices as floats; cast back to int for indexing.
    ivec4 joints  = ivec4(a_joints);
    vec4  weights = a_weights;

    // Linear blend skinning: sum the 4 weighted joint matrices.
    mat4 skinMatrix =
        weights.x * u_jointMatrices[joints.x] +
        weights.y * u_jointMatrices[joints.y] +
        weights.z * u_jointMatrices[joints.z] +
        weights.w * u_jointMatrices[joints.w];

    vec4 skinnedPos     = skinMatrix * vec4(a_position,     1.0);
    vec4 skinnedNormal  = skinMatrix * vec4(a_normal,       0.0);
    vec4 skinnedTangent = skinMatrix * vec4(a_tangent.xyz,  0.0);

    vec4 worldPosition = u_model * skinnedPos;
    gl_Position = sceneGlobals.u_camera.u_viewProj * worldPosition;

    vec3 normal    = normalize((u_normalMatrix * skinnedNormal).xyz);
    vec3 tangent   = normalize((u_normalMatrix * skinnedTangent).xyz);
    vec3 bitangent = normalize(cross(normal, tangent));

    vs_out.position  = worldPosition.xyz;
    vs_out.uv        = a_uv;
    vs_out.normal    = normal;
    vs_out.tangent   = tangent;
    vs_out.bitangent = bitangent;
}
