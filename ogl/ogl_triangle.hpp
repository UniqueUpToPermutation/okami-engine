#pragma once

#include "../renderer.hpp"
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
        IOGLSceneGlobalsProvider* m_sceneGlobalsProvider = nullptr;

        enum class BufferBindingPoints : GLint {
            SceneGlobals,
            Count
        };

        Error RegisterImpl(InterfaceCollection&) override;
        Error StartupImpl(InitContext const&) override;
        void ShutdownImpl(InitContext const&) override;
    
    public:
        Error Pass(entt::registry const& registry, OGLPass const& pass) override;

        std::string GetName() const override;
    };
}