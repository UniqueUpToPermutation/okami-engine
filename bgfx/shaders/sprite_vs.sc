$input a_position, a_color0, a_color1, a_texcoord0

$output v_color0, v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // a_position contains: xyz = vertex position in sprite space
    // a_color0 contains: xy = sprite world position, zw = size  
    // a_color1 contains: rgba = color
    // a_texcoord0 contains: xy = UV coordinates
    
    vec2 spriteWorldPos = a_color0.xy;
    vec2 spriteSize = a_color0.zw;
    vec3 localPos = a_position;
    
    // Scale by sprite size
    localPos.xy *= spriteSize;
    
    // Translate to world position
    vec3 worldPos = vec3(spriteWorldPos + localPos.xy, localPos.z);
    
    gl_Position = mul(vec4(worldPos, 1.0), u_modelViewProj);
    v_color0 = a_color1;
    v_texcoord0 = a_texcoord0;
}