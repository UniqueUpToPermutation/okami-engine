

#include "webgpu_utils.hpp"
#include "webgpu_renderer.hpp"
#include "webgpu_triangle.hpp"

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

#include <webgpu.h>

#ifdef __APPLE__
extern "C" void* createMetalLayerForWebGPU(void* nsWindow);
extern "C" void releaseWebGPUMetalLayer(void* layer);
#endif

using namespace okami;

class WebgpuRendererModule : 
    public EngineModule,
    public IRenderer {
protected:
    RendererParams m_params;
    RendererConfig m_config;
    std::atomic<entity_t> m_activeCamera = 0;
    
    WGPUInstance m_instance = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
       
    INativeWindowProvider* m_windowProvider = nullptr;
    glm::ivec2 m_lastFramebufferSize = {0, 0};
    
    WebgpuTriangleModule* m_triangleModule = nullptr;
    
#ifdef __APPLE__
    void* m_metalLayer = nullptr;
#endif

    Error InitWebGPU() {
        LOG(INFO) << "Creating WebGPU instance...";
        
        // Create instance
        WGPUInstanceDescriptor instanceDesc = {};
        m_instance = wgpuCreateInstance(&instanceDesc);
        if (!m_instance) {
            return Error("Failed to create WebGPU instance - wgpu-native may not be loaded properly");
        }
        
        LOG(INFO) << "WebGPU instance created successfully";
        
        // Create surface (if not headless)
        if (m_windowProvider) {
            auto nativeHandle = m_windowProvider->GetNativeWindowHandle();
            m_lastFramebufferSize = m_windowProvider->GetFramebufferSize();
            
            LOG(INFO) << "Creating WebGPU surface for window " << nativeHandle;
            
            WGPUSurfaceDescriptor surfaceDesc = {};
            
#ifdef __APPLE__
            // Create Metal layer from the GLFW window
            m_metalLayer = createMetalLayerForWebGPU(nativeHandle);
            if (!m_metalLayer) {
                return Error("Failed to create Metal layer from window");
            }
            
            WGPUSurfaceDescriptorFromMetalLayer metalDesc = {};
            metalDesc.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
            metalDesc.layer = m_metalLayer;
            surfaceDesc.nextInChain = &metalDesc.chain;
#endif
            
            m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
            if (!m_surface) {
                return Error("Failed to create WebGPU surface");
            }
            
            LOG(INFO) << "WebGPU surface created successfully";
        }
        
        // Request adapter
        WGPURequestAdapterOptions adapterOpts = {};
        adapterOpts.compatibleSurface = m_surface;
        
        // For simplicity, we'll use a blocking approach here
        // In production, you'd want to use the async callback
        m_adapter = RequestAdapterSync(m_instance, &adapterOpts);
        if (!m_adapter) {
            return Error("Failed to get WebGPU adapter");
        }
        
        // Request device
        WGPUDeviceDescriptor deviceDesc = {};
        m_device = RequestDeviceSync(m_adapter, &deviceDesc);
        if (!m_device) {
            return Error("Failed to create WebGPU device");
        }
        
        m_queue = wgpuDeviceGetQueue(m_device);
        
        // Configure surface
        if (m_surface) {
            ConfigureSurface();
        }
        
        return {};
    }
    
    void ConfigureSurface() {
        if (!m_surface || !m_device) return;
        
        WGPUSurfaceConfiguration config = {};
        config.device = m_device;
        config.format = m_surfaceFormat;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.width = m_lastFramebufferSize.x;
        config.height = m_lastFramebufferSize.y;
        config.presentMode = WGPUPresentMode_Fifo;
        
        wgpuSurfaceConfigure(m_surface, &config);
    }
    
    void ReconfigureSurface() {
        ConfigureSurface();
    }
      
    void RenderFrame(ModuleInterface& mi) {
        if (!m_surface) return;
        
        WGPUSurfaceTexture surfaceTexture;
        wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);
        if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            LOG(ERROR) << "Failed to get surface texture, status: " << surfaceTexture.status;
            return;
        }
       
        WGPUTextureViewDescriptor viewDesc = {};
        viewDesc.format = m_surfaceFormat;
        viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 1;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = WGPUTextureAspect_All;
        
        WGPUTextureView backBuffer = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
       
        WGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.nextInChain = nullptr;
        colorAttachment.view = backBuffer;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue.r = 0.3f;
        colorAttachment.clearValue.g = 0.3f;
        colorAttachment.clearValue.b = 0.3f;
        colorAttachment.clearValue.a = 1.0f;
                
        WGPURenderPassDescriptor renderPassDesc = {};
        renderPassDesc.nextInChain = nullptr;
        renderPassDesc.label = nullptr;
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;
        renderPassDesc.depthStencilAttachment = nullptr;
        renderPassDesc.occlusionQuerySet = nullptr;
        renderPassDesc.timestampWrites = nullptr;
              
        WGPUCommandEncoderDescriptor encoderDesc = {};
        encoderDesc.nextInChain = nullptr;
        encoderDesc.label = nullptr;
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);
               
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
               
        // Use triangle module to render triangles
        if (m_triangleModule) {
            WgpuRenderPassInfo info;
            info.m_camera = {}; // TODO: Get actual camera from scene
            info.m_viewportSize = { 800, 600 }; // TODO: Get actual viewport size
            info.m_info = renderPass; // Store WebGPU render pass encoder in user data
            Time time = {}; // TODO: Get actual time from frame context
            m_triangleModule->Pass(time, mi, info);
        }
        
        wgpuRenderPassEncoderEnd(renderPass);
        
        WGPUCommandBufferDescriptor cmdBufferDesc = {};
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
        wgpuQueueSubmit(m_queue, 1, &commands);
        
        wgpuSurfacePresent(m_surface);
        
        wgpuCommandBufferRelease(commands);
        wgpuCommandEncoderRelease(encoder);
        wgpuRenderPassEncoderRelease(renderPass);
        wgpuTextureViewRelease(backBuffer);
    }
    
    void CleanupWebGPU() {
        if (m_queue) {
            wgpuQueueRelease(m_queue);
            m_queue = nullptr;
        }
        if (m_device) {
            wgpuDeviceRelease(m_device);
            m_device = nullptr;
        }
        if (m_adapter) {
            wgpuAdapterRelease(m_adapter);
            m_adapter = nullptr;
        }
        if (m_surface) {
            wgpuSurfaceRelease(m_surface);
            m_surface = nullptr;
        }
        if (m_instance) {
            wgpuInstanceRelease(m_instance);
            m_instance = nullptr;
        }
        
#ifdef __APPLE__
        if (m_metalLayer) {
            releaseWebGPUMetalLayer(m_metalLayer);
            m_metalLayer = nullptr;
        }
#endif
    }
    
    // Helper functions for synchronous adapter/device creation
    static WGPUAdapter RequestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options) {
        struct AdapterRequest {
            WGPUAdapter adapter = nullptr;
            bool done = false;
        } request;
        
        auto callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
            auto* req = static_cast<AdapterRequest*>(userdata);
            if (status == WGPURequestAdapterStatus_Success) {
                req->adapter = adapter;
            }
            req->done = true;
        };
        
        wgpuInstanceRequestAdapter(instance, options, callback, &request);
        
        // Simple blocking wait
        while (!request.done) {
            // In a real implementation, you'd want to process events here
        }
        
        return request.adapter;
    }
    
    static WGPUDevice RequestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor) {
        struct DeviceRequest {
            WGPUDevice device = nullptr;
            bool done = false;
        } request;
        
        auto callback = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata) {
            auto* req = static_cast<DeviceRequest*>(userdata);
            if (status == WGPURequestDeviceStatus_Success) {
                req->device = device;
            }
            req->done = true;
        };
        
        wgpuAdapterRequestDevice(adapter, descriptor, callback, &request);
        
        // Simple blocking wait
        while (!request.done) {
            // In a real implementation, you'd want to process events here
        }
        
        return request.device;
    }

private:
    Error RegisterImpl(ModuleInterface& mi) override {
        mi.m_interfaces.Register<IRenderer>(this);
        RegisterConfig<RendererConfig>(mi.m_interfaces, LOG_WRAP(WARNING));
        return {};
    }

    Error StartupImpl(ModuleInterface& a) override {
        LOG(INFO) << "Starting WebGPU renderer...";
        
        // Get window provider
        m_windowProvider = a.m_interfaces.Query<INativeWindowProvider>();
        if (!m_windowProvider && !m_params.m_headlessMode) {
            return Error("No INativeWindowProvider available for WebGPU initialization");
        }
        
        LOG(INFO) << "Window provider: " << (m_windowProvider ? "found" : "headless mode");
        
        // Initialize WebGPU
        auto err = InitWebGPU();
        OKAMI_ERROR_RETURN(err);
        
        LOG(INFO) << "WebGPU initialized successfully";
        
        // Initialize triangle module with device
        if (m_triangleModule) {
            m_triangleModule->SetDevice(m_device, m_surfaceFormat);
        }
        
        LOG(INFO) << "WebGPU renderer startup complete";
        return {};
    }

    Error ProcessFrameImpl(Time const& t, ModuleInterface& a) override {
        if (!m_device || m_params.m_headlessMode) {
            return {};
        }
        
        // Check for window resize
        if (m_windowProvider) {
            auto currentSize = m_windowProvider->GetFramebufferSize();
            if (currentSize != m_lastFramebufferSize) {
                m_lastFramebufferSize = currentSize;
                ReconfigureSurface();
            }
        }
        
        // Render frame
        RenderFrame(a);
        
        return {};
    }

    void ShutdownImpl(ModuleInterface&) override {
        CleanupWebGPU();
    }

    Error MergeImpl() override {
        return {};
    }

public:
    std::string_view GetName() const override {
        return "WebGPU Renderer Module";
    }
    
    void SetActiveCamera(entity_t e) override {
        m_activeCamera.store(e);
    }
    
    entity_t GetActiveCamera() const override {
        return m_activeCamera.load();
    }   

    WebgpuRendererModule(RendererParams const& params) : m_params(params) {
        SetChildrenProcessFrame(false); // Manually process child modules
        
        // Create triangle module
        m_triangleModule = CreateChild<WebgpuTriangleModule>();
    }
};

std::unique_ptr<EngineModule> WebgpuRendererFactory::operator()(RendererParams const& params) {
    return std::make_unique<WebgpuRendererModule>(params);
}