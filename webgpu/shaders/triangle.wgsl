struct Transform {
    matrix: mat4x4<f32>,
}

@group(0) @binding(0) var<uniform> transform: Transform;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>
}

@vertex fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>( 0.0,  0.5),
        vec2<f32>(-0.5, -0.5),
        vec2<f32>( 0.5, -0.5)
    );

    var color = array<vec4<f32>, 3>(
        vec4<f32>(1.0,  0.0, 0.0, 1.0),
        vec4<f32>(0.0, 1.0, 0.0, 1.0),
        vec4<f32>(0.0, 0.0, 1.0, 1.0)
    );
    
    var out: VertexOutput;
    out.position = transform.matrix * vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    out.color = color[vertexIndex];
    return out;
}

@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}