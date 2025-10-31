#ifndef TYPES_GLSL
#define TYPES_GLSL

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

namespace glsl {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

struct VertexArraySetupAttrib {
    GLuint buffer = 0;
    size_t offset = 0;
};

struct VertexArraySetupArgs {
    GLuint vertexArray;
    std::optional<VertexArraySetupAttrib> indexBufferSetup;
    std::optional<std::unordered_map<int, VertexArraySetupAttrib>> setupByLocation;
};

struct InputAttribInfo {
    okami::AttributeType type;
    int componentCount;
};

struct VertexShaderInputInfo {
    std::unordered_map<int, InputAttribInfo> locationToAttrib;
};

struct VertexShaderMeta {
    std::optional<VertexArraySetupArgs> vertexArraySetup;
    VertexShaderInputInfo* outInfo = nullptr;
};

enum class Frequency {
    PER_VERTEX = 0,
    PER_INSTANCE = 1,
};

enum class VertexLayout {
    INTERLEAVED = 0,
    SEPARATE = 1
};

using vs_meta_func_t = std::function<void(VertexShaderMeta const&)>;

inline VertexShaderInputInfo GetVSShaderInputInfo(vs_meta_func_t func) {
    glsl::VertexShaderMeta meta;
    glsl::VertexShaderInputInfo info;
    meta.outInfo = &info;
    func(meta);
    return info;
}

inline void SetupVertexArray(vs_meta_func_t func, VertexArraySetupArgs args) {
    glsl::VertexShaderMeta meta;
    meta.vertexArraySetup = std::move(args);
    func(meta);
}

} // namespace glsl

#endif

#endif // TYPES_GLSL