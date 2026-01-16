#pragma once

#include "ogl_utils.hpp"
#include "ogl_texture.hpp"

#include "../material.hpp"

namespace okami {
    class OGLMaterialBindings {
    public:
        virtual Error FetchBindings(GLProgram const& program) = 0;
        virtual bool Bind(MaterialHandle handle, OGLTextureManager const& textureManager) const = 0;
    };

    class IOGLMaterialManager {
    public:
        virtual ~IOGLMaterialManager() = default;

        virtual ProgramShaderPaths GetShaderPaths() const = 0;
        virtual std::unique_ptr<OGLMaterialBindings> CreateBinding() const = 0;
        virtual std::type_index GetMaterialType() const = 0; 
    };

    class OGLBasicTexturedMaterialManager;

    class OGLBasicTexturedMaterialBindings : public OGLMaterialBindings {
    private:
        GLint m_diffuseMap = -1;
        GLint m_color = -1;
        OGLBasicTexturedMaterialManager const* m_manager = nullptr;

    public:
        inline OGLBasicTexturedMaterialBindings(OGLBasicTexturedMaterialManager const* manager) : m_manager(manager) {} 

        Error FetchBindings(GLProgram const& program) override;
        bool Bind(MaterialHandle handle, OGLTextureManager const& textureManager) const override;
    };

    class OGLBasicTexturedMaterialManager final :
        public MaterialModuleBase<BasicTexturedMaterial, std::monostate>,
        public IOGLMaterialManager {
    public:
        ProgramShaderPaths GetShaderPaths() const override;
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        std::unique_ptr<OGLMaterialBindings> CreateBinding() const override;
        std::type_index GetMaterialType() const override;
    };
}