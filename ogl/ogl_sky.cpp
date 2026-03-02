#include "ogl_sky.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLSkyRenderer::StartupImpl(InitContext const& context) {
    Error err;

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider,
        "IOGLSceneGlobalsProvider interface not available for OGLSkyRenderer");

    // Obtain the default sky material from the material manager.
    auto* matMgr = context.m_interfaces.Query<IMaterialManager<SkyDefaultMaterial>>();
    OKAMI_ERROR_RETURN_IF(!matMgr,
        "IMaterialManager<SkyDefaultMaterial> not available for OGLSkyRenderer");
    m_defaultMaterial = matMgr->CreateMaterial(SkyDefaultMaterial{});

    m_pipelineState.depthTestEnabled = true;
    m_pipelineState.blendEnabled     = false;
    m_pipelineState.cullFaceEnabled  = true;
    m_pipelineState.depthMask        = true;
    m_pipelineState.depthFunc        = GL_LEQUAL;

    LOG(INFO) << "OGL Sky Renderer initialized successfully";
    return err;
}

Error OGLSkyRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    Error err;

    m_pipelineState.SetToGL();
    err += GET_GL_ERROR();

    registry.view<SkyComponent>().each([&](auto entity, SkyComponent const& component) {
        // Resolve effective material (fall back to default when none set).
        auto* mat = static_cast<OGLMaterial*>(
            component.m_skyMaterial
                ? component.m_skyMaterial.get()
                : m_defaultMaterial.get());

        if (mat) {
            mat->Bind();
            err += GET_GL_ERROR();
            err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        err += GET_GL_ERROR();
    });

    return err;
}

std::string OGLSkyRenderer::GetName() const {
    return "OGL Sky Renderer";
}
