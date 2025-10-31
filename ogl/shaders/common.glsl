#ifndef COMMON_GLSL_INCLUDED
#define COMMON_GLSL_INCLUDED

#include "types.glsl"

#ifdef __cplusplus
#define BEGIN_INPUT_STRUCT(name, sFrequency, sLayout) \
    struct name { \
        using type_t = name; \
        static constexpr int __frequency = static_cast<int>(sFrequency); \
        static constexpr VertexLayout __layout = sLayout;

#define IN_MEMBER(type_name, member_name, location, attribute_type) \
        type_name member_name; \
        static constexpr int __##member_name##_location = location; \
        static constexpr int __##member_name##_size = ComponentCount<type_name>::value; \
        static constexpr okami::AttributeType __##member_name##_type = attribute_type;

#define END_INPUT_STRUCT() \
    };

#define VERTEX_ARRAY_DEF(name) \
    inline void __create_vertex_array_##name(glsl::VertexShaderMeta const& args) { \
        if (args.vertexArraySetup) { \
            glBindVertexArray(args.vertexArraySetup->vertexArray); \
            if (args.vertexArraySetup->indexBufferSetup) { \
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, args.vertexArraySetup->indexBufferSetup->buffer); \
            } \
        } \
        using type_t = name;

#define VERTEX_ARRAY_ITEM(member_name) \
    if (args.vertexArraySetup) { \
        size_t offset_##member_name = offsetof(type_t, member_name); \
        GLsizei stride_##member_name = static_cast<GLsizei>(sizeof(type_t)); \
        if constexpr (type_t::__layout == VertexLayout::SEPARATE) { \
            offset_##member_name = 0; \
        } \
        if constexpr (type_t::__layout == VertexLayout::SEPARATE) { \
            stride_##member_name = 0; \
        } \
        if (args.vertexArraySetup->setupByLocation) { \
            auto const& setupByLocation = args.vertexArraySetup->setupByLocation.value().at(type_t::__##member_name##_location); \
            glBindBuffer(GL_ARRAY_BUFFER, setupByLocation.buffer); \
            offset_##member_name += setupByLocation.offset; \
        } \
        glEnableVertexAttribArray(type_t::__##member_name##_location); \
        glVertexAttribPointer( \
            type_t::__##member_name##_location, \
            type_t::__##member_name##_size, \
            GL_FLOAT, \
            GL_FALSE, \
            stride_##member_name, \
            (void*)offset_##member_name); \
        glVertexAttribDivisor(type_t::__##member_name##_location, type_t::__frequency); \
    } \
    if (args.outInfo) { \
        args.outInfo->locationToAttrib[type_t::__##member_name##_location] = glsl::InputAttribInfo{ \
            type_t::__##member_name##_type, \
            type_t::__##member_name##_size \
        }; \
    }

#define VERTEX_ARRAY_DEF_END() \
        if (args.vertexArraySetup) { \
            glBindVertexArray(0); \
        } \
}

#else
// GLSL side - generate vertex shader input attributes
#define BEGIN_INPUT_STRUCT(name, frequency, sLayout)
#define IN_MEMBER(type, member_name, loc_idx, attribute_type) \
    layout(location = loc_idx) in type member_name;
#define END_INPUT_STRUCT()

// GLSL side doesn't need vertex array setup
#define VERTEX_ARRAY_DEF(name)
#define VERTEX_ARRAY_ITEM(member_name)
#define VERTEX_ARRAY_DEF_END()
#endif

#endif // COMMON_GLSL_INCLUDED