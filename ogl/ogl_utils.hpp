#pragma once

#include "../common.hpp"

#include <glad/gl.h>

#include <filesystem>

#include <glm/glm.hpp>

namespace okami {

    struct OGLPass {
        glm::ivec2 m_viewport = {0, 0};
        glm::mat4 m_projection = glm::mat4(1.0f);
        glm::mat4 m_view = glm::mat4(1.0f);
        glm::mat4 m_viewProjection = glm::mat4(1.0f);
        glm::vec3 m_cameraPosition = glm::vec3(0.0f);
        glm::vec3 m_cameraDirection = glm::vec3(0.0f, 0.0f, -1.0f);
    };

    class IOGLRenderModule {
    public:
        virtual ~IOGLRenderModule() = default;
        virtual Error Pass(OGLPass const& pass) = 0;
    };

    template<typename Deleter>
    class GLObject {
    private:
        GLuint id_;
        Deleter deleter_;

    public:
        GLObject() : id_(0) {}
        
        explicit GLObject(GLuint id) : id_(id) {}
        
        // Delete copy constructor and assignment
        OKAMI_NO_COPY(GLObject);
        
        ~GLObject() {
            if (id_ != 0) {
                deleter_(id_);
            }
        }
        
        GLObject(GLObject&& other) noexcept : id_(other.id_) {
            other.id_ = 0;
        }
        
        GLObject& operator=(GLObject&& other) noexcept {
            if (this != &other) {
                if (id_ != 0) {
                    deleter_(id_);
                }
                id_ = other.id_;
                other.id_ = 0;
            }
            return *this;
        }
        
        GLuint get() const { return id_; }
        GLuint release() {
            GLuint temp = id_;
            id_ = 0;
            return temp;
        }
        
        void reset(GLuint new_id = 0) {
            if (id_ != 0) {
                deleter_(id_);
            }
            id_ = new_id;
        }

        GLuint* ptr() { return &id_; }
        
        operator bool() const { return id_ != 0; }
        operator GLuint() const { return id_; }
    };

    struct BufferDeleter {
        void operator()(GLuint id) const {
            glDeleteBuffers(1, &id);
        }
    };

    struct TextureDeleter {
        void operator()(GLuint id) const {
            glDeleteTextures(1, &id);
        }
    };

    struct ShaderDeleter {
        void operator()(GLuint id) const {
            glDeleteShader(id);
        }
    };

    struct ProgramDeleter {
        void operator()(GLuint id) const {
            glDeleteProgram(id);
        }
    };

    struct FramebufferDeleter {
        void operator()(GLuint id) const {
            glDeleteFramebuffers(1, &id);
        }
    };

    struct VertexArrayDeleter {
        void operator()(GLuint id) const {
            glDeleteVertexArrays(1, &id);
        }
    };

    using GLBuffer = GLObject<BufferDeleter>;
    using GLTexture = GLObject<TextureDeleter>;
    using GLShader = GLObject<ShaderDeleter>;
    using GLProgram = GLObject<ProgramDeleter>;
    using GLFramebuffer = GLObject<FramebufferDeleter>;
    using GLVertexArray = GLObject<VertexArrayDeleter>;

    Expected<GLShader> LoadShader(GLenum shaderType, const std::filesystem::path& shaderPath);

    class IGLShaderCache {
    public:
        virtual ~IGLShaderCache() = default;
        virtual Expected<GLShader*> LoadShader(GLenum shaderType, const std::filesystem::path& path) = 0;
    };

    std::unique_ptr<IGLShaderCache> CreateGLShaderCache();

    struct ProgramShaders {
        GLShader const& m_vertex; 
        GLShader const* m_fragment = nullptr; // Optional
        GLShader const* m_geometry = nullptr; // Optional
        GLShader const* m_tessControl = nullptr; // Optional
        GLShader const* m_tessEval = nullptr; // Optional
    };

    struct ProgramShaderPaths {
        std::filesystem::path m_vertex; 
        std::optional<std::filesystem::path> m_fragment; // Optional
        std::optional<std::filesystem::path> m_geometry; // Optional
        std::optional<std::filesystem::path> m_tessControl; // Optional
        std::optional<std::filesystem::path> m_tessEval; // Optional
    };

    Expected<GLProgram> CreateProgram(ProgramShaders const& shaders);
    Expected<GLProgram> CreateProgram(ProgramShaderPaths const& shaderPaths, IGLShaderCache& cache);

    GLint GetUniformLocation(GLProgram const& program, const char* name, Error& error);
    Error GetGlError();


}