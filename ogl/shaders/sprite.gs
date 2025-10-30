#version 410 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 u_viewProj;

// Input from vertex shader
in VS_OUT {
    vec3 position;
    float rotation;
    vec4 spriteRect;
    vec4 color;
    vec2 size;
} gs_in[];

// Output to fragment shader
out vec2 v_texCoord;
out vec4 v_color;

void main() {
    // Get sprite data from the input point
    vec3 center = gs_in[0].position;
    float rotation = gs_in[0].rotation;
    vec4 spriteRect = gs_in[0].spriteRect;
    vec4 color = gs_in[0].color;
    vec2 size = gs_in[0].size;
    
    // Calculate rotation matrix
    float sinRot = sin(rotation);
    float cosRot = cos(rotation);
    mat2 rotationMatrix = mat2(
        cosRot, -sinRot,
        sinRot,  cosRot
    );
    
    // Define quad corners (relative to center)
    vec2 corners[4] = vec2[](
        vec2(-0.5, -0.5),  // Bottom-left
        vec2( 0.5, -0.5),  // Bottom-right
        vec2(-0.5,  0.5),  // Top-left
        vec2( 0.5,  0.5)   // Top-right
    );
    
    // Define UV coordinates for each corner
    vec2 uvs[4] = vec2[](
        vec2(0.0, 1.0),  // Bottom-left
        vec2(1.0, 1.0),  // Bottom-right
        vec2(0.0, 0.0),  // Top-left
        vec2(1.0, 0.0)   // Top-right
    );
    
    // Pass color to all vertices
    v_color = color;
    
    // Emit vertices for triangle strip (forms a quad)
    for (int i = 0; i < 4; i++) {
        // Apply size and rotation to corner position
        vec2 rotatedCorner = rotationMatrix * (size * corners[i]);
        vec3 worldPos = center + vec3(rotatedCorner, 0.0);
        
        // Transform to clip space
        gl_Position = u_viewProj * vec4(worldPos, 1.0);
        
        // Calculate texture coordinate
        v_texCoord = spriteRect.xy + uvs[i] * spriteRect.zw;
        
        EmitVertex();
    }
    
    EndPrimitive();
}
