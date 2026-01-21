#pragma once

#include "ogl_utils.hpp"
#include "ogl_texture.hpp"

#include "../material.hpp"

namespace okami {
    struct EmptyMaterialImpl {};

    struct OGLMaterialBindParams {
        GLint m_startBufferBindPoint = -1;
        GLint m_startTextureBindPoint = -1;
    };

    class IOGLMaterialManager {
    public:
        virtual ~IOGLMaterialManager() = default;

        virtual ProgramShaderPaths GetShaderPaths() const = 0;
        virtual std::type_index GetMaterialType() const = 0; 
        virtual Error Bind(MaterialHandle handle,
            OGLMaterialBindParams const& params) const = 0;
        virtual Error OnProgramCreated(GLProgram const& program, 
            OGLMaterialBindParams const& params) const = 0;

        inline std::string GetMaterialName() const {
            return GetMaterialType().name();
        }
    };

    template <typename T>
    class IOGLMaterialManagerT : public IOGLMaterialManager {
    public:
        virtual ~IOGLMaterialManagerT() = default;

        std::type_index GetMaterialType() const override {
            return std::type_index(typeid(T));
        }
    };

    class OGLDefaultMaterialManager final :
        public EngineModule,
        public IOGLMaterialManagerT<void> {
    public:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

        ProgramShaderPaths GetShaderPaths() const override;
        Error Bind(MaterialHandle handle, OGLMaterialBindParams const& params) const override;
        Error OnProgramCreated(GLProgram const& program, 
            OGLMaterialBindParams const& params) const override;
    };

    class OGLBasicTexturedMaterialManager final :
        public MaterialModuleBase<BasicTexturedMaterial, EmptyMaterialImpl>,
        public IOGLMaterialManagerT<BasicTexturedMaterial> {
    protected:
        OGLTextureManager* m_textureManager = nullptr;

        enum class TextureUnits : GLint {
            ColorTexture = 0
        };
    
    public:
        inline OGLBasicTexturedMaterialManager(OGLTextureManager* textureManager)
            : m_textureManager(textureManager) {}

        Error RegisterImpl(InterfaceCollection& interfaces) override;

        EmptyMaterialImpl CreateImpl(BasicTexturedMaterial const& material) override;
		void DestroyImpl(EmptyMaterialImpl& impl) override;

        ProgramShaderPaths GetShaderPaths() const override;
        Error Bind(MaterialHandle handle, OGLMaterialBindParams const& params) const override;
        Error OnProgramCreated(GLProgram const& program, 
            OGLMaterialBindParams const& params) const override;
    };
}