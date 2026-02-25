#pragma once

#include "../module.hpp"
#include "../imgui.hpp"
#include "ogl_utils.hpp"

namespace okami {
    class OGLImguiRenderer final :
        public EngineModule,
        public IOGLRenderModule,
        public IImguiRenderer {
    private:
        IImguiProvider* m_imguiProvider = nullptr;

    protected:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;
        void ShutdownImpl(InitContext const& context) override;

    public:
        Error Pass(entt::registry const& registry, OGLPass const& pass) override;

        std::string GetName() const override;
    };
}