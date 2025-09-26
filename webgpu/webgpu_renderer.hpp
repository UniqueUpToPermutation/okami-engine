#pragma once

#include "../engine.hpp"
#include "../renderer.hpp"
#include <memory>

namespace okami {
    // Factory for creating webgpu renderer modules
    struct WebgpuRendererFactory {
        std::unique_ptr<EngineModule> operator()(RendererParams const& params = {});
    };
}