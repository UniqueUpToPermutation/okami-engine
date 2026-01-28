#version 410 core

#include "static_mesh.glsl"
#include "scene.glsl"
#include "vs_outputs.glsl"

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;  // Use the struct inside the block
};

layout(std140) uniform StaticMeshInstanceBlock {
    StaticMeshInstance staticMeshInstance;  // Use the struct inside the block
};

out MESH_VS_OUT vs_out;

void main() {
    vec4 worldPosition = staticMeshInstance.u_model * vec4(a_position, 1.0);
    gl_Position = sceneGlobals.u_camera.u_viewProj * worldPosition;

    vec3 normal = normalize(staticMeshInstance.u_normalMatrix * vec4(a_normal, 0.0)).xyz;
    vec3 tangent = (staticMeshInstance.u_normalMatrix * vec4(a_tangent.xyz, 0.0)).xyz;
    tangent = normalize(tangent / a_tangent.w);
    vec3 bitangent = normalize(cross(normal, tangent));

    vs_out.position = worldPosition.xyz;
    vs_out.uv = a_uv;
    vs_out.normal = normal;
    vs_out.tangent = tangent;
    vs_out.bitangent = bitangent;
}
