#include "ogl_depth_pass.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLDepthPass::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IOGLDepthPassProvider>(this);
    return {};
}

Error OGLDepthPass::StartupImpl(InitContext const& context) {
    Error err;

    // Create the persistent shadow-pass UBO (camera matrices only).
    {
        auto ubo = UniformBuffer<glsl::CameraGlobals>::Create();
        if (!ubo) { err += ubo.error(); return err; }
        m_shadowUBO = std::move(*ubo);
    }

    // Create the shadow-map depth texture.
    {
        GLuint id;
        glGenTextures(1, &id);
        m_shadowMapTexture = GLTexture(id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
                     kShadowMapSize, kShadowMapSize,
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindTexture(GL_TEXTURE_2D, 0);
        err += GET_GL_ERROR();
    }

    // Create the FBO and attach the shadow map.
    {
        GLuint fboId;
        glGenFramebuffers(1, &fboId);
        m_shadowFBO = GLFramebuffer(fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, m_shadowMapTexture.get(), 0);
        // No colour attachment needed.
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        OKAMI_ERROR_RETURN_IF(status != GL_FRAMEBUFFER_COMPLETE,
            "OGLDepthPass: Shadow map FBO is incomplete");
        err += GET_GL_ERROR();
    }

    LOG(INFO) << "OGLDepthPass initialised (" << kShadowMapSize << "x" << kShadowMapSize << ")";
    return err;
}

Error OGLDepthPass::BeginDepthPass(glsl::CameraGlobals const& cameraGlobals) {
    Error err;

    // --- Save current FBO and viewport ---------------------------------------
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFBO);
    glGetIntegerv(GL_VIEWPORT, m_prevViewport);

    // --- Upload light-space camera data to the UBO ---------------------------
    err += m_shadowUBO.Write(cameraGlobals);
    OKAMI_ERROR_RETURN(err);

    // --- Bind shadow FBO and prepare render state ----------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO.get());
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);   // reduce peter-panning
    err += GET_GL_ERROR();

    return err;
}

Error OGLDepthPass::EndDepthPass() {
    Error err;

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_prevFBO));
    glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
    err += GET_GL_ERROR();

    return err;
}
