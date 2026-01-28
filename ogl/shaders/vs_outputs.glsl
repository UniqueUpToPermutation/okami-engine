/*
    GLSL structures for vertex shader outputs
*/

struct MESH_VS_OUT {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
};

struct SKY_VS_OUT {
    vec2 uv;
    vec3 direction;
};