#pragma once

#include "ogl_utils.hpp"

#include "../module.hpp"
#include "../renderer.hpp"

#include "shaders/scene.glsl"

namespace okami {
    // Owns the shadow-map depth texture array and layered framebuffer.
    // Call BeginDepthPass() before rendering depth modules, EndDepthPass() after.
    // Implements IOGLDepthPassProvider so other modules can bind the cascade UBO
    // and sample the shadow map array texture.
    class OGLDepthPass final :
        public EngineModule,
        public IOGLDepthPassProvider {
    public:
        static constexpr int kNumCascades = NUM_SHADOW_CASCADES;
        int m_shadowMapSize = 2048; // set from ShadowConfig at startup

        GLTexture     m_shadowMapTexture;   // GL_TEXTURE_2D_ARRAY, kNumCascades layers
        GLFramebuffer m_shadowFBO;          // layered FBO (all cascade layers attached)

        // UBO written in BeginDepthPass, bound by the depth-pass geometry shader.
        UniformBuffer<glsl::ShadowCascadesBlock> m_cascadesUBO;

        // Last values written in BeginDepthPass, read by OGLSceneModule.
        glsl::ShadowCascadesBlock m_currentCascades = {};
        glm::vec4                 m_currentSplits   = {};

        // IOGLDepthPassProvider
        UniformBuffer<glsl::ShadowCascadesBlock> const& GetCascadesBuffer() const override {
            return m_cascadesUBO;
        }
        glsl::ShadowCascadesBlock const& GetCurrentCascades() const override {
            return m_currentCascades;
        }
        glm::vec4 GetCurrentCascadeSplits() const override {
            return m_currentSplits;
        }
        GLuint GetDepthTexture() const override { return m_shadowMapTexture.get(); }

        // Saves current FBO/viewport, uploads the cascade data, binds the layered
        // shadow FBO, sets the viewport, and clears the depth buffer.
        Error BeginDepthPass(glsl::ShadowCascadesBlock const& cascades,
                             glm::vec4 const& cascadeSplits);

        // Restores FBO and viewport saved by BeginDepthPass.
        Error EndDepthPass();

    protected:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

    private:
        GLint m_prevFBO = 0;
        GLint m_prevViewport[4] = {};

    public:
        std::string GetName() const override { return "OGL Depth Pass"; }
    };
}
