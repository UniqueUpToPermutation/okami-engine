#pragma once

#include "../module.hpp"

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace okami {
    class BGFXTriangleModule : public EngineModule {
    private:
        bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;

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