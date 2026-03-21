#pragma once

#include "module.hpp"

#include <memory>

namespace okami {
    // A lightweight scene inspector that renders two ImGui windows:
    //   "Scene"    — lists all alive entities; click to select one.
    //   "Inspector" — lists components on the selected entity by name,
    //                 with expanded detail for known component types.
    struct EditorModuleFactory {
        std::unique_ptr<EngineModule> operator()() const;
    };
} // namespace okami
