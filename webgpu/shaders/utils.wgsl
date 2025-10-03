// Utility functions for colors and transformations

#include "common.wgsl"

// Additional color utilities
fn mixColors(color1: vec4<f32>, color2: vec4<f32>, factor: f32) -> vec4<f32> {
    return color1 * (1.0 - factor) + color2 * factor;
}

// Additional transform utilities  
fn rotateZ(angle: f32) -> mat4x4<f32> {
    let c = cos(angle);
    let s = sin(angle);
    return mat4x4<f32>(
        vec4<f32>(c, -s, 0.0, 0.0),
        vec4<f32>(s,  c, 0.0, 0.0),
        vec4<f32>(0.0, 0.0, 1.0, 0.0),
        vec4<f32>(0.0, 0.0, 0.0, 1.0)
    );
}