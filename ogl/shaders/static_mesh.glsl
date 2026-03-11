#pragma once
#include "common.glsl"

#ifdef __cplusplus
namespace glsl {
#endif

BEGIN_INPUT_STRUCT(StaticMeshVertex, Frequency::PerVertex)
    IN_MEMBER(vec3, a_position, 0,      okami::AttributeType::Position)       
    IN_MEMBER(vec2, a_uv, 1,            okami::AttributeType::TexCoord)   
    IN_MEMBER(vec3, a_normal, 2,        okami::AttributeType::Normal)
    IN_MEMBER(vec4, a_tangent, 3,       okami::AttributeType::Tangent)   
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(StaticMeshVertex)
    VERTEX_ARRAY_ITEM(a_position)
    VERTEX_ARRAY_ITEM(a_uv)
    VERTEX_ARRAY_ITEM(a_normal)
    VERTEX_ARRAY_ITEM(a_tangent)
VERTEX_ARRAY_DEF_END()

BEGIN_INPUT_STRUCT(StaticMeshInstance, Frequency::PerInstance)
    IN_MEMBER(vec4, a_instanceModel_col0,   4,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceModel_col1,   5,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceModel_col2,   6,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceModel_col3,   7,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col0,  8,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col1,  9,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col2,  10, okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col3,  11, okami::AttributeType::Unknown)
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(StaticMeshInstance)
    VERTEX_ARRAY_ITEM(a_instanceModel_col0)
    VERTEX_ARRAY_ITEM(a_instanceModel_col1)
    VERTEX_ARRAY_ITEM(a_instanceModel_col2)
    VERTEX_ARRAY_ITEM(a_instanceModel_col3)
    VERTEX_ARRAY_ITEM(a_instanceNormal_col0)
    VERTEX_ARRAY_ITEM(a_instanceNormal_col1)
    VERTEX_ARRAY_ITEM(a_instanceNormal_col2)
    VERTEX_ARRAY_ITEM(a_instanceNormal_col3)
VERTEX_ARRAY_DEF_END()

#ifdef __cplusplus
} // namespace glsl
#endif