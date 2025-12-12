#pragma once
#include "common.glsl"

#ifdef __cplusplus
namespace glsl {
#endif

BEGIN_INPUT_STRUCT(Im3dVertex, Frequency::PerVertex)
    IN_MEMBER(vec4, a_positionSize, 0, okami::AttributeType::Position)  
    IN_MEMBER(u8vec4_norm_t, a_color, 1, okami::AttributeType::Color)   
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(Im3dVertex)
    VERTEX_ARRAY_ITEM(a_positionSize)
    VERTEX_ARRAY_ITEM(a_color)
VERTEX_ARRAY_DEF_END()

#ifdef __cplusplus
} // namespace glsl
#endif