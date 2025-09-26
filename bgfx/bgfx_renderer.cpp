#include "bgfx_renderer.hpp"
#include "bgfx_triangle.hpp"
#include "bgfx_texture_manager.hpp"
#include "bgfx_sprite.hpp"

#include "../config.hpp"
#include "../renderer.hpp"
#include "../storage.hpp"
#include "../camera.hpp"
#include "../transform.hpp"
#include "../texture.hpp"
#include "../paths.hpp"

#include <glog/logging.h>

#include <cstring>
#include <filesystem>

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#ifdef __APPLE__
extern "C" void* createMetalLayerForHeadless(int width, int height);
extern "C" void releaseMetalLayer(void* layer);
#endif

using namespace okami;

class BgfxRendererModule : 
    public EngineModule,
    public IRenderer {
protected:
    RendererParams m_params;
    RendererConfig m_config;
    glm::ivec2 m_lastFramebufferSize = {0, 0};
    
    StorageModule<Camera>* m_cameraModule = nullptr;

    BgfxTriangleModule* m_triangleModule = nullptr;
    BgfxTextureManager* m_textureManager = nullptr;
    BgfxSpriteModule* m_spriteModule = nullptr;

    std::atomic<entity_t> m_activeCamera = 0;
    size_t m_frameCounter = 0;
    
    AutoHandle<bgfx::FrameBufferHandle> m_headlessFrameBuffer;
    AutoHandle<bgfx::TextureHandle> m_headlessColorTexture;
    AutoHandle<bgfx::TextureHandle> m_headlessDepthTexture;
    AutoHandle<bgfx::TextureHandle> m_headlessReadbackTexture;
    void* m_headlessMetalLayer = nullptr;

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
        //bgfx::renderFrame();
        // Initialize bgfx using the native window handle and window resolution.
        bgfx::Init init;

        auto windowProvider = a.m_interfaces.Query<INativeWindowProvider>();
        if (!windowProvider && !m_params.m_headlessMode) {
            return Error("No INativeWindowProvider available for bgfx initialization");
        } else if (!windowProvider) {
            m_lastFramebufferSize = glm::ivec2(800, 600);
            // For headless Metal rendering on macOS, create a CAMetalLayer
            #ifdef __APPLE__
            m_headlessMetalLayer = createMetalLayerForHeadless(m_lastFramebufferSize.x, m_lastFramebufferSize.y);
            if (m_headlessMetalLayer) {
                init.platformData.nwh = m_headlessMetalLayer;
                LOG(INFO) << "Headless mode: created CAMetalLayer for Metal rendering";
            } else {
                LOG(WARNING) << "Failed to create Metal layer, falling back to default bgfx behavior";
            }
            #endif
        } else {
            init.platformData.nwh = windowProvider->GetNativeWindowHandle();
            init.platformData.ndt = windowProvider->GetNativeDisplayType();
            m_lastFramebufferSize = windowProvider->GetFramebufferSize();
        }

        init.resolution.width = (uint32_t)m_lastFramebufferSize.x;
        init.resolution.height = (uint32_t)m_lastFramebufferSize.y;
        init.resolution.reset = BGFX_RESET_VSYNC;

        if (!bgfx::init(init)) {
            return Error("Failed to initialize bgfx");
        }

        // Set view 0 to the same dimensions as the window and to clear the color buffer.
        bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0xffffffff  // Blue background instead of white for better visibility
			, 1.0f
			, 0
			);
        bgfx::setDebug(BGFX_DEBUG_TEXT);

        // If headless mode, create offscreen framebuffer
        if (m_params.m_headlessMode) {
            uint32_t width = static_cast<uint32_t>(m_lastFramebufferSize.x);
            uint32_t height = static_cast<uint32_t>(m_lastFramebufferSize.y);
            
            // Create color texture for rendering (render target)
            m_headlessColorTexture = AutoHandle<bgfx::TextureHandle>(
                bgfx::createTexture2D(
                    width, height, false, 1, bgfx::TextureFormat::RGBA8,
                    BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
                )
            );
            
            // Create depth texture
            m_headlessDepthTexture = AutoHandle<bgfx::TextureHandle>(
                bgfx::createTexture2D(
                    width, height, false, 1, bgfx::TextureFormat::D24S8,
                    BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
                )
            );
            
            // Create separate texture for readback (not a render target)
            m_headlessReadbackTexture = AutoHandle<bgfx::TextureHandle>(
                bgfx::createTexture2D(
                    width, height, false, 1, bgfx::TextureFormat::RGBA8,
                    BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
                )
            );
            
            // Create framebuffer with color and depth attachments
            bgfx::TextureHandle attachments[] = { 
                m_headlessColorTexture, 
                m_headlessDepthTexture 
            };
            m_headlessFrameBuffer = AutoHandle<bgfx::FrameBufferHandle>(
                bgfx::createFrameBuffer(2, attachments, false)
            );
            
            LOG(INFO) << "Headless rendering enabled with " << width << "x" << height << " framebuffer";
        }

        return {};
    }

    Error ProcessFrameImpl(Time const& t, ModuleInterface& a) override {
        m_textureManager->ProcessNewResources({});
        
        auto transformView = a.m_interfaces.Query<IComponentView<Transform>>();
        if (!transformView) {
            return Error("No IComponentView<Transform> available in BgfxRendererModule");
        }

        Error e;

        // Set view 0 default viewport and framebuffer
        bgfx::setViewRect(0, 0, 0, 
            uint16_t(m_lastFramebufferSize.x), uint16_t(m_lastFramebufferSize.y));
        
        // If headless mode, render to our offscreen framebuffer
        if (m_params.m_headlessMode && bgfx::isValid(m_headlessFrameBuffer)) {
            bgfx::setViewFrameBuffer(0, m_headlessFrameBuffer);
        }
        
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
        e += m_spriteModule->Pass(t, a, info);

        bgfx::frame();

        // If headless mode, capture frame to file
        if (m_params.m_headlessMode && bgfx::isValid(m_headlessColorTexture) && bgfx::isValid(m_headlessReadbackTexture)) {
            // Copy render target to readback texture using blit
            bgfx::blit(0, m_headlessReadbackTexture, 0, 0, m_headlessColorTexture, 0, 0, 
                       static_cast<uint16_t>(m_lastFramebufferSize.x), 
                       static_cast<uint16_t>(m_lastFramebufferSize.y));

            std::filesystem::path outputDir = GetExecutablePath().parent_path() / m_params.m_headlessRenderOutputDir;
            
            // Create output directory if it doesn't exist
            if (!std::filesystem::exists(outputDir)) {
                std::filesystem::create_directories(outputDir);
            }
            
            // Generate unique filename with frame number
            std::string filename = m_params.m_headlessOutputFileStem + "_" + std::to_string(m_frameCounter) + ".png";
            std::filesystem::path outputPath = outputDir / filename;
            
            // Create buffer for texture readback - RGBA8 format (4 bytes per pixel)
            uint32_t width = static_cast<uint32_t>(m_lastFramebufferSize.x);
            uint32_t height = static_cast<uint32_t>(m_lastFramebufferSize.y);
            std::vector<uint8_t> textureBuffer(width * height * 4);
            
            // Request texture readback from the readback texture
            // bgfx::readTexture params: texture handle, data buffer, mip level
            bgfx::readTexture(m_headlessReadbackTexture, textureBuffer.data(), 0);
            bgfx::frame();

            // Create a Texture object and save it
            TextureDesc desc{
                .type = TextureType::TEXTURE_2D,
                .format = TextureFormat::RGBA8,
                .width = width,
                .height = height,
                .depth = 1,
                .arraySize = 1,
                .mipLevels = 1
            };
            
            Texture texture(desc, std::move(textureBuffer));

            auto saveResult = texture.SavePNG(outputPath);
            if (!saveResult.IsOk()) {
                e += saveResult;
            }
        }

        ++m_frameCounter;

        return e;
    }

    void ShutdownImpl(ModuleInterface&) override {
        m_headlessFrameBuffer.Reset();
        m_headlessColorTexture.Reset();
        m_headlessDepthTexture.Reset();
        m_headlessReadbackTexture.Reset();
        
        // Clean up Metal layer if created
        #ifdef __APPLE__
        if (m_headlessMetalLayer) {
            releaseMetalLayer(m_headlessMetalLayer);
            m_headlessMetalLayer = nullptr;
        }
        #endif
        
        bgfx::shutdown();
    }

    Error MergeImpl() override {
        return {};
    }

public:
    std::string_view GetName() const override {
        return "bgfx Renderer Module";
    }
    
    void SetActiveCamera(entity_t e) override {
        m_activeCamera.store(e);
    }
    
    entity_t GetActiveCamera() const override {
        return m_activeCamera.load();
    }   

    BgfxRendererModule(RendererParams const& params) : m_params(params) {
        SetChildrenProcessFrame(false); // Manually process child modules
        m_triangleModule = CreateChild<BgfxTriangleModule>();
        m_cameraModule = CreateChild<StorageModule<Camera>>();
        m_textureManager = CreateChild<BgfxTextureManager>();
        m_spriteModule = CreateChild<BgfxSpriteModule>(m_textureManager);
    }
};

std::unique_ptr<EngineModule> BgfxRendererFactory::operator()(RendererParams const& params) {
    return std::make_unique<BgfxRendererModule>(params);
}