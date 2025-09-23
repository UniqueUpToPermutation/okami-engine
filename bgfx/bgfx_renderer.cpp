#include "bgfx_renderer.hpp"
#include "bgfx_triangle.hpp"
#include "bgfx_texture_manager.hpp"

#include "../config.hpp"
#include "../renderer.hpp"
#include "../storage.hpp"
#include "../camera.hpp"
#include "../transform.hpp"

#include <glog/logging.h>

#include <cstring>

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

using namespace okami;
class BgfxRendererModule : 
    public EngineModule,
    public IRenderer {
protected:
    RendererConfig m_config;
    glm::ivec2 m_lastFramebufferSize = {0, 0};
    
    StorageModule<Camera>* m_cameraModule = nullptr;

    BgfxTriangleModule* m_triangleModule = nullptr;
    BgfxTextureManager* m_textureManager = nullptr;

    std::atomic<entity_t> m_activeCamera = 0;

    constexpr static bgfx::ViewId kClearView = 0;

private:
    Error RegisterImpl(ModuleInterface& mi) override {
        mi.m_interfaces.Register<IRenderer>(this);
        RegisterConfig<RendererConfig>(mi.m_interfaces, LOG_WRAP(WARNING));
        return {};
    }

    Error StartupImpl(ModuleInterface& a) override{
        m_config = ReadConfig<RendererConfig>(a.m_interfaces, LOG_WRAP(WARNING));
        
        // Call bgfx::renderFrame before bgfx::init to signal to bgfx not to create a render thread.
        // Most graphics APIs must be used on the same thread that created the window.
        bgfx::renderFrame();
        // Initialize bgfx using the native window handle and window resolution.
        bgfx::Init init;

        auto windowProvider = a.m_interfaces.Query<INativeWindowProvider>();
        if (!windowProvider) {
            return Error("No INativeWindowProvider available for bgfx initialization");
        }

        init.platformData.nwh = windowProvider->GetNativeWindowHandle();
        init.platformData.ndt = windowProvider->GetNativeDisplayType();
        m_lastFramebufferSize = windowProvider->GetFramebufferSize();

        init.resolution.width = (uint32_t)m_lastFramebufferSize.x;
        init.resolution.height = (uint32_t)m_lastFramebufferSize.y;
        init.resolution.reset = BGFX_RESET_VSYNC;
        if (!bgfx::init(init)) {
            return Error("Failed to initialize bgfx");
        }

        // Set view 0 to the same dimensions as the window and to clear the color buffer.
        bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0xffffffff
			, 1.0f
			, 0
			);
        bgfx::setDebug(BGFX_DEBUG_TEXT);

        return {};
    }

    Error ProcessFrameImpl(Time const& t, ModuleInterface& a) override {
        m_textureManager->ProcessNewResources({});
        
        auto transformView = a.m_interfaces.Query<IComponentView<Transform>>();
        if (!transformView) {
            return Error("No IComponentView<Transform> available in BgfxRendererModule");
        }

        Error e;

        // Set view 0 default viewport.
        bgfx::setViewRect(0, 0, 0, 
            uint16_t(m_lastFramebufferSize.x), uint16_t(m_lastFramebufferSize.y));
        
        bgfx::touch(0);

        auto* cameraPtr = m_cameraModule->TryGet(m_activeCamera.load());
        auto camera = cameraPtr ? *cameraPtr : Camera::Identity();

        auto projMat = camera.GetProjectionMatrix(
            m_lastFramebufferSize.x, 
            m_lastFramebufferSize.y, 
            true);
        auto viewMat = transformView->GetOr(m_activeCamera.load(), Transform::Identity()).AsMatrix();
        viewMat = glm::inverse(viewMat);
        
        bgfx::setViewTransform(0, &viewMat, &projMat);

        // Draw any triangles that need to be drawn.
        RenderPassInfo info{
            .m_camera = camera,
            .m_viewportSize = m_lastFramebufferSize,
            .m_target = 0
        };
        e += m_triangleModule->Pass(t, a, info);

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();

        return e;
    }

    void ShutdownImpl(ModuleInterface&) override {
        bgfx::shutdown();
    }

    Error MergeImpl() override {
        return {};
    }

public:
    std::string_view GetName() const override {
        return "bgfx Renderer Module";
    }

    Error SaveToFile(const std::string& filename) override {
        return {};
    }
    
    void SetHeadlessMode(bool headless) override {
        // bgfx can support headless rendering
    }

    void SetActiveCamera(entity_t e) override {
        m_activeCamera.store(e);
    }
    
    entity_t GetActiveCamera() const override {
        return m_activeCamera.load();
    }   

    BgfxRendererModule() {
        SetChildrenProcessFrame(false); // Manually process child modules
        m_triangleModule = CreateChild<BgfxTriangleModule>();
        m_cameraModule = CreateChild<StorageModule<Camera>>();
        m_textureManager = CreateChild<BgfxTextureManager>();
    }
};

std::unique_ptr<EngineModule> BgfxRendererFactory::operator()() {
    return std::make_unique<BgfxRendererModule>();
}