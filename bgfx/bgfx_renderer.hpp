#pragma once

#include "../engine.hpp"
#include "../renderer.hpp"
#include <memory>

namespace okami {
    // Factory for creating bgfx renderer modules
    struct BgfxRendererFactory {
        std::unique_ptr<EngineModule> operator()(RendererParams const& params = {});
    };
}