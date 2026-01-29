#include "ogl_renderer.hpp"
#include "ogl_utils.hpp"
#include "ogl_triangle.hpp"
#include "ogl_texture.hpp"
#include "ogl_sprite.hpp"
#include "ogl_geometry.hpp"
#include "ogl_static_mesh.hpp"
#include "ogl_im3d.hpp"
#include "ogl_imgui.hpp"
#include "ogl_brdf.hpp"
#include "ogl_sky.hpp"

#include "../config.hpp"
#include "../storage.hpp"
#include "../camera.hpp"
#include "../transform.hpp"

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

    OGLBrdfProvider* m_brdfProvider = nullptr;

    OGLDefaultMaterialManager* m_defaultMaterialManager = nullptr;
    OGLBasicTexturedMaterialManager* m_basicTexturedMaterialManager = nullptr;
    OGLSkyDefaultMaterialManager* m_skyDefaultMaterialManager = nullptr;

    StorageModule<Camera>* m_cameraStorage = nullptr;
    IComponentView<Transform>* m_transformView = nullptr;

protected:
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IRenderModule>(this);
        interfaces.Register<IGLShaderCache>(m_shaderCache.get());
        RegisterConfig<RendererConfig>(interfaces, LOG_WRAP(WARNING));

        m_glProvider = interfaces.Query<IGLProvider>();
        OKAMI_ERROR_RETURN_IF(!m_glProvider, "IGLProvider interface not available for OpenGL renderer");

        m_transformView = interfaces.Query<IComponentView<Transform>>();
        OKAMI_ERROR_RETURN_IF(!m_transformView, "IComponentView<Transform> interface not available for OpenGL renderer");

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

    glsl::SceneGlobals GetSceneGlobals() {
        auto cameraPtr = m_cameraStorage->TryGet(m_activeCamera.load(std::memory_order_relaxed));
        auto camera = cameraPtr ? *cameraPtr : Camera::Identity();

        auto framebufferSize = m_glProvider->GetFramebufferSize();
        glViewport(0, 0, framebufferSize.x, framebufferSize.y);

        auto cameraTransform = m_transformView->GetOr(
            m_activeCamera.load(std::memory_order_relaxed), 
            Transform::Identity());
        glm::mat4 viewMatrix = cameraTransform.Inverse().AsMatrix();
        glm::mat4 projMatrix = camera.GetProjectionMatrix(framebufferSize.x, framebufferSize.y, false);
        glm::mat4 viewProjMatrix = projMatrix * viewMatrix;

        return glsl::SceneGlobals{
            .u_camera = {
                .u_view = viewMatrix,
                .u_proj = projMatrix,
                .u_viewProj = viewProjMatrix,
                .u_invView = cameraTransform.AsMatrix(),
                .u_invProj = glm::inverse(projMatrix),
                .u_invViewProj = glm::inverse(viewProjMatrix),

                .u_viewport = glm::vec4(m_glProvider->GetFramebufferSize(), 0.0f, 0.0f),
                .u_cameraPosition = glm::vec4(cameraTransform.m_position, 1.0f),
                .u_cameraDirection = glm::vec4(cameraTransform.TransformVector(glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f)
            }
        };
    }

    Error Render() override {
        m_shaderCache.reset();

        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        //glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        OGLPass pass{
            .m_sceneGlobals = GetSceneGlobals(),
        };

        m_staticMeshRenderer->Pass(pass);
        m_triangleRenderer->Pass(pass);
        m_spriteRenderer->Pass(pass);
        m_im3dRenderer->Pass(pass);
        m_imguiRenderer->Pass(pass);
        m_skyRenderer->Pass(pass);
        m_glProvider->SwapBuffers();

        return {};
    }

public:
    OGLRendererModule(RendererParams const& params) : 
        m_params(params), 
        m_shaderCache(CreateGLShaderCache()) {
        SetChildrenProcessFrame(false);
        
        m_cameraStorage = CreateChild<StorageModule<Camera>>();
        m_triangleRenderer = CreateChild<OGLTriangleRenderer>();
        m_textureManager = CreateChild<OGLTextureManager>();
        m_geometryManager = CreateChild<OGLGeometryManager>();
        m_spriteRenderer = CreateChild<OGLSpriteRenderer>(m_textureManager);
        m_staticMeshRenderer = CreateChild<OGLStaticMeshRenderer>(m_geometryManager);
        m_im3dRenderer = CreateChild<OGLIm3DRenderer>();
        m_imguiRenderer = CreateChild<OGLImguiRenderer>();
        m_skyRenderer = CreateChild<OGLSkyRenderer>();

        m_defaultMaterialManager = CreateChild<OGLDefaultMaterialManager>();
        m_basicTexturedMaterialManager = CreateChild<OGLBasicTexturedMaterialManager>(m_textureManager);
        m_skyDefaultMaterialManager = CreateChild<OGLSkyDefaultMaterialManager>();

        m_brdfProvider = CreateChild<OGLBrdfProvider>(/*debug = */ false);
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

