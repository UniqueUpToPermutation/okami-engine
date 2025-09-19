#pragma once

#include "../module.hpp"

namespace okami {
    struct GLFWModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}