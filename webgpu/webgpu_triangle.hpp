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
        WRenderPipeline m_pipeline;
        StorageModule<DummyTriangleComponent>* m_storage = nullptr;
        
        // Transform uniform buffer resources
        WBuffer m_transformBuffer;
        WBindGroup m_transformBindGroup;
        WBindGroupLayout m_transformBindGroupLayout;
        
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