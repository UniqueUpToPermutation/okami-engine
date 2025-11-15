#pragma once
#ifdef __cplusplus
#include <glad/gl.h> // Preprocessor: Ignore
#include <glm/glm.hpp> // Preprocessor: Ignore
#include <cstddef> // Preprocessor: Ignore
#include <functional> // Preprocessor: Ignore
#include <unordered_map> // Preprocessor: Ignore
#include <optional> // Preprocessor: Ignore

#include "../../geometry.hpp" // Preprocessor: Ignore

// Type trait to get component count - defined outside namespace for easier access
template<typename T> struct ComponentCount;
template<> struct ComponentCount<glm::vec3> { static constexpr int value = 3; };
template<> struct ComponentCount<glm::vec4> { static constexpr int value = 4; };
template<> struct ComponentCount<glm::vec2> { static constexpr int value = 2; };
template<> struct ComponentCount<float> { static constexpr int value = 1; };

template <typename T> struct ComponentTypeTrait;
template <> struct ComponentTypeTrait<glm::vec4> {
    static constexpr okami::AccessorComponentType componentType = okami::AccessorComponentType::Float;
};
template <> struct ComponentTypeTrait<glm::vec3> {
    static constexpr okami::AccessorComponentType componentType = okami::AccessorComponentType::Float;
};
template <> struct ComponentTypeTrait<glm::vec2> {
    static constexpr okami::AccessorComponentType componentType = okami::AccessorComponentType::Float;
};
template <> struct ComponentTypeTrait<float> {
    static constexpr okami::AccessorComponentType componentType = okami::AccessorComponentType::Float;
};

namespace glsl {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using mat3 = glm::mat3;

enum class Frequency {
    PerVertex = 0,
    PerInstance = 1,
};

struct InputAttribInfo {
    okami::AttributeType m_type;
    okami::AccessorComponentType m_componentType;
    uint32_t m_componentCount;
    uint32_t m_offset;
    uint32_t m_stride;
    Frequency m_frequency;
};

struct VertexShaderInputInfo {
    uint32_t m_totalStride = 0;
    std::unordered_map<int, InputAttribInfo> locationToAttrib;
};

} // namespace glsl

#endif