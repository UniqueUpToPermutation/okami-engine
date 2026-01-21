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
        OGLPipelineState m_pipelineState;

        GLProgram m_program;
        GLVertexArray m_vao;  // Vertex Array Object

        GLint u_world = -1;
        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;

        enum class BufferBindingPoints : GLint {
            SceneGlobals,
            Count
        };

        StorageModule<DummyTriangleComponent>* m_storage = nullptr;
        IComponentView<Transform>* m_transformView = nullptr;

        Error RegisterImpl(InterfaceCollection&) override;
        Error StartupImpl(InitContext const&) override;
        void ShutdownImpl(InitContext const&) override;
    
    public:
        OGLTriangleRenderer();

        Error Pass(OGLPass const& pass) override;

        std::string GetName() const override;
    };
}