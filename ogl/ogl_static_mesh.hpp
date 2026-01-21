#pragma once

#include "ogl_utils.hpp"
#include "ogl_geometry.hpp"
#include "ogl_material.hpp"

#include "../content.hpp"
#include "../storage.hpp"
#include "../transform.hpp"
#include "../renderer.hpp"
#include "../material.hpp"

#include "shaders/scene.glsl"
#include "shaders/static_mesh.glsl"

namespace okami {
    class OGLStaticMeshRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        OGLPipelineState m_pipelineState;
        std::type_index m_defaultMaterialType = typeid(void);

        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;
        UniformBuffer<glsl::StaticMeshInstance> m_instanceUBO;

        enum class BufferBindingPoints : GLint {
            SceneGlobals,
            StaticMeshInstance,
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

        // Component storage and views
        StorageModule<StaticMeshComponent>* m_storage = nullptr;
        IComponentView<Transform>* m_transformView = nullptr;
        OGLGeometryManager* m_geometryManager = nullptr;

        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;
        void ShutdownImpl(InitContext const& context) override;

        constexpr static OGLMaterialBindParams GetMaterialBindParams() {
            return OGLMaterialBindParams{
                .m_startBufferBindPoint = static_cast<GLint>(BufferBindingPoints::Count),
                .m_startTextureBindPoint = static_cast<GLint>(TextureBindingPoints::Count),
            };
        }

    public:
        OGLStaticMeshRenderer(OGLGeometryManager* geometryManager);

        Error Pass(OGLPass const& pass) override;

        std::string GetName() const override;
    };
}