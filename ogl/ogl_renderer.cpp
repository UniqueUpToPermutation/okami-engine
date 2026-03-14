#include "ogl_renderer.hpp"
#include "ogl_utils.hpp"
#include "ogl_triangle.hpp"
#include "ogl_texture.hpp"
#include "ogl_sprite.hpp"
#include "ogl_geometry.hpp"
#include "ogl_static_mesh.hpp"
#include "ogl_depth_pass.hpp"
#include "ogl_im3d.hpp"
#include "ogl_imgui.hpp"
#include "ogl_brdf.hpp"
#include "ogl_sky.hpp"
#include "ogl_scene.hpp"

#include "../config.hpp"
#include "../camera.hpp"
#include "../transform.hpp"
#include "../light.hpp"

#include <glog/logging.h>

#include <glad/gl.h>

using namespace okami;

class OGLRendererModule final : 
    public EngineModule, 
    public IRenderModule {
private:
    RendererParams m_params;
    RendererConfig m_config;
    std::atomic<entity_t> m_activeCamera = kNullEntity;

    IGLProvider* m_glProvider = nullptr;
    std::unique_ptr<IGLShaderCache> m_shaderCache;

    OGLGeometryManager* m_geometryManager = nullptr;

    OGLTriangleRenderer* m_triangleRenderer = nullptr;
    OGLTextureManager* m_textureManager = nullptr;
    OGLSpriteRenderer* m_spriteRenderer = nullptr;

    OGLStaticMeshRenderer* m_staticMeshRenderer = nullptr;
    OGLIm3DRenderer* m_im3dRenderer = nullptr;
    OGLImguiRenderer* m_imguiRenderer = nullptr;
    OGLSkyRenderer* m_skyRenderer = nullptr;
    OGLDepthPass* m_depthPass = nullptr;

    OGLBrdfProvider* m_brdfProvider = nullptr;

    OGLMaterialManager* m_materialManager = nullptr;

    OGLSceneModule* m_sceneModule = nullptr;
    
protected:
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IRenderModule>(this);
        interfaces.Register<IGLShaderCache>(m_shaderCache.get());
        RegisterConfig<RendererConfig>(interfaces, LOG_WRAP(WARNING));

        m_glProvider = interfaces.Query<IGLProvider>();
        OKAMI_ERROR_RETURN_IF(!m_glProvider, "IGLProvider interface not available for OpenGL renderer");

        m_glProvider->NotifyNeedGLContext();

        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        int version = gladLoadGL(m_glProvider->GetGLLoaderFunction());

        OKAMI_ERROR_RETURN_IF(version == 0, "Failed to initialize OpenGL context via GLAD");

        m_glProvider->SetSwapInterval(1);

        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error Render(entt::registry const& registry) override {
        m_shaderCache.reset();

        const entity_t activeCam = m_activeCamera.load(std::memory_order_relaxed);

        // ── Shadow pass ──────────────────────────────────────────────────────
        // Find the first shadow-casting directional light and run a depth pass.
        {
            auto* viewCam       = registry.try_get<Camera>(activeCam);
            auto* viewTransform = registry.try_get<Transform>(activeCam);

            if (viewCam && viewTransform) {
                auto view = registry.view<DirectionalLightComponent>();
                for (auto entity : view) {
                    auto const& light = view.get<DirectionalLightComponent>(entity);
                    if (!light.b_castShadow) continue;

                    auto framebufferSize = m_glProvider->GetFramebufferSize();
                    ShadowCascade cascade = ComputeShadowCascade(
                        light, *viewCam, *viewTransform, framebufferSize, 0.0f, 10.0f,
                        OGLDepthPass::kShadowMapSize);

                    const glm::mat4 lightView =
                        cascade.transform.Inverse().AsMatrix();
                    const glm::mat4 lightProj =
                        cascade.camera.GetProjectionMatrix(
                            OGLDepthPass::kShadowMapSize,
                            OGLDepthPass::kShadowMapSize,
                            /*usingDirectX=*/false);
                    const glm::mat4 lightVP = lightProj * lightView;

                    glsl::CameraGlobals lightCamera{};
                    lightCamera.u_view        = lightView;
                    lightCamera.u_proj        = lightProj;
                    lightCamera.u_viewProj    = lightVP;
                    lightCamera.u_invView     = cascade.transform.AsMatrix();
                    lightCamera.u_invProj     = glm::inverse(lightProj);
                    lightCamera.u_invViewProj = glm::inverse(lightVP);

                    m_depthPass->BeginDepthPass(lightCamera);

                    OGLPass shadowPass{ .m_type = OGLPassType::Shadow };
                    m_staticMeshRenderer->Pass(registry, shadowPass);

                    m_depthPass->EndDepthPass();
                    break; // single cascade for now
                }
            }
        }

        // ── Forward pass ─────────────────────────────────────────────────────
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        OGLPass pass{};

        m_sceneModule->UpdateSceneGlobals(registry, activeCam);

        m_staticMeshRenderer->Pass(registry, pass);
        m_triangleRenderer->Pass(registry, pass);
        m_spriteRenderer->Pass(registry, pass);
        m_im3dRenderer->Pass(registry, pass);
        m_imguiRenderer->Pass(registry, pass);
        m_skyRenderer->Pass(registry, pass);
        m_glProvider->SwapBuffers();

        return {};
    }

public:
    OGLRendererModule(RendererParams const& params) : 
        m_params(params), 
        m_shaderCache(CreateGLShaderCache()) {
        SetChildrenProcessFrame(false);

        m_sceneModule = CreateChild<OGLSceneModule>();

        m_textureManager = CreateChild<OGLTextureManager>();
        m_materialManager = CreateChild<OGLMaterialManager>();
        m_geometryManager = CreateChild<OGLGeometryManager>();

        m_triangleRenderer = CreateChild<OGLTriangleRenderer>();
        m_spriteRenderer = CreateChild<OGLSpriteRenderer>();
        m_staticMeshRenderer = CreateChild<OGLStaticMeshRenderer>(m_geometryManager);
        m_im3dRenderer = CreateChild<OGLIm3DRenderer>();
        m_imguiRenderer = CreateChild<OGLImguiRenderer>();
        m_skyRenderer = CreateChild<OGLSkyRenderer>();
        m_depthPass = CreateChild<OGLDepthPass>();

        // m_brdfProvider = CreateChild<OGLBrdfProvider>(/*debug = */ false);
    }

    std::string GetName() const override {
        return "OpenGL Renderer Module";
    }

    void SetActiveCamera(entity_t e) override {
        m_activeCamera.store(e, std::memory_order_relaxed);
    }

    entity_t GetActiveCamera() const override {
        return m_activeCamera.load(std::memory_order_relaxed);
    }
};

std::unique_ptr<EngineModule> OGLRendererFactory::operator()(RendererParams const& params) {
    return std::make_unique<OGLRendererModule>(params);
}

