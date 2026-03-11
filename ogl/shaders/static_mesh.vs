#version 410 core

#include "static_mesh.glsl"
#include "scene.glsl"
#include "vs_outputs.glsl"

layout(std140) uniform SceneGlobalsBlock {
    SceneGlobals sceneGlobals;
};

out MESH_VS_OUT vs_out;

void main() {
    mat4 u_model = mat4(a_instanceModel_col0, a_instanceModel_col1,
                        a_instanceModel_col2, a_instanceModel_col3);
    mat4 u_normalMatrix = mat4(a_instanceNormal_col0, a_instanceNormal_col1,
                               a_instanceNormal_col2, a_instanceNormal_col3);

    vec4 worldPosition = u_model * vec4(a_position, 1.0);
    gl_Position = sceneGlobals.u_camera.u_viewProj * worldPosition;

    vec3 normal = normalize(u_normalMatrix * vec4(a_normal, 0.0)).xyz;
    vec3 tangent = (u_normalMatrix * vec4(a_tangent.xyz, 0.0)).xyz;
    tangent = normalize(tangent / a_tangent.w);
    vec3 bitangent = normalize(cross(normal, tangent));

    vs_out.position = worldPosition.xyz;
    vs_out.uv = a_uv;
    vs_out.normal = normal;
    vs_out.tangent = tangent;
    vs_out.bitangent = bitangent;
}
