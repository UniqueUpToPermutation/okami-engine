#pragma once
#include "common.glsl"

#ifdef __cplusplus
namespace glsl {
#endif

// Per-vertex data for a skinned (skeletal) mesh primitive.
// Locations 0-3 match StaticMeshVertex so the same fragment shaders apply.
// Locations 4-5 carry the skinning data.
BEGIN_INPUT_STRUCT(SkinnedMeshVertex, Frequency::PerVertex)
    IN_MEMBER(vec3, a_position, 0, okami::AttributeType::Position)
    IN_MEMBER(vec2, a_uv,       1, okami::AttributeType::TexCoord)
    IN_MEMBER(vec3, a_normal,   2, okami::AttributeType::Normal)
    IN_MEMBER(vec4, a_tangent,  3, okami::AttributeType::Tangent)
    IN_MEMBER(vec4, a_joints,   4, okami::AttributeType::Joints)
    IN_MEMBER(vec4, a_weights,  5, okami::AttributeType::Weights)
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(SkinnedMeshVertex)
    VERTEX_ARRAY_ITEM(a_position)
    VERTEX_ARRAY_ITEM(a_uv)
    VERTEX_ARRAY_ITEM(a_normal)
    VERTEX_ARRAY_ITEM(a_tangent)
    VERTEX_ARRAY_ITEM(a_joints)
    VERTEX_ARRAY_ITEM(a_weights)
VERTEX_ARRAY_DEF_END()

// Per-instance data for skinned meshes.
// Starts at location 6 (after the 6 per-vertex attributes, locations 0-5).
BEGIN_INPUT_STRUCT(SkinnedMeshInstance, Frequency::PerInstance)
    IN_MEMBER(vec4, a_instanceModel_col0,   6,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceModel_col1,   7,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceModel_col2,   8,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceModel_col3,   9,  okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col0,  10, okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col1,  11, okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col2,  12, okami::AttributeType::Unknown)
    IN_MEMBER(vec4, a_instanceNormal_col3,  13, okami::AttributeType::Unknown)
END_INPUT_STRUCT()

VERTEX_ARRAY_DEF(SkinnedMeshInstance)
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
