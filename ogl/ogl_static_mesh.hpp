#pragma once

#include "ogl_utils.hpp"
#include "ogl_geometry.hpp"

#include "../content.hpp"
#include "../storage.hpp"
#include "../transform.hpp"
#include "../renderer.hpp"

#include "shaders/scene.glsl"
#include "shaders/static_mesh.glsl"

namespace okami {
    class OGLStaticMeshRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        GLProgram m_program;

        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;
        UniformBuffer<glsl::StaticMeshInstance> m_instanceUBO;

        // Component storage and views
        StorageModule<StaticMeshComponent>* m_storage = nullptr;
        IComponentView<Transform>* m_transformView = nullptr;
        OGLGeometryManager* m_geometryManager = nullptr;

        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;
        void ShutdownImpl(InitContext const& context) override;

        Error ProcessFrameImpl(Time const& time, ExecutionContext const& context) override;
        Error MergeImpl(MergeContext const& context) override;
    
    public:
        OGLStaticMeshRenderer(OGLGeometryManager* geometryManager);

        Error Pass(OGLPass const& pass) override;

        std::string GetName() const override;
    };
}