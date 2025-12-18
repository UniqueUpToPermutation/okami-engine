#pragma once

#include "module.hpp"

#include <imgui.h>

namespace okami {
    constexpr int kImGuiInputPriority = 100;

    struct ImGuiModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };

    struct ImGuiContextObject {
        ImGuiContext* m_context = nullptr;

        inline ImGuiContext* operator->() {
            return m_context;
        }
        inline ImGuiContext& operator*() const {
            return *m_context;
        }
    };

    class IImguiProvider {
    public:
        virtual ImDrawData const& GetDrawData() const = 0;
    };
};