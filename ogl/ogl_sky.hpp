#pragma once

#include "ogl_utils.hpp"
#include "ogl_geometry.hpp"
#include "ogl_material.hpp"

#include "../content.hpp"
#include "../transform.hpp"
#include "../renderer.hpp"
#include "../material.hpp"
#include "../sky.hpp"

#include "shaders/scene.glsl"

namespace okami {
    class OGLSkyDefaultMaterialManager final :
        public MaterialModuleBase<SkyDefaultMaterial, EmptyMaterialImpl>,
        public IOGLMaterialManagerT<SkyDefaultMaterial, OGLVertexShaderOutput::Sky> {
    public:
        Error RegisterImpl(InterfaceCollection& interfaces) override;

        EmptyMaterialImpl CreateImpl(SkyDefaultMaterial const& material) override;
		void DestroyImpl(EmptyMaterialImpl& impl) override;

        ProgramShaderPaths GetShaderPaths() const override;
        Error Bind(MaterialHandle handle, OGLMaterialBindParams const& params) const override;
        Error OnProgramCreated(GLProgram const& program, 
            OGLMaterialBindParams const& params) const override;
    };

    class OGLSkyRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        OGLPipelineState m_pipelineState;
        std::type_index m_defaultMaterialType = typeid(SkyDefaultMaterial);

        enum class BufferBindingPoints : GLint {
            SceneGlobals,
            Count
        };

        enum class TextureBindingPoints : GLint {
            Count = 0
        };

        struct MaterialImpl {
            GLProgram m_program;
            IOGLMaterialManager* m_manager = nullptr;
        };

        std::unordered_map<std::type_index, MaterialImpl> m_programs;
        IOGLSceneGlobalsProvider* m_sceneGlobalsProvider = nullptr;

        Error StartupImpl(InitContext const& context) override;

        constexpr static OGLMaterialBindParams GetMaterialBindParams() {
            return OGLMaterialBindParams{
                .m_startBufferBindPoint = static_cast<GLint>(BufferBindingPoints::Count),
                .m_startTextureBindPoint = static_cast<GLint>(TextureBindingPoints::Count),
            };
        }

    public:
        Error Pass(entt::registry const& registry, OGLPass const& pass) override;

        std::string GetName() const override;
    };
}