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
        WGPUDevice m_device = nullptr;
        WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
        StorageModule<DummyTriangleComponent>* m_storage = nullptr;

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
        
        void SetDevice(WGPUDevice device, WGPUTextureFormat surfaceFormat);
    };
}