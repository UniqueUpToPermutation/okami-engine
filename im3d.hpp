#pragma once

#include <im3d.h>

#include "module.hpp"

#include <memory>

namespace okami {
    struct Im3dRenderMessage {
        std::shared_ptr<::Im3d::Context> m_context; 
    };

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

    class IIm3dDataProvider {
    public:
        // Thread-safe
        virtual Im3dData const& GetIm3dData() const = 0;
    };
}