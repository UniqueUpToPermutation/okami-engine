#include "ogl_depth_pass.hpp"

#include "../renderer.hpp"
#include "../config.hpp"
#include <glog/logging.h>

using namespace okami;

Error OGLDepthPass::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IOGLDepthPassProvider>(this);
    RegisterConfig<ShadowConfig>(interfaces, LOG_WRAP(WARNING));
    return {};
}

Error OGLDepthPass::StartupImpl(InitContext const& context) {
    Error err;

    // Read shadow config from file (falls back to defaults if absent).
    auto cfg = ReadConfig<ShadowConfig>(context.m_interfaces, LOG_WRAP(WARNING));
    m_shadowMapSize = cfg.m_shadowMapSize;

    // Emplace initial ShadowConfig into registry ctx so other modules can
    // read and modify it at runtime via UpdateCtxSignal<ShadowConfig>.
    context.m_registry.ctx().emplace<ShadowConfig>(cfg);

    // Create the cascade UBO (VP matrices for the depth-pass geometry shader).
    {
        auto ubo = UniformBuffer<glsl::ShadowCascadesBlock>::Create();
        if (!ubo) { err += ubo.error(); return err; }
        m_cascadesUBO = std::move(*ubo);
    }

    // Create the shadow-map depth texture array (one layer per cascade).
    {
        GLuint id;
        glGenTextures(1, &id);
        m_shadowMapTexture = GLTexture(id);
        glBindTexture(GL_TEXTURE_2D_ARRAY, id);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                     m_shadowMapSize, m_shadowMapSize, kNumCascades,
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        err += GET_GL_ERROR();
    }

    // Create a layered FBO and attach all cascade layers at once.
    // The geometry shader uses gl_Layer to route each primitive to its cascade.
    {
        GLuint fboId;
        glGenFramebuffers(1, &fboId);
        m_shadowFBO = GLFramebuffer(fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        // glFramebufferTexture (GL 3.2+) attaches every layer, enabling layered rendering.
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                             m_shadowMapTexture.get(), 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        OKAMI_ERROR_RETURN_IF(status != GL_FRAMEBUFFER_COMPLETE,
            "OGLDepthPass: Shadow map FBO is incomplete");
        err += GET_GL_ERROR();
    }

    LOG(INFO) << "OGLDepthPass initialised ("
              << m_shadowMapSize << "x" << m_shadowMapSize
              << " x" << kNumCascades << " cascades)";
    return err;
}

Error OGLDepthPass::BeginDepthPass(glsl::ShadowCascadesBlock const& cascades,
                                   glm::vec4 const& cascadeSplits) {
    Error err;

    // --- Save current FBO and viewport ---------------------------------------
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFBO);
    glGetIntegerv(GL_VIEWPORT, m_prevViewport);

    // --- Store cascade data for OGLSceneModule to consume --------------------
    m_currentCascades = cascades;
    m_currentSplits   = cascadeSplits;

    // --- Upload cascade VP matrices to the UBO (used by the GS) -------------
    err += m_cascadesUBO.Write(cascades);
    OKAMI_ERROR_RETURN(err);

    // --- Bind layered shadow FBO and prepare render state --------------------
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO.get());
    glViewport(0, 0, m_shadowMapSize, m_shadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);   // clears all layers in a layered FBO
    glCullFace(GL_FRONT);           // reduce peter-panning
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
