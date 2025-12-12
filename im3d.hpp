#pragma once

#include <im3d.h>

#include "module.hpp"

#include <memory>

namespace okami {
    struct Im3dModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };

    struct Im3dData {
        std::vector<Im3d::VertexData> m_data;
        std::vector<Im3d::DrawList> m_drawLists;

        inline void Clear() {
            m_data.clear();
            m_drawLists.clear();
        }
    };

    class IIm3dProvider {
    public:
        // Thread-safe
        virtual Im3d::Context const& GetIm3dContext() const = 0;
    };
}