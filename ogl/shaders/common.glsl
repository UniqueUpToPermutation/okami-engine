#pragma once
#include "types.glsl"

#ifdef __cplusplus
#define BEGIN_INPUT_STRUCT(name, sFrequency) \
    struct name { \
        using type_t = name; \
        static constexpr Frequency __frequency = sFrequency;

#define IN_MEMBER(type_name, member_name, location, attribute_type) \
    type_name member_name; \
    static constexpr uint32_t __##member_name##_location = location; \
    static constexpr okami::AttributeType __##member_name##_type = attribute_type; \
    static constexpr okami::AccessorComponentType __##member_name##_componentType = ComponentTypeTrait<type_name>::componentType; \
    static constexpr int __##member_name##_componentCount = ComponentCount<type_name>::value; 

#define END_INPUT_STRUCT() \
    };

#define VERTEX_ARRAY_DEF(name) \
    inline VertexShaderInputInfo __get_vs_input_info##name() { \
        VertexShaderInputInfo result; \
        result.m_totalStride = sizeof(name); \
        using type_t = name;

#define VERTEX_ARRAY_ITEM(member_name) \
    result.locationToAttrib[type_t::__##member_name##_location] = glsl::InputAttribInfo{ \
        .m_type = type_t::__##member_name##_type, \
        .m_componentType = type_t::__##member_name##_componentType, \
        .m_componentCount = type_t::__##member_name##_componentCount, \
        .m_offset = offsetof(type_t, member_name), \
        .m_stride = sizeof(type_t), \
        .m_frequency = type_t::__frequency, \
        .m_isNormalized = (type_t::__##member_name##_type == okami::AttributeType::Color) \
    }; \

#define VERTEX_ARRAY_DEF_END() \
    return result; \
}

#else
// GLSL side - generate vertex shader input attributes
#define BEGIN_INPUT_STRUCT(name, frequency)
#define IN_MEMBER(type, member_name, loc_idx, attribute_type) \
    layout(location = loc_idx) in type member_name;
#define END_INPUT_STRUCT()

// GLSL side doesn't need vertex array setup
#define VERTEX_ARRAY_DEF(name)
#define VERTEX_ARRAY_ITEM(member_name)
#define VERTEX_ARRAY_DEF_END()
#endif