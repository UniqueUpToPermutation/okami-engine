#version 410 core

// Input from geometry shader
in vec2 v_texCoord;
in vec4 v_color;

// Uniforms
uniform sampler2D u_texture;

// Output
layout(location = 0) out vec4 FragColor;

void main() {
    // Sample texture and apply tint color
    vec4 texColor = texture(u_texture, v_texCoord);
    
    // Apply sprite color tint
    //FragColor = texColor * v_color;
    FragColor = texColor;

    // Discard transparent pixels if needed (optional)
    //if (FragColor.a < 0.01) {
    //    discard;
    //}

    //FragColor = vec4(v_texCoord, 1.0, 1.0);
}
