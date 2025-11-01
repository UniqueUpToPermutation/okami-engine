#version 410 core

#include "static_mesh.glsl"
#include "scene.glsl"

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;  // Use the struct inside the block
};

layout(std140) uniform StaticMeshInstanceBlock {
    StaticMeshInstance staticMeshInstance;  // Use the struct inside the block
};

out VS_OUT {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
} vs_out;

void main() {
    vec4 worldPosition = staticMeshInstance.u_model * vec4(a_position, 1.0);
    gl_Position = sceneGlobals.u_camera.u_viewProj * worldPosition;

    vec3 normal = normalize(staticMeshInstance.u_normalMatrix * a_normal);
    vec3 tangent = normalize(staticMeshInstance.u_normalMatrix * a_tangent);
    vec3 bitangent = normalize(cross(normal, tangent));

    vs_out.position = worldPosition.xyz;
    vs_out.uv = a_uv;
    vs_out.normal = normal;
    vs_out.tangent = tangent;
    vs_out.bitangent = bitangent;
}
