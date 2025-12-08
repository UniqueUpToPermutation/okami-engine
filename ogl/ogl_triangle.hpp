#pragma once

#include "../renderer.hpp"
#include "../storage.hpp"
#include "../transform.hpp"

#include "ogl_utils.hpp"

namespace okami {
    class OGLTriangleRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        GLProgram m_program;
        GLVertexArray m_vao;  // Vertex Array Object

        GLint u_world = -1;
        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;

        StorageModule<DummyTriangleComponent>* m_storage = nullptr;
        IComponentView<Transform>* m_transformView = nullptr;

        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl(ModuleInterface&) override;
    
    public:
        OGLTriangleRenderer();

        Error Pass(OGLPass const& pass) override;

        std::string GetName() const override;
    };
}