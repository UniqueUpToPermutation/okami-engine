struct Transform {
    matrix: mat4x4<f32>,
}

@group(0) @binding(0) var<uniform> transform: Transform;

// Common vertex positions for a triangle
fn getTrianglePos(vertexIndex: u32) -> vec2<f32> {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>( 0.0,  0.5),
        vec2<f32>(-0.5, -0.5),
        vec2<f32>( 0.5, -0.5)
    );
    return pos[vertexIndex];
}

// Common vertex colors for a triangle
fn getTriangleColor(vertexIndex: u32) -> vec4<f32> {
    var color = array<vec4<f32>, 3>(
        vec4<f32>(1.0, 0.0, 0.0, 1.0),
        vec4<f32>(0.0, 1.0, 0.0, 1.0),
        vec4<f32>(0.0, 0.0, 1.0, 1.0)
    );
    return color[vertexIndex];
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>
}

@vertex fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    out.position = transform.matrix * vec4<f32>(getTrianglePos(vertexIndex), 0.0, 1.0);
    out.color = getTriangleColor(vertexIndex);

    return out;
}

@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}