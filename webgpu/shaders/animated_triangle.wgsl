#include "utils.wgsl"

@group(0) @binding(0) var<uniform> transform: Transform;
@group(1) @binding(0) var<uniform> time: f32;

@vertex fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    let rotation = rotateZ(time * 2.0);
    let combinedTransform = transform.matrix * rotation;
    
    var out: VertexOutput;
    out.position = combinedTransform * vec4<f32>(getTrianglePos(vertexIndex), 0.0, 1.0);
    
    // Mix colors over time
    let baseColor = getTriangleColor(vertexIndex);
    let altColor = vec4<f32>(baseColor.y, baseColor.z, baseColor.x, baseColor.w); // Rotate RGB
    out.color = mixColors(baseColor, altColor, (sin(time) + 1.0) * 0.5);
    
    return out;
}

@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}