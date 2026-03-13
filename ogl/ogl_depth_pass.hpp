#pragma once

#include "ogl_utils.hpp"

#include "../module.hpp"

#include "shaders/scene.glsl"

namespace okami {
    // Owns the shadow-map depth texture and framebuffer.
    // Call BeginDepthPass() before rendering depth modules, EndDepthPass() after.
    // Implements IOGLDepthPassProvider so other modules can bind the light-space
    // camera UBO and sample the shadow map texture.
    class OGLDepthPass final :
        public EngineModule,
        public IOGLDepthPassProvider {
    public:
        static constexpr int kShadowMapSize = 2048;

        GLTexture     m_shadowMapTexture;
        GLFramebuffer m_shadowFBO;

        // Light-space camera UBO, written once per frame in BeginDepthPass.
        UniformBuffer<glsl::CameraGlobals> m_shadowUBO;

        // IOGLDepthPassProvider
        UniformBuffer<glsl::CameraGlobals> const& GetCameraGlobalsBuffer() const override {
            return m_shadowUBO;
        }
        GLuint GetDepthTexture() const override { return m_shadowMapTexture.get(); }

        // Saves current FBO/viewport, writes the light-space camera globals,
        // binds the shadow FBO, sets the viewport, and clears the depth buffer.
        Error BeginDepthPass(glsl::CameraGlobals const& cameraGlobals);

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
