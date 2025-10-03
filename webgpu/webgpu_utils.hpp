#pragma once

#include "../renderer.hpp"

#include <webgpu.h>
#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace okami {
    // Forward declarations of release functions
    void WgpuRelease(WGPUDevice ptr);
    void WgpuRelease(WGPUBuffer ptr);
    void WgpuRelease(WGPUTexture ptr);
    void WgpuRelease(WGPUTextureView ptr);
    void WgpuRelease(WGPUSampler ptr);
    void WgpuRelease(WGPUBindGroup ptr);
    void WgpuRelease(WGPUBindGroupLayout ptr);
    void WgpuRelease(WGPUPipelineLayout ptr);
    void WgpuRelease(WGPURenderPipeline ptr);
    void WgpuRelease(WGPUComputePipeline ptr);
    void WgpuRelease(WGPUShaderModule ptr);
    void WgpuRelease(WGPUCommandEncoder ptr);
    void WgpuRelease(WGPUCommandBuffer ptr);
    void WgpuRelease(WGPURenderPassEncoder ptr);
    void WgpuRelease(WGPUComputePassEncoder ptr);
    void WgpuRelease(WGPUQuerySet ptr);
    void WgpuRelease(WGPUSurface ptr);
    void WgpuRelease(WGPUAdapter ptr);
    void WgpuRelease(WGPUInstance ptr);
    void WgpuRelease(WGPUQueue ptr);

    // RAII wrapper for WebGPU resources
    template<typename T>
    class WgpuAutoPtr {
    private:
        T m_ptr = nullptr;
        
    public:
        // Constructor with resource
        explicit WgpuAutoPtr(T ptr = nullptr) : m_ptr(ptr) {}
        
        // Move constructor
        WgpuAutoPtr(WgpuAutoPtr&& other) noexcept : m_ptr(other.m_ptr) {
            other.m_ptr = nullptr;
        }
        
        // Move assignment
        WgpuAutoPtr& operator=(WgpuAutoPtr&& other) noexcept {
            if (this != &other) {
                reset();
                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;
            }
            return *this;
        }
        
        // Disable copy constructor and copy assignment
        WgpuAutoPtr(const WgpuAutoPtr&) = delete;
        WgpuAutoPtr& operator=(const WgpuAutoPtr&) = delete;
        
        // Destructor
        ~WgpuAutoPtr() {
            reset();
        }
        
        // Get the raw pointer
        T get() const { return m_ptr; }
        
        // Release ownership without destroying
        T release() {
            T ptr = m_ptr;
            m_ptr = nullptr;
            return ptr;
        }
        
        // Reset with new resource
        void reset(T ptr = nullptr) {
            if (m_ptr) {
                WgpuRelease(m_ptr);
            }
            m_ptr = ptr;
        }
        
        // Boolean conversion
        explicit operator bool() const { return m_ptr != nullptr; }
        
        // Dereference (for pointer-like usage)
        T operator->() const { return m_ptr; }

		// Get pointer to the internal pointer (for functions that output to a pointer)
		T* getPtr() { return &m_ptr; }
    };

    // Convenience type aliases for common WebGPU types
    using WDevice = WgpuAutoPtr<WGPUDevice>;
    using WBuffer = WgpuAutoPtr<WGPUBuffer>;
    using WTexture = WgpuAutoPtr<WGPUTexture>;
    using WTextureView = WgpuAutoPtr<WGPUTextureView>;
    using WSampler = WgpuAutoPtr<WGPUSampler>;
    using WBindGroup = WgpuAutoPtr<WGPUBindGroup>;
    using WBindGroupLayout = WgpuAutoPtr<WGPUBindGroupLayout>;
    using WPipelineLayout = WgpuAutoPtr<WGPUPipelineLayout>;
    using WRenderPipeline = WgpuAutoPtr<WGPURenderPipeline>;
    using WComputePipeline = WgpuAutoPtr<WGPUComputePipeline>;
    using WShaderModule = WgpuAutoPtr<WGPUShaderModule>;
    using WCommandEncoder = WgpuAutoPtr<WGPUCommandEncoder>;
    using WCommandBuffer = WgpuAutoPtr<WGPUCommandBuffer>;
    using WRenderPassEncoder = WgpuAutoPtr<WGPURenderPassEncoder>;
    using WComputePassEncoder = WgpuAutoPtr<WGPUComputePassEncoder>;
    using WQuerySet = WgpuAutoPtr<WGPUQuerySet>;
    using WSurface = WgpuAutoPtr<WGPUSurface>;
    using WAdapter = WgpuAutoPtr<WGPUAdapter>;
    using WInstance = WgpuAutoPtr<WGPUInstance>;
    using WQueue = WgpuAutoPtr<WGPUQueue>;

    struct WgpuRenderPassInfo {
		Camera m_camera;
		glm::ivec2 m_viewportSize;
		WGPURenderPassEncoder m_info;
		WGPUQueue m_queue;
		WGPUDevice m_device;
		WGPUTextureFormat m_surfaceFormat;
		bool m_hasDepthStencil = false;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projMatrix;
	};

	class IWgpuRenderModule {
	public:
		virtual ~IWgpuRenderModule() = default;

		virtual Error Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) = 0;
	};

	class IWgpuRenderer {
	public:
		virtual ~IWgpuRenderer() = default;

		virtual WGPUDevice GetDevice() const = 0;
		virtual WGPUTextureFormat GetPreferredSwapchainFormat() const = 0;
	};

	// Utility function to load WGSL shader files
	std::string LoadShaderFile(const std::string& filename);
}