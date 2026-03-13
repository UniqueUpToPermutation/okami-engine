#version 410 core

#include "static_mesh.glsl"
#include "scene.glsl"

layout(std140) uniform CameraGlobalsBlock {
    CameraGlobals cameraGlobals;
};

void main() {
    mat4 model = mat4(a_instanceModel_col0, a_instanceModel_col1,
                      a_instanceModel_col2, a_instanceModel_col3);
    gl_Position = cameraGlobals.u_viewProj * model * vec4(a_position, 1.0);
}
