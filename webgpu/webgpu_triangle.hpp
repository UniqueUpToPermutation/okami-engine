#pragma once

#include "webgpu_utils.hpp"

#include "../module.hpp"
#include "../storage.hpp"
#include "../renderer.hpp"

#include <webgpu.h>

namespace okami {
    class WebgpuTriangleModule : 
        public EngineModule,
        public IWgpuRenderModule {
    private:
        WGPURenderPipeline m_pipeline = nullptr;
        StorageModule<DummyTriangleComponent>* m_storage = nullptr;
        
        // Transform uniform buffer resources
        WGPUBuffer m_transformBuffer = nullptr;
        WGPUBindGroup m_transformBindGroup = nullptr;
        WGPUBindGroupLayout m_transformBindGroupLayout = nullptr;
        
        // Constants for dynamic buffer management
        static constexpr uint32_t MAX_TRIANGLES = 256;
        static constexpr uint32_t TRANSFORM_ALIGNMENT = 256; // Required alignment for uniform buffers

    protected:
        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl() override;

    public:
        WebgpuTriangleModule();

        std::string_view GetName() const override;
        Error Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) override;
    };
}