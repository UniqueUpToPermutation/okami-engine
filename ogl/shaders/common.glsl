#ifdef __cplusplus
#include <glad/gl.h> // Preprocessor: Ignore
#include <glm/glm.hpp> // Preprocessor: Ignore
#include <cstddef> // Preprocessor: Ignore

// Type trait to get component count - defined outside namespace for easier access
template<typename T> struct ComponentCount;
template<> struct ComponentCount<glm::vec3> { static constexpr int value = 3; };
template<> struct ComponentCount<glm::vec4> { static constexpr int value = 4; };
template<> struct ComponentCount<glm::vec2> { static constexpr int value = 2; };
template<> struct ComponentCount<float> { static constexpr int value = 1; };

namespace glsl {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

} // namespace glsl

#define BEGIN_INPUT_STRUCT(name) \
    struct name { \
        using type_t = name;

#define IN_MEMBER(type_name, member_name, location) \
        type_name member_name; \
        static constexpr std::size_t __##member_name##_offset() { return offsetof(type_t, member_name); } \
        static constexpr int __##member_name##_location() { return location; } \
        static constexpr int __##member_name##_size() { return ComponentCount<type_name>::value; }

#define END_INPUT_STRUCT() \
    };

#define VERTEX_ARRAY_DEF(name) \
    inline void __create_vertex_array_##name() { \
        using type_t = name;

#define VERTEX_ARRAY_ITEM(member_name) \
    glEnableVertexAttribArray(type_t::__##member_name##_location()); \
    glVertexAttribPointer( \
        type_t::__##member_name##_location(), \
        type_t::__##member_name##_size(), \
        GL_FLOAT, \
        GL_FALSE, \
        sizeof(type_t), \
        (void*)type_t::__##member_name##_offset() \
    ); \
    glVertexAttribDivisor(type_t::__##member_name##_location(), 1);

#define VERTEX_ARRAY_DEF_END() \
    }

#else
// GLSL side - generate vertex shader input attributes
#define BEGIN_INPUT_STRUCT(name)
#define IN_MEMBER(type, member_name, loc_idx) \
    layout(location = loc_idx) in type member_name;
#define END_INPUT_STRUCT()

// GLSL side doesn't need vertex array setup
#define VERTEX_ARRAY_DEF(name)
#define VERTEX_ARRAY_ITEM(member_name)
#define VERTEX_ARRAY_DEF_END()
#endif