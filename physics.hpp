#pragma once

#include "module.hpp"

namespace okami {
    struct PhysicsModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}