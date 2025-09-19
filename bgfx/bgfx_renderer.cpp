#include "bgfx_renderer.hpp"
#include "bgfx_triangle.hpp"

#include "../config.hpp"
#include "../renderer.hpp"

#include "../glfw/glfw_module.hpp"

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
    
    BGFXTriangleModule* m_triangleModule = nullptr;

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
        // Set view 0 default viewport.
        bgfx::setViewRect(0, 0, 0, 
            uint16_t(m_lastFramebufferSize.x), uint16_t(m_lastFramebufferSize.y));
        
        bgfx::touch(0);

        // Draw any triangles that need to be drawn.
        m_triangleModule->ProcessFrame(t, a);

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();

        return {};
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
    }
    
    entity_t GetActiveCamera() const override {
        return {};
    }   

    BgfxRendererModule() {
        SetChildrenProcessFrame(false); // Manually process child modules
        m_triangleModule = CreateChild<BGFXTriangleModule>();
    }
};

std::unique_ptr<EngineModule> BgfxRendererFactory::operator()() {
    return std::make_unique<BgfxRendererModule>();
}