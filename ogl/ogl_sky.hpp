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
    class OGLSkyRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        OGLPipelineState m_pipelineState;

        enum class BufferBindingPoints : GLint {
            SceneGlobals,
            Count
        };

        // The fallback material used when a SkyComponent has no material set.
        MaterialHandle m_defaultMaterial;

        IOGLSceneGlobalsProvider* m_sceneGlobalsProvider = nullptr;

        Error StartupImpl(InitContext const& context) override;

    public:
        Error Pass(entt::registry const& registry, OGLPass const& pass) override;

        std::string GetName() const override;
    };
}
