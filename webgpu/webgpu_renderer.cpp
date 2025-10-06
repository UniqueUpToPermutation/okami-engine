#include "webgpu_utils.hpp"
#include "webgpu_renderer.hpp"
#include "webgpu_triangle.hpp"
#include "webgpu_texture.hpp"

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
#include <chrono>
#include <thread>

#include <webgpu.h>

#ifdef __APPLE__
extern "C" void* createMetalLayerForWebGPU(void* nsWindow);
extern "C" void releaseWebGPUMetalLayer(void* layer);
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define WEBGPU_BACKEND_WGPU
#ifdef WEBGPU_BACKEND_WGPU
#  include <wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

using namespace okami;

class WebgpuRendererModule : 
    public EngineModule,
    public IRenderer,
    public IWgpuRenderer {
protected:
    RendererParams m_params;
    RendererConfig m_config;
    std::atomic<entity_t> m_activeCamera = 0;
    
    // Frame counter for headless capture
    uint32_t m_frameCounter = 0;
    
    WInstance m_instance;
    WSurface m_surface;
    WAdapter m_adapter;
    WDevice m_device;
    WQueue m_queue;
    WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
    
    // Headless rendering resources
    WTexture m_headlessColorTexture;
    WTextureView m_headlessColorView;
    WTexture m_headlessDepthTexture;
    WTextureView m_headlessDepthView;

    // Windowed depth buffer (to match headless mode)
    WTexture m_windowedDepthTexture;
    WTextureView m_windowedDepthView;
       
    INativeWindowProvider* m_windowProvider = nullptr;
    glm::ivec2 m_lastFramebufferSize = {0, 0};
    
    WebgpuTriangleModule* m_triangleModule = nullptr;
    StorageModule<Camera>* m_cameraModule = nullptr;

    WebGpuTextureModule* m_textureModule = nullptr;

#ifdef __APPLE__
    void* m_metalLayer = nullptr;
#endif

    Error InitWebGPU() {
        LOG(INFO) << "Creating WebGPU instance...";
        
        // Create instance
        WGPUInstanceDescriptor instanceDesc = {};
        m_instance = WgpuAutoPtr(wgpuCreateInstance(&instanceDesc));
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
#elif defined(_WIN32)
            // Windows: use HWND
            WGPUSurfaceDescriptorFromWindowsHWND windowsDesc = {};
            windowsDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
            windowsDesc.chain.next = nullptr;
            windowsDesc.hinstance = GetModuleHandle(nullptr);
            windowsDesc.hwnd = nativeHandle;
            surfaceDesc.nextInChain = &windowsDesc.chain;
#endif

            m_surface = WgpuAutoPtr(wgpuInstanceCreateSurface(m_instance.get(), &surfaceDesc));
            if (!m_surface) {
                return Error("Failed to create WebGPU surface");
            }
            
            LOG(INFO) << "WebGPU surface created successfully";
        }
        
        // Request adapter
        WGPURequestAdapterOptions adapterOpts = {};
        adapterOpts.compatibleSurface = m_surface.get();
        
        // For simplicity, we'll use a blocking approach here
        // In production, you'd want to use the async callback
        m_adapter = RequestAdapterSync(m_instance.get(), &adapterOpts);
        if (!m_adapter) {
            return Error("Failed to get WebGPU adapter");
        }
        
        // Request device
        WGPUDeviceDescriptor deviceDesc = {};
        m_device = RequestDeviceSync(m_adapter.get(), &deviceDesc);
        if (!m_device) {
            return Error("Failed to create WebGPU device");
        }
        
        m_queue = WgpuAutoPtr(wgpuDeviceGetQueue(m_device.get()));
        
        // Configure surface or create headless textures
        if (m_surface) {
            ConfigureSurface();
        } else {
            auto err = CreateHeadlessTextures();
            OKAMI_ERROR_RETURN(err);
        }
        
        return {};
    }
    
    void ConfigureSurface() {
        if (!m_surface || !m_device) return;
        
        WGPUSurfaceConfiguration config = {};
        config.device = m_device.get();
        config.format = m_surfaceFormat;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.width = m_lastFramebufferSize.x;
        config.height = m_lastFramebufferSize.y;
        config.presentMode = WGPUPresentMode_Fifo;
        
        wgpuSurfaceConfigure(m_surface.get(), &config);
        
        // Create depth buffer for windowed mode (to match headless mode)
        CreateWindowedDepthBuffer();
    }
    
    Error CreateHeadlessTextures() {
        // Clean up existing headless textures if they exist
        ResetHeadlessTextures();
        
        // Create color texture
        WGPUTextureDescriptor colorTextureDesc = {};
        colorTextureDesc.dimension = WGPUTextureDimension_2D;
        colorTextureDesc.format = m_surfaceFormat;
        colorTextureDesc.mipLevelCount = 1;
        colorTextureDesc.sampleCount = 1;
        colorTextureDesc.size = {
            static_cast<uint32_t>(m_lastFramebufferSize.x),
            static_cast<uint32_t>(m_lastFramebufferSize.y),
            1
        };
        colorTextureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        colorTextureDesc.viewFormatCount = 0;
        colorTextureDesc.viewFormats = nullptr;

        m_headlessColorTexture = WgpuAutoPtr(wgpuDeviceCreateTexture(m_device.get(), &colorTextureDesc));
        if (!m_headlessColorTexture) {
            return Error("Failed to create headless color texture");
        }
        
        // Create color texture view
        WGPUTextureViewDescriptor colorViewDesc = {};
        colorViewDesc.format = m_surfaceFormat;
        colorViewDesc.dimension = WGPUTextureViewDimension_2D;
        colorViewDesc.baseMipLevel = 0;
        colorViewDesc.mipLevelCount = 1;
        colorViewDesc.baseArrayLayer = 0;
        colorViewDesc.arrayLayerCount = 1;

        m_headlessColorView = WgpuAutoPtr(wgpuTextureCreateView(m_headlessColorTexture.get(), &colorViewDesc));
        if (!m_headlessColorView) {
            return Error("Failed to create headless color texture view");
        }
        
        // Create depth texture
        WGPUTextureDescriptor depthTextureDesc = {};
        depthTextureDesc.dimension = WGPUTextureDimension_2D;
        depthTextureDesc.format = WGPUTextureFormat_Depth24Plus;
        depthTextureDesc.mipLevelCount = 1;
        depthTextureDesc.sampleCount = 1;
        depthTextureDesc.size = {
            static_cast<uint32_t>(m_lastFramebufferSize.x),
            static_cast<uint32_t>(m_lastFramebufferSize.y),
            1
        };
        depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthTextureDesc.viewFormatCount = 0;
        depthTextureDesc.viewFormats = nullptr;

        m_headlessDepthTexture = WgpuAutoPtr(wgpuDeviceCreateTexture(m_device.get(), &depthTextureDesc));
        if (!m_headlessDepthTexture) {
            return Error("Failed to create headless depth texture");
        }
        
        // Create depth texture view
        WGPUTextureViewDescriptor depthViewDesc = {};
        depthViewDesc.format = WGPUTextureFormat_Depth24Plus;
        depthViewDesc.dimension = WGPUTextureViewDimension_2D;
        depthViewDesc.baseMipLevel = 0;
        depthViewDesc.mipLevelCount = 1;
        depthViewDesc.baseArrayLayer = 0;
        depthViewDesc.arrayLayerCount = 1;

        m_headlessDepthView = WgpuAutoPtr(wgpuTextureCreateView(m_headlessDepthTexture.get(), &depthViewDesc));
        if (!m_headlessDepthView) {
            return Error("Failed to create headless depth texture view");
        }
        
        LOG(INFO) << "Headless textures created successfully: " << m_lastFramebufferSize.x << "x" << m_lastFramebufferSize.y;
        return {};
    }
    
    void ResetHeadlessTextures() {
        m_headlessColorView.reset();
        m_headlessColorTexture.reset();
        m_headlessDepthView.reset();
        m_headlessDepthTexture.reset();
    }
    
    void CreateWindowedDepthBuffer() {
        // Clean up existing depth buffer if it exists
        ResetWindowedDepthBuffer();
        
        // Create depth texture
        WGPUTextureDescriptor depthTextureDesc = {};
        depthTextureDesc.dimension = WGPUTextureDimension_2D;
        depthTextureDesc.format = WGPUTextureFormat_Depth24Plus;
        depthTextureDesc.mipLevelCount = 1;
        depthTextureDesc.sampleCount = 1;
        depthTextureDesc.size = {
            static_cast<uint32_t>(m_lastFramebufferSize.x),
            static_cast<uint32_t>(m_lastFramebufferSize.y),
            1
        };
        depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthTextureDesc.viewFormatCount = 0;
        depthTextureDesc.viewFormats = nullptr;

        m_windowedDepthTexture = WgpuAutoPtr(wgpuDeviceCreateTexture(m_device.get(), &depthTextureDesc));
        if (!m_windowedDepthTexture) {
            LOG(ERROR) << "Failed to create windowed depth texture";
            return;
        }
        
        // Create depth texture view
        WGPUTextureViewDescriptor depthViewDesc = {};
        depthViewDesc.format = WGPUTextureFormat_Depth24Plus;
        depthViewDesc.dimension = WGPUTextureViewDimension_2D;
        depthViewDesc.baseMipLevel = 0;
        depthViewDesc.mipLevelCount = 1;
        depthViewDesc.baseArrayLayer = 0;
        depthViewDesc.arrayLayerCount = 1;

        m_windowedDepthView = WgpuAutoPtr(wgpuTextureCreateView(m_windowedDepthTexture.get(), &depthViewDesc));
        if (!m_windowedDepthView) {
            LOG(ERROR) << "Failed to create windowed depth texture view";
            return;
        }
        
        LOG(INFO) << "Windowed depth buffer created successfully: " << m_lastFramebufferSize.x << "x" << m_lastFramebufferSize.y;
    }
    
    void ResetWindowedDepthBuffer() {
        m_windowedDepthView.reset();
        m_windowedDepthTexture.reset();
    }
    
    Error CaptureHeadlessFrame() {
        if (!m_headlessColorTexture) {
            return {}; // Not in headless mode or no texture to capture
        }
        
        // Create output directory if it doesn't exist
        std::filesystem::path outputDir = m_params.m_headlessRenderOutputDir;
        if (!std::filesystem::exists(outputDir)) {
            std::filesystem::create_directories(outputDir);
        }
        
        // Generate output filename
        std::filesystem::path outputPath = outputDir / (m_params.m_headlessOutputFileStem + ".png");
        
        // Create a buffer to read the texture data
        uint32_t width = static_cast<uint32_t>(m_lastFramebufferSize.x);
        uint32_t height = static_cast<uint32_t>(m_lastFramebufferSize.y);
        uint32_t unalignedBytesPerRow = width * 4; // BGRA8 = 4 bytes per pixel
        
        // Align bytesPerRow to COPY_BYTES_PER_ROW_ALIGNMENT (256 bytes)
        const uint32_t COPY_BYTES_PER_ROW_ALIGNMENT = 256;
        uint32_t bytesPerRow = ((unalignedBytesPerRow + COPY_BYTES_PER_ROW_ALIGNMENT - 1) / COPY_BYTES_PER_ROW_ALIGNMENT) * COPY_BYTES_PER_ROW_ALIGNMENT;
        uint32_t bufferSize = bytesPerRow * height;
        
        // Create buffer for reading texture data
        WGPUBufferDescriptor bufferDesc = {};
        bufferDesc.size = bufferSize;
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        bufferDesc.mappedAtCreation = false;

        WBuffer readbackBuffer = WgpuAutoPtr(wgpuDeviceCreateBuffer(m_device.get(), &bufferDesc));
        if (!readbackBuffer) {
            return Error("Failed to create readback buffer for headless capture");
        }
        
        // Create command encoder for the copy operation
        WGPUCommandEncoderDescriptor encoderDesc = {};
        WCommandEncoder encoder = WgpuAutoPtr(wgpuDeviceCreateCommandEncoder(m_device.get(), &encoderDesc));
        
        // Copy texture to buffer
        WGPUImageCopyTexture source = {};
        source.texture = m_headlessColorTexture.get();
        source.mipLevel = 0;
        source.origin = {0, 0, 0};
        source.aspect = WGPUTextureAspect_All;
        
        WGPUImageCopyBuffer destination = {};
        destination.buffer = readbackBuffer.get();
        destination.layout.offset = 0;
        destination.layout.bytesPerRow = bytesPerRow;
        destination.layout.rowsPerImage = height;
        
        WGPUExtent3D copySize = {width, height, 1};
        
        wgpuCommandEncoderCopyTextureToBuffer(encoder.get(), &source, &destination, &copySize);
        
        // Submit the copy command and ensure completion
        WGPUCommandBufferDescriptor cmdDesc = {};
        WCommandBuffer commands = WgpuAutoPtr(wgpuCommandEncoderFinish(encoder.get(), &cmdDesc));
        wgpuQueueSubmit(m_queue.get(), 1, commands.getPtr());
        
        // Create a simple fence mechanism - submit an empty command buffer to ensure ordering
        WGPUCommandEncoderDescriptor fenceEncoderDesc = {};
        WCommandEncoder fenceEncoder = WgpuAutoPtr(wgpuDeviceCreateCommandEncoder(m_device.get(), &fenceEncoderDesc));
        WGPUCommandBufferDescriptor fenceCmdDesc = {};
        WCommandBuffer fenceCommands = WgpuAutoPtr(wgpuCommandEncoderFinish(fenceEncoder.get(), &fenceCmdDesc));
        wgpuQueueSubmit(m_queue.get(), 1, fenceCommands.getPtr());
        
        // Map the buffer for reading (this is synchronous in this implementation)
        struct MapData {
            bool done = false;
            WGPUBufferMapAsyncStatus status = WGPUBufferMapAsyncStatus_Unknown;
        } mapData;
        
        auto mapCallback = [](WGPUBufferMapAsyncStatus status, void* userdata) {
            auto* data = static_cast<MapData*>(userdata);
            data->status = status;
            data->done = true;
        };
        
        wgpuBufferMapAsync(readbackBuffer.get(), WGPUMapMode_Read, 0, bufferSize, mapCallback, &mapData);

        // Wait for mapping to complete with proper event processing
        int timeout = 1000; // 1 second timeout
        while (!mapData.done && timeout > 0) {
            // Process WebGPU events to handle the async callback
            #ifdef __EMSCRIPTEN__
                emscripten_sleep(1);
            #else
                wgpuDevicePoll(m_device.get(), true, nullptr);
            #endif
            --timeout;
        }
        
        if (!mapData.done) {
            return Error("Timeout waiting for buffer mapping to complete");
        }
        
        if (mapData.status != WGPUBufferMapAsyncStatus_Success) {
            return Error("Failed to map readback buffer");
        }
        
        // Get the mapped data
        const uint8_t* mappedData = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(readbackBuffer.get(), 0, bufferSize));
        if (!mappedData) {
            return Error("Failed to get mapped buffer data");
        }
        
        // Convert BGRA to RGBA and create texture
        std::vector<uint8_t> textureData(width * height * 4); // Final image data without padding
        
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t srcOffset = y * bytesPerRow + x * 4;
                uint32_t dstOffset = (y * width + x) * 4;
                
                // Convert BGRA to RGBA
                textureData[dstOffset + 0] = mappedData[srcOffset + 2]; // R
                textureData[dstOffset + 1] = mappedData[srcOffset + 1]; // G
                textureData[dstOffset + 2] = mappedData[srcOffset + 0]; // B
                textureData[dstOffset + 3] = mappedData[srcOffset + 3]; // A
            }
        }
        
        // Create texture description
        TextureDesc desc;
        desc.type = TextureType::TEXTURE_2D;
        desc.format = TextureFormat::RGBA8;
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.arraySize = 1;
        desc.mipLevels = 1;
        
        // Create texture and save as PNG
        Texture texture(desc, std::move(textureData));
        auto saveResult = texture.SavePNG(outputPath);
        
        // Clean up WebGPU resources
        wgpuBufferUnmap(readbackBuffer.get());
        
        if (saveResult.IsError()) {
            return Error("Failed to save PNG: " + saveResult.Str());
        }
        
        LOG(INFO) << "Headless render captured and saved to: " << outputPath;
        return {};
    }
      
    void RenderFrame(ModuleInterface& mi) {
        WGPUTextureView backBufferView = nullptr;
        WGPUSurfaceTexture surfaceTexture = {};
        
        // Get the appropriate render target
        if (m_surface) {
            // Windowed mode - get surface texture
            wgpuSurfaceGetCurrentTexture(m_surface.get(), &surfaceTexture);
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
            
            backBufferView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
        } else if (m_headlessColorView) {
            // Headless mode - use offscreen texture
            backBufferView = m_headlessColorView.get();
        } else {
            LOG(ERROR) << "No valid render target available";
            return;
        }
       
        WGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.nextInChain = nullptr;
        colorAttachment.view = backBufferView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue.r = 0.1f;
        colorAttachment.clearValue.g = 0.1f;
        colorAttachment.clearValue.b = 0.1f;
        colorAttachment.clearValue.a = 1.0f;
        
        // Setup depth attachment for both windowed and headless modes
        WGPURenderPassDepthStencilAttachment depthAttachment = {};
        WGPURenderPassDescriptor renderPassDesc = {};
        renderPassDesc.nextInChain = nullptr;
        renderPassDesc.label = nullptr;
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;
        
        // Always use depth buffer - windowed or headless
        WGPUTextureView depthView = nullptr;
        if (m_headlessDepthView) {
            depthView = m_headlessDepthView.get();
        } else if (m_surface && m_windowedDepthView) {
            depthView = m_windowedDepthView.get();
        }
        
        if (depthView) {
            depthAttachment.view = depthView;
            depthAttachment.depthClearValue = 1.0f;
            depthAttachment.depthLoadOp = WGPULoadOp_Clear;
            depthAttachment.depthStoreOp = WGPUStoreOp_Store;
            depthAttachment.depthReadOnly = false;
            depthAttachment.stencilClearValue = 0;
            depthAttachment.stencilLoadOp = WGPULoadOp_Clear;
            depthAttachment.stencilStoreOp = WGPUStoreOp_Store;
            depthAttachment.stencilReadOnly = false;
            renderPassDesc.depthStencilAttachment = &depthAttachment;
        } else {
            LOG(WARNING) << "No depth buffer available for rendering";
            renderPassDesc.depthStencilAttachment = nullptr;
        }
        
        renderPassDesc.occlusionQuerySet = nullptr;
        renderPassDesc.timestampWrites = nullptr;
              
        WGPUCommandEncoderDescriptor encoderDesc = {};
        encoderDesc.nextInChain = nullptr;
        encoderDesc.label = nullptr;
        WCommandEncoder encoder = WgpuAutoPtr(wgpuDeviceCreateCommandEncoder(m_device.get(), &encoderDesc));

        WRenderPassEncoder renderPass = WgpuAutoPtr(wgpuCommandEncoderBeginRenderPass(encoder.get(), &renderPassDesc));

        // Use triangle module to render triangles
        if (m_triangleModule) {
            // Get active camera
            auto* cameraPtr = m_cameraModule->TryGet(m_activeCamera.load());
            auto camera = cameraPtr ? *cameraPtr : Camera::Identity();
            
            // Calculate camera matrices
            auto projMat = camera.GetProjectionMatrix(
                m_lastFramebufferSize.x,
                m_lastFramebufferSize.y,
                true
            );
            
            // Get camera transform and calculate view matrix (inverse of camera transform)
            auto transformView = mi.m_interfaces.Query<IComponentView<Transform>>();
            auto cameraTransform = transformView ? 
                transformView->GetOr(m_activeCamera.load(), Transform::Identity()).AsMatrix() :
                glm::mat4(1.0f);
            auto viewMat = glm::inverse(cameraTransform);
            
            WgpuRenderPassInfo info;
            info.m_camera = camera;
            info.m_viewportSize = m_lastFramebufferSize;
            info.m_info = renderPass.get();
            info.m_queue = m_queue.get();
            info.m_device = m_device.get();
            info.m_surfaceFormat = m_surfaceFormat;
            info.m_hasDepthStencil = true; // Both windowed and headless modes have depth
            info.m_viewMatrix = viewMat;
            info.m_projMatrix = projMat;
            Time time = {}; // TODO: Get actual time from frame context
            m_triangleModule->Pass(time, mi, info);
        }
        
        wgpuRenderPassEncoderEnd(renderPass.get());
        
        WGPUCommandBufferDescriptor cmdBufferDesc = {};
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder.get(), &cmdBufferDesc);
        wgpuQueueSubmit(m_queue.get(), 1, &commands);

        // Only present if we have a surface (windowed mode)
        if (m_surface) {
            wgpuSurfacePresent(m_surface.get());
        } else if (m_params.m_headlessRenderToFile) {
            // Capture headless render to PNG
            auto captureResult = CaptureHeadlessFrame();
            if (captureResult.IsError()) {
                LOG(ERROR) << "Failed to capture headless frame: " << captureResult.Str();
            }
        }
    }
    
    void CleanupWebGPU() {
        // Clean up headless textures first
        ResetHeadlessTextures();
        
        // Clean up windowed depth buffer
        ResetWindowedDepthBuffer();

        m_queue.reset();
        m_device.reset();
        m_adapter.reset();
        m_surface.reset();
        m_instance.reset();
        
#ifdef __APPLE__
        if (m_metalLayer) {
            releaseWebGPUMetalLayer(m_metalLayer);
            m_metalLayer = nullptr;
        }
#endif
    }
    
    // Helper functions for synchronous adapter/device creation
    static WAdapter RequestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions* options) {
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
        
        return WgpuAutoPtr(request.adapter);
    }

    static WDevice RequestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor) {
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
        
        return WgpuAutoPtr(request.device);
    }

    WGPUDevice GetDevice() const override {
        return m_device.get();
    }

    WGPUTextureFormat GetPreferredSwapchainFormat() const override {
        return m_surfaceFormat;
    }
    
private:
    Error RegisterImpl(ModuleInterface& mi) override {
        mi.m_interfaces.Register<IRenderer>(this);
        mi.m_interfaces.Register<IWgpuRenderer>(this);
        RegisterConfig<RendererConfig>(mi.m_interfaces, LOG_WRAP(WARNING));
        return {};
    }

    Error StartupImpl(ModuleInterface& a) override {
        LOG(INFO) << "Starting WebGPU renderer...";
        
        // Get window provider
        m_windowProvider = a.m_interfaces.Query<INativeWindowProvider>();
        
        // Set default framebuffer size for headless mode
        if (!m_windowProvider) {
            m_lastFramebufferSize = {800, 600}; // Default headless size
        }
        
        LOG(INFO) << "Window provider: " << (m_windowProvider ? "found" : "headless mode");
        
        // Initialize WebGPU
        auto err = InitWebGPU();
        OKAMI_ERROR_RETURN(err);
        
        LOG(INFO) << "WebGPU renderer startup complete";
        return {};
    }

    Error ProcessFrameImpl(Time const& t, ModuleInterface& a) override {
        if (!m_device) {
            return {};
        }

        // Check for window resize (only for windowed mode)
        if (m_windowProvider) {
            auto currentSize = m_windowProvider->GetFramebufferSize();
            if (currentSize != m_lastFramebufferSize) {
                m_lastFramebufferSize = currentSize;
                ConfigureSurface();
            }
        }

        // Upload textures if any new ones were added
        WebGpuTextureModuleUserData textureUserData{
            .m_device = m_device.get(),
            .m_queue = m_queue.get()
        };
        m_textureModule->ProcessNewResources(textureUserData);
        
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
        
        m_triangleModule = CreateChild<WebgpuTriangleModule>();
        m_cameraModule = CreateChild<StorageModule<Camera>>();
        m_textureModule = CreateChild<WebGpuTextureModule>();
    }
};

std::unique_ptr<EngineModule> WebgpuRendererFactory::operator()(RendererParams const& params) {
    return std::make_unique<WebgpuRendererModule>(params);
}