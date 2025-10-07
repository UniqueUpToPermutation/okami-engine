struct CameraUniforms {
    view: mat4x4<f32>,
    projection: mat4x4<f32>
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var spriteTexture: texture_2d<f32>;
@group(1) @binding(1) var spriteSampler: sampler;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) rotation: f32,
    @location(2) size: vec2<f32>,
    @location(3) uv0: vec2<f32>,
    @location(4) uv1: vec2<f32>,
    @location(5) origin: vec2<f32>,
    @location(6) color: vec4<f32>
}

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>
}

// Quad vertices: two triangles forming a rectangle
// Vertex order: 0-1-2, 0-2-3 (counter-clockwise)
var<private> QUAD_VERTICES: array<vec2<f32>, 6> = array<vec2<f32>, 6>(
    vec2<f32>(0.0, 0.0),  // Bottom-left
    vec2<f32>(1.0, 0.0),  // Bottom-right  
    vec2<f32>(1.0, 1.0),  // Top-right
    vec2<f32>(0.0, 0.0),  // Bottom-left (second triangle)
    vec2<f32>(1.0, 1.0),  // Top-right
    vec2<f32>(0.0, 1.0)   // Top-left
);

var<private> QUAD_UVS: array<vec2<f32>, 6> = array<vec2<f32>, 6>(
    vec2<f32>(0.0, 1.0),  // Bottom-left UV (flipped Y)
    vec2<f32>(1.0, 1.0),  // Bottom-right UV
    vec2<f32>(1.0, 0.0),  // Top-right UV
    vec2<f32>(0.0, 1.0),  // Bottom-left UV (second triangle)
    vec2<f32>(1.0, 0.0),  // Top-right UV
    vec2<f32>(0.0, 0.0)   // Top-left UV
);

@vertex fn vs_main(
    @builtin(vertex_index) vertexIndex: u32,
    input: VertexInput
) -> VertexOutput {
    var output: VertexOutput;
    
    // Get the quad vertex position (0-1 range)
    let quadVertex = QUAD_VERTICES[vertexIndex];
    let quadUV = QUAD_UVS[vertexIndex];
    
    // Calculate sprite corner position relative to origin
    let cornerOffset = (quadVertex - input.origin) * input.size;
    
    // Apply 2D rotation around the origin
    let cosRot = cos(input.rotation);
    let sinRot = sin(input.rotation);
    let rotatedOffset = vec2<f32>(
        cornerOffset.x * cosRot - cornerOffset.y * sinRot,
        cornerOffset.x * sinRot + cornerOffset.y * cosRot
    );
    
    // Calculate final world position
    let worldPos = input.position + vec3<f32>(rotatedOffset, 0.0);
    
    // Transform to clip space
    output.position = camera.projection * camera.view * vec4<f32>(worldPos, 1.0);
    
    // Interpolate UV coordinates based on the sprite's UV rectangle
    output.uv = mix(input.uv0, input.uv1, quadUV);
    
    // Pass through color
    output.color = input.color;
    
    return output;
}

@fragment fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let textureColor = textureSample(spriteTexture, spriteSampler, input.uv);
    return textureColor * input.color;
}