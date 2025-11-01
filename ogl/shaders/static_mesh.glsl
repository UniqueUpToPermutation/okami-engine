#pragma once
#include "common.glsl"

#ifdef __cplusplus
namespace glsl {
#endif

BEGIN_INPUT_STRUCT(StaticMeshVertex, Frequency::PER_VERTEX, VertexLayout::SEPARATE)
    IN_MEMBER(vec3, a_position, 0,      okami::AttributeType::Position)       
    IN_MEMBER(vec2, a_uv, 1,            okami::AttributeType::TexCoord)   
    IN_MEMBER(vec3, a_normal, 2,        okami::AttributeType::Normal)
    IN_MEMBER(vec3, a_tangent, 3,       okami::AttributeType::Tangent)   
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(StaticMeshVertex)
    VERTEX_ARRAY_ITEM(a_position)
    VERTEX_ARRAY_ITEM(a_uv)
    VERTEX_ARRAY_ITEM(a_normal)
    VERTEX_ARRAY_ITEM(a_tangent)
VERTEX_ARRAY_DEF_END()

struct StaticMeshInstance {
    mat4 u_model;
    mat3 u_normalMatrix;
};

#ifdef __cplusplus
} // namespace glsl
#endif