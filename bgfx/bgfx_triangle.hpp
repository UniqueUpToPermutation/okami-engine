#pragma once

#include "../module.hpp"
#include "../storage.hpp"
#include "../renderer.hpp"
#include "bgfx_util.hpp"

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace okami {
    class BGFXTriangleModule : 
        public EngineModule,
        public IRenderModule{
    private:
        AutoHandle<bgfx::ProgramHandle> m_program;
        StorageModule<DummyTriangleComponent>* m_storage = nullptr;

    protected:
        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl() override;

    public:
        BGFXTriangleModule();

        std::string_view GetName() const override;
        Error Pass(Time const& time, ModuleInterface& mi, RenderPassInfo info) override;
    };
}