#pragma once

#include "../module.hpp"
#include "bgfx_util.hpp"

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace okami {
    class BGFXTriangleModule : public EngineModule {
    private:
        AutoHandle<bgfx::ProgramHandle> m_program;

    protected:
        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl() override;

    public:
        std::string_view GetName() const override;
    };
}