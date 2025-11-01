#pragma once
#include "common.glsl"

#ifdef __cplusplus
namespace glsl {
#endif

BEGIN_INPUT_STRUCT(SpriteInstance, Frequency::PER_VERTEX)
    IN_MEMBER(vec3, a_position, 0,      okami::AttributeType::Position)       // vec3 a_position;
    IN_MEMBER(float, a_rotation, 1,     okami::AttributeType::Unknown)       // float a_rotation;
    IN_MEMBER(vec4, a_spriteRect, 2,    okami::AttributeType::Unknown)     // vec4 a_spriteRect;
    IN_MEMBER(vec4, a_color, 3,         okami::AttributeType::Unknown)          // vec4 a_color;
    IN_MEMBER(vec2, a_size, 4,          okami::AttributeType::Unknown)          // vec2 a_size;
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(SpriteInstance)
    VERTEX_ARRAY_ITEM(a_position)
    VERTEX_ARRAY_ITEM(a_rotation)
    VERTEX_ARRAY_ITEM(a_spriteRect)
    VERTEX_ARRAY_ITEM(a_color)
    VERTEX_ARRAY_ITEM(a_size)
VERTEX_ARRAY_DEF_END()

#ifdef __cplusplus
} // namespace glsl
#endif