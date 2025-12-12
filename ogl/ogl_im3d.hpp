#pragma once

#include "../renderer.hpp"
#include "../im3d.hpp"

#include "ogl_utils.hpp"

#include "shaders/im3d.glsl"

namespace okami {
    class OGLIm3D final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        IIm3dProvider* m_dataProvider;

        OGLPipelineState m_pipelineState;
        GLProgram m_program;

        UploadVertexBuffer<glsl::Im3dVertex> m_vertexBuffer; // Instance buffer for sprite data

        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;

        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;
        void ShutdownImpl(InitContext const& context) override;

        Error ProcessFrameImpl(Time const& time, ExecutionContext const& context) override;
        Error MergeImpl(MergeContext const& context) override;
    
    public:
        Error Pass(OGLPass const& pass) override;

        std::string GetName() const override;
    };
}