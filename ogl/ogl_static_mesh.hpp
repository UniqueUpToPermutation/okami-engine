#pragma once

#include "ogl_utils.hpp"
#include "ogl_geometry.hpp"
#include "ogl_material.hpp"

#include "../content.hpp"
#include "../transform.hpp"
#include "../renderer.hpp"

#include "shaders/scene.glsl"
#include "shaders/static_mesh.glsl"

namespace okami {
    class OGLStaticMeshRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        OGLPipelineState m_pipelineState;

        UploadVertexBuffer<glsl::StaticMeshInstance> m_instanceVBO;

        enum class BufferBindingPoints : GLint {
            SceneGlobals,
            Count
        };

        static constexpr GLint kShadowMapUnit = 2;

        // The fallback material used when a StaticMeshComponent has no material set.
        MaterialHandle m_defaultMaterial;

        // Depth-only program compiled from static_mesh_depth.vs/fs.
        GLProgram m_depthProgram;

        OGLGeometryManager*          m_geometryManager      = nullptr;
        IOGLSceneGlobalsProvider*    m_sceneGlobalsProvider = nullptr;
        IOGLDepthPassProvider*       m_depthPassProvider    = nullptr;

        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

    public:
        OGLStaticMeshRenderer(OGLGeometryManager* geometryManager);

        Error Pass(entt::registry const& registry, OGLPass const& pass) override;

        std::string GetName() const override;
    };
}
