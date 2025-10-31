#pragma once

#include "ogl_utils.hpp"
#include "ogl_geometry.hpp"

#include "../content.hpp"
#include "../storage.hpp"
#include "../transform.hpp"
#include "../renderer.hpp"

namespace okami {
    class OGLStaticMeshRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        GLProgram m_program;
        
        // Uniform locations
        GLint u_viewProj = -1;
        GLint u_texture = -1;

        // Component storage and views
        StorageModule<StaticMeshComponent>* m_storage = nullptr;
        IComponentView<Transform>* m_transformView = nullptr;
        OGLGeometryManager* m_geometryManager = nullptr;

        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl() override;
    
    public:
        OGLStaticMeshRenderer(OGLGeometryManager* geometryManager);

        Error Pass(OGLPass const& pass) override;

        std::string_view GetName() const override;
    };
}