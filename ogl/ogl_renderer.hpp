#pragma once

#include "../renderer.hpp"

namespace okami {
    struct OGLRendererFactory {
        std::unique_ptr<EngineModule> operator()(RendererParams const& params);
    };
}