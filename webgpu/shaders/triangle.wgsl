#include "common.wgsl"

@group(0) @binding(0) var<uniform> transform: Transform;

@vertex fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    out.position = transform.matrix * vec4<f32>(getTrianglePos(vertexIndex), 0.0, 1.0);
    out.color = getTriangleColor(vertexIndex);

    return out;
}

@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}