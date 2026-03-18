#pragma once

#include "../module.hpp"

namespace okami {
    // Owns the RenderDebugConfig registry ctx variable and provides an ImGui
    // panel to switch the active debug visualization mode at runtime.
    class OGLDebugModule final : public EngineModule {
    protected:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

    public:
        std::string GetName() const override { return "OGL Debug Module"; }
    };
}
