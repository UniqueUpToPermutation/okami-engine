#pragma once

#include "../common.hpp"
#include "../sizer.hpp"

#include <glad/gl.h>

#include <filesystem>

#include <glm/glm.hpp>

#include "../log.hpp"

#include "shaders/types.glsl"
#include "shaders/scene.glsl"

#include "entt/entity/registry.hpp"

#define GET_GL_ERROR() []() -> Error { \
    Error glErr = okami::GetGlError(); \
    glErr.m_line = __LINE__; \
    glErr.m_file = __FILE__; \
    return glErr; \
}()

#ifndef NDEBUG
#define OKAMI_CHK_GL \
    { \
        Error glErr = GET_GL_ERROR(); \
        if (glErr.IsError()) { \
            OKAMI_LOG_ERROR(glErr.Str()); \
            return glErr; \
        } \
    } 
#else
#define OKAMI_CHK_GL
#endif

#ifndef NDEBUG
#define OKAMI_LOG_GL \
    { \
        Error glErr = GET_GL_ERROR(); \
        if (glErr.IsError()) { \
            OKAMI_LOG_ERROR(glErr.Str()); \
        } \
    }
#else
#define OKAMI_LOG_GL
#endif

namespace okami {
    enum class OGLPassType {
        Shadow,
        Forward,
        Transparent,
        PostProcess,
        _2D,
    };

    struct OGLPass;

    struct OGL2DPayload {
        int m_layer = 0;
        std::function<Error(entt::registry const& registry, OGLPass const& pass)> m_payload = nullptr;
    };

    struct OGLPass {
        OGLPassType m_type = OGLPassType::Forward;
        std::vector<OGL2DPayload>* m_2DOutputs = nullptr;
    };

    class IOGLRenderModule {
    public:
        virtual ~IOGLRenderModule() = default;
        virtual Error Pass(entt::registry const& registry, OGLPass const& pass) = 0;
    };

    GLint ToOpenGL(AccessorComponentType type);

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

    void SetupVertexArray(
        GLVertexArray const& vao,
        glsl::VertexShaderInputInfo const& inputInfo,
        GLuint vertexBuffer,
        std::optional<GLuint> indexBuffer);

    template <typename InstanceType>
	class BufferWriteMap {
	private:
		InstanceType* m_data = nullptr;
        GLBuffer* m_resource = nullptr;

	public:
		BufferWriteMap() = default;
		BufferWriteMap(BufferWriteMap&& map) : m_data(map.m_data), m_resource(map.m_resource) {
            map.m_data = nullptr;
            map.m_resource = nullptr;
        }
		BufferWriteMap& operator=(BufferWriteMap&& map) {
            std::swap(m_data, map.m_data);
            std::swap(m_resource, map.m_resource);
            return *this;
        }
		BufferWriteMap(BufferWriteMap const&) = delete;
		BufferWriteMap& operator=(BufferWriteMap const&) = delete;

		inline InstanceType* Data() {
			return static_cast<InstanceType*>(m_data);
		}

		static Expected<BufferWriteMap<InstanceType>> Map(
			GLBuffer& resource) {
			BufferWriteMap<InstanceType> result;

            OKAMI_UNEXPECTED_RETURN_IF(!resource, "Resource not initialized");

			result.m_resource = &resource;

            glBindBuffer(GL_ARRAY_BUFFER, result.m_resource->get());
            OKAMI_DEFER(glBindBuffer(GL_ARRAY_BUFFER, 0));
            void* data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            auto err = GET_GL_ERROR();
            OKAMI_UNEXPECTED_RETURN(err);

            OKAMI_UNEXPECTED_RETURN_IF(!data, "Failed to map buffer resource");

			result.m_data = reinterpret_cast<InstanceType*>(data);
			return result;
		}

		InstanceType& operator[](size_t i) {
			return m_data[i];
		}

		InstanceType& At(size_t i) {
			return m_data[i];
		}

		InstanceType& operator*() {
			return *m_data;
		}

		~BufferWriteMap() {
            if (m_data) {
                glBindBuffer(GL_ARRAY_BUFFER, m_resource->get()); OKAMI_LOG_GL;
                OKAMI_DEFER(glBindBuffer(GL_ARRAY_BUFFER, 0)); OKAMI_LOG_GL;
                glUnmapBuffer(GL_ARRAY_BUFFER); OKAMI_LOG_GL;

                m_data = nullptr;
            }
		}
	};

    template <typename T>
    class UploadBuffer {
    private:
        GLBuffer m_buffer;
		Sizer m_sizer;

    public:
        GLuint GetBuffer() const {
            return m_buffer.get();
        }

        size_t SizeOf(size_t elementCount) const {
			return elementCount * sizeof(T);
		}

        Error Resize(size_t elementCount, bool* wasResized = nullptr) {
			// Release the current buffer
			m_buffer.reset();

			if (elementCount == 0) {
				return {};
			}

            glGenBuffers(1, m_buffer.ptr()); OKAMI_CHK_GL;
            glBindBuffer(GL_ARRAY_BUFFER, m_buffer.get()); 
            OKAMI_DEFER(glBindBuffer(GL_ARRAY_BUFFER, 0)); OKAMI_CHK_GL;
            glBufferData(GL_ARRAY_BUFFER, SizeOf(elementCount), nullptr, GL_DYNAMIC_DRAW); OKAMI_CHK_GL;

            if (wasResized) {
                *wasResized = true;
            }

			return {};
		}

        static Expected<UploadBuffer<T>> Create(
            size_t elementCount = 1) {
			UploadBuffer<T> result;

			result.m_sizer.Reset(result.SizeOf(elementCount));
            auto err = result.Resize(elementCount);
            OKAMI_UNEXPECTED_RETURN(err);

			return result;
		}

        Error Reserve(size_t elementCount, bool* wasResized = nullptr) {
			auto sizeChanged = m_sizer.GetNextSize(SizeOf(elementCount));

			if (sizeChanged) {
				return Resize(elementCount, wasResized);
			}
			else {
				return {};
			}
		}

        size_t GetElementCount() const {
			return m_sizer.m_currentSize;
		}

        Expected<BufferWriteMap<T>> Map() {
			return BufferWriteMap<T>::Map(m_buffer);
		}
    };

    template <typename T>
    class UploadVertexBuffer {
    private:
		UploadBuffer<T> m_buffer;
        GLVertexArray m_vao;
        glsl::VertexShaderInputInfo m_vsInputInfo;

    public:
        GLuint GetVertexArray() const {
            return m_vao.get();
        }

        GLuint GetBuffer() const {
            return m_buffer.GetBuffer();
        }

        size_t SizeOf(size_t elementCount) const {
			return m_buffer.SizeOf(elementCount);
		}

        Error Resize(size_t elementCount) {
            auto err = m_buffer.Resize(elementCount);
            OKAMI_ERROR_RETURN(err);
            
            if (elementCount == 0) {
                return {};
            }

            glBindBuffer(GL_ARRAY_BUFFER, m_buffer.GetBuffer()); 
            OKAMI_DEFER(glBindBuffer(GL_ARRAY_BUFFER, 0));

            // Setup vertex attributes after buffer is bound
            SetupVertexArray(m_vao, m_vsInputInfo, m_buffer.GetBuffer(), std::nullopt);
                
            OKAMI_CHK_GL;

			return {};
		}

        static Expected<UploadVertexBuffer<T>> Create(
			glsl::VertexShaderInputInfo const& vsInputInfo,
            size_t elementCount = 1) {
			UploadVertexBuffer<T> result;
			result.m_vsInputInfo = vsInputInfo;
            auto buffer = UploadBuffer<T>::Create(elementCount);
            OKAMI_UNEXPECTED_RETURN(buffer);
            result.m_buffer = std::move(*buffer);
			
            glGenVertexArrays(1, result.m_vao.ptr());
            auto err = GET_GL_ERROR();
            OKAMI_UNEXPECTED_RETURN(err);

            err = result.Resize(elementCount);
            OKAMI_UNEXPECTED_RETURN(err);

			return result;
		}

        Error Reserve(size_t elementCount) {
			bool wasResized = false;
            auto err = m_buffer.Reserve(elementCount, &wasResized);
            OKAMI_ERROR_RETURN(err);
            if (wasResized) {
                return Resize(m_buffer.GetElementCount());
            }
            return {};
		}

		size_t GetElementCount() const {
			return m_buffer.GetElementCount();
		}

        Expected<BufferWriteMap<T>> Map() {
			return m_buffer.Map();
		}
    };

    template<typename T>
    concept ConvertibleToGLint =
        std::convertible_to<T, GLint> ||
        (std::is_enum_v<T> && std::convertible_to<std::underlying_type_t<T>, GLint>);

    Error AssignBufferBindingPoint(
        GLProgram const& program,
        std::string_view blockName,
        ConvertibleToGLint auto bindingPoint) {
#ifndef NDEBUG
        GLint currentProgramId = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgramId);
        OKAMI_ASSERT(currentProgramId == static_cast<GLint>(program.get()),
            "Program must be bound before assigning buffer binding points");
#endif

        GLint blockIndex = glGetUniformBlockIndex(program.get(), blockName.data());
        OKAMI_ERROR_RETURN_IF(blockIndex == GL_INVALID_INDEX,
            "Uniform block '" + std::string(blockName) + "' not found in program");
        
        // Assign binding point
        glUniformBlockBinding(program, blockIndex, static_cast<GLint>(bindingPoint));
        return GET_GL_ERROR();
    }

    Error AssignBufferBindingPoint(
        GLProgram const& program,
        std::string_view blockName,
        ConvertibleToGLint auto bindingPointStart,
        ConvertibleToGLint auto bindingPointOffset) {
        return AssignBufferBindingPoint(
            program,
            blockName,
            static_cast<GLint>(bindingPointStart) + static_cast<GLint>(bindingPointOffset));
    }

    Error AssignTextureBindingPoint(
        GLProgram const& program,
        std::string_view samplerName,
        ConvertibleToGLint auto bindingPoint) {
#ifndef NDEBUG
        GLint currentProgramId = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgramId);
        OKAMI_ASSERT(currentProgramId == static_cast<GLint>(program.get()),
            "Program must be bound before assigning buffer binding points");
#endif

        GLint location = glGetUniformLocation(program.get(), samplerName.data());
        OKAMI_ERROR_RETURN_IF(location == -1,
            "Sampler uniform '" + std::string(samplerName) + "' not found in program");
        glUniform1i(location, static_cast<GLint>(bindingPoint));

        // If you get this error, check if the program is currently bound
        return GET_GL_ERROR();
    }

    Error AssignTextureBindingPoint(
        GLProgram const& program,
        std::string_view samplerName,
        ConvertibleToGLint auto bindingPointStart,
        ConvertibleToGLint auto bindingPointOffset) {
        return AssignTextureBindingPoint(
            program,
            samplerName,
            static_cast<GLint>(bindingPointStart) + static_cast<GLint>(bindingPointOffset));
    }

    template <typename T>
    class UniformBuffer {
    private:
        GLBuffer m_buffer;
        GLint m_bindingPoint = -1;

    public:
        GLuint GetBuffer() const {
            return m_buffer.get();
        }

        static Expected<UniformBuffer<T>> Create(size_t count = 1) {
            UniformBuffer<T> result;
            glGenBuffers(1, result.m_buffer.ptr()); 
            OKAMI_UNEXPECTED_RETURN_IF(!result.m_buffer, "Failed to create uniform buffer");

            glBindBuffer(GL_UNIFORM_BUFFER, result.m_buffer.get());
            OKAMI_DEFER(glBindBuffer(GL_UNIFORM_BUFFER, 0)); 
            OKAMI_UNEXPECTED_RETURN_IF(!result.m_buffer, "Failed to bind uniform buffer");

            glBufferData(GL_UNIFORM_BUFFER, sizeof(T) * count, nullptr, GL_DYNAMIC_DRAW); 
            OKAMI_UNEXPECTED_RETURN_IF(!result.m_buffer, "Failed to allocate uniform buffer data");

            return result;
        }

        Error Bind(ConvertibleToGLint auto bindingPoint) const {
            glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLint>(bindingPoint), m_buffer.get()); 
            OKAMI_CHK_GL;
            return {};
        }

        Error Write(const T& data) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_buffer.get()); 
            OKAMI_DEFER(glBindBuffer(GL_UNIFORM_BUFFER, 0)); 
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(T), &data); 
            OKAMI_CHK_GL;
            return {};
        }

        BufferWriteMap<T> Map() {
            return BufferWriteMap<T>::Map(m_buffer);
        }
    };

    class IOGLSceneGlobalsProvider {
    public:
        virtual ~IOGLSceneGlobalsProvider() = default;
        virtual UniformBuffer<glsl::SceneGlobals> const& GetSceneGlobalsBuffer() const = 0;
    };

    struct OGLPipelineState {
        // Depth Testing
        bool depthTestEnabled = false;
        GLenum depthFunc = GL_LESS;
        bool depthMask = true;

        // Blending
        bool blendEnabled = false;
        GLenum blendEquationRGB = GL_FUNC_ADD;
        GLenum blendEquationAlpha = GL_FUNC_ADD;
        GLenum blendSrcRGB = GL_ONE;
        GLenum blendDstRGB = GL_ZERO;
        GLenum blendSrcAlpha = GL_ONE;
        GLenum blendDstAlpha = GL_ZERO;
        glm::vec4 blendColor = glm::vec4(0.0f);

        // Culling
        bool cullFaceEnabled = false;
        GLenum cullFaceMode = GL_BACK;

        // Stencil Testing
        bool stencilTestEnabled = false;
        GLenum stencilFunc = GL_ALWAYS;
        GLint stencilRef = 0;
        GLuint stencilMask = ~0u;
        GLenum stencilFail = GL_KEEP;
        GLenum stencilPassDepthFail = GL_KEEP;
        GLenum stencilPassDepthPass = GL_KEEP;

        // Polygon Mode
        GLenum polygonMode = GL_FILL;

        // Multisampling
        bool sampleAlphaToCoverageEnabled = false;

        // Get current state from OpenGL
        void GetFromGL();

        // Set state to OpenGL
        void SetToGL() const;
    };
}