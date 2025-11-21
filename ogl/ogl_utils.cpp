#include "ogl_utils.hpp"
#include "../paths.hpp"

#include <glog/logging.h>
#include <fstream>
#include <sstream>

using namespace okami;

class GLShaderCache : public IGLShaderCache {
private:
    std::unordered_map<std::filesystem::path, std::unique_ptr<GLShader>, PathHash> m_shaderCache;

public:
    Expected<GLShader*> LoadShader(GLenum shaderType, const std::filesystem::path& path) override {
        auto it = m_shaderCache.find(path);
        if (it != m_shaderCache.end()) {
            return it->second.get();
        } else {
            auto shaderResult = okami::LoadShader(shaderType, path);
            if (!shaderResult) {
                return std::unexpected(shaderResult.error());
            } else {
                auto newIt = m_shaderCache.emplace_hint(it, path, std::make_unique<GLShader>(std::move(*shaderResult)));
                return newIt->second.get();
            }
        }
    }
};

std::unique_ptr<IGLShaderCache> okami::CreateGLShaderCache() {
    return std::make_unique<GLShaderCache>();
}

Expected<GLShader> okami::LoadShader(GLenum shaderType, const std::filesystem::path& shaderPath) {
    // Resolve shader path - check if it's absolute or relative to GLSL shaders directory
    std::filesystem::path fullShaderPath;
    if (shaderPath.is_absolute()) {
        fullShaderPath = shaderPath;
    } else {
        fullShaderPath = GetGLSLShaderPath(shaderPath);
    }
    
    // Check if file exists
    OKAMI_UNEXPECTED_RETURN_IF(!std::filesystem::exists(fullShaderPath), "Shader file does not exist: " + fullShaderPath.string());
    
    // Read shader source from file
    std::ifstream file(fullShaderPath);
    OKAMI_UNEXPECTED_RETURN_IF(!file.is_open(), "Failed to open shader file: " + fullShaderPath.string());
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string shaderSource = buffer.str();
    file.close();
    
    OKAMI_UNEXPECTED_RETURN_IF(shaderSource.empty(), "Shader file is empty: " + fullShaderPath.string());

    // Create shader object
    GLuint shaderId = glCreateShader(shaderType);
    OKAMI_UNEXPECTED_RETURN_IF(shaderId == 0, "Failed to create shader object");    
    
    // Compile shader
    const char* sourcePtr = shaderSource.c_str();
    glShaderSource(shaderId, 1, &sourcePtr, nullptr);
    glCompileShader(shaderId);
    
    // Check compilation status
    GLint compileStatus;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
    
    if (compileStatus == GL_FALSE) {
        // Get compilation error log
        GLint logLength;
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
        
        std::string errorLog;
        if (logLength > 0) {
            errorLog.resize(logLength);
            glGetShaderInfoLog(shaderId, logLength, nullptr, &errorLog[0]);
        }
        
        glDeleteShader(shaderId);
        return OKAMI_UNEXPECTED("Shader compilation failed for " + fullShaderPath.string() + ": " + errorLog);
    }
    
    LOG(INFO) << "Successfully compiled shader: " << fullShaderPath.string();
    return GLShader(shaderId);
}

Expected<GLProgram> okami::CreateProgram(ProgramShaders const& shaders) {
    // Create program object
    GLuint programId = glCreateProgram();
    OKAMI_UNEXPECTED_RETURN_IF(programId == 0, "Failed to create program object");
    
    // Attach shaders to program
    if (shaders.m_vertex) {
        glAttachShader(programId, shaders.m_vertex);
    }
    
    if (shaders.m_fragment) {
        glAttachShader(programId, *shaders.m_fragment);
    }
    
    if (shaders.m_geometry) {
        glAttachShader(programId, *shaders.m_geometry);
    }
    
    if (shaders.m_tessControl) {
        glAttachShader(programId, *shaders.m_tessControl);
    }
    
    if (shaders.m_tessEval) {
        glAttachShader(programId, *shaders.m_tessEval);
    }
    
    // Link program
    glLinkProgram(programId);
    
    // Check linking status
    GLint linkStatus;
    glGetProgramiv(programId, GL_LINK_STATUS, &linkStatus);
    
    if (linkStatus == GL_FALSE) {
        // Get linking error log
        GLint logLength;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
        
        std::string errorLog;
        if (logLength > 0) {
            errorLog.resize(logLength);
            glGetProgramInfoLog(programId, logLength, nullptr, &errorLog[0]);
        }
        
        glDeleteProgram(programId);
        return OKAMI_UNEXPECTED("Program linking failed: " + errorLog);
    }
    
    // Detach shaders after linking (optional, but good practice)
    if (shaders.m_vertex) {
        glDetachShader(programId, shaders.m_vertex);
    }
    
    if (shaders.m_fragment) {
        glDetachShader(programId, *shaders.m_fragment);
    }
    
    if (shaders.m_geometry) {
        glDetachShader(programId, *shaders.m_geometry);
    }
    
    if (shaders.m_tessControl) {
        glDetachShader(programId, *shaders.m_tessControl);
    }
    
    if (shaders.m_tessEval) {
        glDetachShader(programId, *shaders.m_tessEval);
    }
    
    LOG(INFO) << "Successfully created and linked shader program";
    return GLProgram(programId);
}

Expected<GLProgram> okami::CreateProgram(ProgramShaderPaths const& shaderPaths, IGLShaderCache& cache) {
    GLShader* vertex = nullptr;
    GLShader* fragment = nullptr;
    GLShader* geometry = nullptr;
    GLShader* tessControl = nullptr;
    GLShader* tessEval = nullptr;
    
    // Load vertex shader if path is provided
    auto vertexShader = cache.LoadShader(GL_VERTEX_SHADER, shaderPaths.m_vertex);
    if (!vertexShader) {
        return std::unexpected(vertexShader.error());
    }
    vertex = *vertexShader;

    // Load fragment shader if path is provided
    if (shaderPaths.m_fragment) {
        auto fragmentShader = cache.LoadShader(GL_FRAGMENT_SHADER, *shaderPaths.m_fragment);
        if (!fragmentShader) {
            return std::unexpected(fragmentShader.error());
        }
        fragment = *fragmentShader;
    }
    
    // Load geometry shader if path is provided
    if (shaderPaths.m_geometry) {
        auto geometryShader = cache.LoadShader(GL_GEOMETRY_SHADER, *shaderPaths.m_geometry);
        if (!geometryShader) {
            return std::unexpected(geometryShader.error());
        }
        geometry = *geometryShader;
    }
    
    // Load tessellation control shader if path is provided
    if (shaderPaths.m_tessControl) {
        auto tessControlShader = cache.LoadShader(GL_TESS_CONTROL_SHADER, *shaderPaths.m_tessControl);
        if (!tessControlShader) {
            return std::unexpected(tessControlShader.error());
        }
        tessControl = *tessControlShader;
    }
    
    // Load tessellation evaluation shader if path is provided
    if (shaderPaths.m_tessEval) {
        auto tessEvalShader = cache.LoadShader(GL_TESS_EVALUATION_SHADER, *shaderPaths.m_tessEval);
        if (!tessEvalShader) {
            return std::unexpected(tessEvalShader.error());
        }
        tessEval = *tessEvalShader;
    }
    
    // Create program using the loaded shaders
    return CreateProgram(ProgramShaders{
        .m_vertex = *vertex, 
        .m_fragment = fragment, 
        .m_geometry = geometry,
        .m_tessControl = tessControl,
        .m_tessEval = tessEval
    });
}

std::string GetGLErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR: return "No error";
        case GL_INVALID_ENUM: return "Invalid enum";
        case GL_INVALID_VALUE: return "Invalid value";
        case GL_INVALID_OPERATION: return "Invalid operation";
        case GL_STACK_OVERFLOW: return "Stack overflow";
        case GL_STACK_UNDERFLOW: return "Stack underflow";
        case GL_OUT_OF_MEMORY: return "Out of memory";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid framebuffer operation";
        default: return "Unknown error";
    }
}

GLint okami::GetUniformLocation(GLProgram const& program, const char* name, Error& error) {
    GLint location = glGetUniformLocation(program, name);
    if (location == -1) {
        auto errorCode = glGetError();
        error += OKAMI_ERROR("Uniform '" + std::string(name) + "' not found in program: " + GetGLErrorString(errorCode));
    }
    return location;
}

Error okami::GetGlError() {
    GLenum errorCode = glGetError();
    if (errorCode != GL_NO_ERROR) {
        return OKAMI_ERROR("OpenGL error: " + GetGLErrorString(errorCode));
    }
    return {};
}

GLint okami::ToOpenGL(AccessorComponentType type) {
    switch (type) {
        case AccessorComponentType::Double:
            return GL_DOUBLE;
        case AccessorComponentType::Float:
            return GL_FLOAT;
        case AccessorComponentType::Int:
            return GL_INT;
        case AccessorComponentType::UInt:
            return GL_UNSIGNED_INT;
        case AccessorComponentType::Short:
            return GL_SHORT;
        case AccessorComponentType::UShort:
            return GL_UNSIGNED_SHORT;
        case AccessorComponentType::Byte:
            return GL_BYTE;
        case AccessorComponentType::UByte:
            return GL_UNSIGNED_BYTE;
        default:
            throw std::invalid_argument("Unknown AccessorComponentType");
    }
}

void okami::SetupVertexArray(
    GLVertexArray const& vao,
    glsl::VertexShaderInputInfo const& inputInfo,
    GLuint vertexBuffer,
    std::optional<GLuint> indexBuffer) {
    glBindVertexArray(vao.get()); OKAMI_DEFER(glBindVertexArray(0));

    // Bind buffers
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    if (indexBuffer) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);
    }

    for (const auto& [location, attribInfo] : inputInfo.locationToAttrib) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(
            location,
            attribInfo.m_componentCount,
            ToOpenGL(attribInfo.m_componentType),
            attribInfo.m_type == okami::AttributeType::Color ? GL_TRUE : GL_FALSE,
            attribInfo.m_stride,
            static_cast<uint8_t*>(0) + attribInfo.m_offset
        );
        if (attribInfo.m_frequency == glsl::Frequency::PerInstance) {
            glVertexAttribDivisor(location, 1);
        } else {
            glVertexAttribDivisor(location, 0);
        }
    }
}

void OGLPipelineState::GetFromGL() {
    // Depth
    depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_DEPTH_FUNC, reinterpret_cast<GLint*>(&depthFunc));
    glGetBooleanv(GL_DEPTH_WRITEMASK, reinterpret_cast<GLboolean*>(&depthMask));

    // Blend
    blendEnabled = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, reinterpret_cast<GLint*>(&blendEquationRGB));
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, reinterpret_cast<GLint*>(&blendEquationAlpha));
    glGetIntegerv(GL_BLEND_SRC_RGB, reinterpret_cast<GLint*>(&blendSrcRGB));
    glGetIntegerv(GL_BLEND_DST_RGB, reinterpret_cast<GLint*>(&blendDstRGB));
    glGetIntegerv(GL_BLEND_SRC_ALPHA, reinterpret_cast<GLint*>(&blendSrcAlpha));
    glGetIntegerv(GL_BLEND_DST_ALPHA, reinterpret_cast<GLint*>(&blendDstAlpha));
    glGetFloatv(GL_BLEND_COLOR, &blendColor[0]);

    // Cull
    cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_CULL_FACE_MODE, reinterpret_cast<GLint*>(&cullFaceMode));

    // Stencil
    stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
    glGetIntegerv(GL_STENCIL_FUNC, reinterpret_cast<GLint*>(&stencilFunc));
    glGetIntegerv(GL_STENCIL_REF, &stencilRef);
    glGetIntegerv(GL_STENCIL_VALUE_MASK, reinterpret_cast<GLint*>(&stencilMask));
    glGetIntegerv(GL_STENCIL_FAIL, reinterpret_cast<GLint*>(&stencilFail));
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, reinterpret_cast<GLint*>(&stencilPassDepthFail));
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, reinterpret_cast<GLint*>(&stencilPassDepthPass));

    // Polygon Mode
    glGetIntegerv(GL_POLYGON_MODE, reinterpret_cast<GLint*>(&polygonMode));

    // Multisampling
    sampleAlphaToCoverageEnabled = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);

    // Program
    glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&program));
}

void OGLPipelineState::SetToGL() const {
    // Depth
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthFunc(depthFunc);
    glDepthMask(depthMask);

    // Blend
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendEquationSeparate(blendEquationRGB, blendEquationAlpha);
    glBlendFuncSeparate(blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha);
    glBlendColor(blendColor.r, blendColor.g, blendColor.b, blendColor.a);

    // Cull
    if (cullFaceEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glCullFace(cullFaceMode);

    // Stencil
    if (stencilTestEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    glStencilFunc(stencilFunc, stencilRef, stencilMask);
    glStencilOp(stencilFail, stencilPassDepthFail, stencilPassDepthPass);

    // Polygon Mode
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode);

    // Multisampling
    if (sampleAlphaToCoverageEnabled) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE); else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    // Program
    glUseProgram(program);
}