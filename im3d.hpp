#pragma once

#include <im3d.h>

#include "module.hpp"

#include <memory>

namespace okami {
    struct Im3dModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };

    struct Im3dData {
        std::vector<Im3d::VertexData> m_data;      // owned vertex data
        std::vector<Im3d::DrawList>   m_drawLists;  // point into m_data

        uint32_t getDrawListCount() const { return static_cast<uint32_t>(m_drawLists.size()); }
        Im3d::DrawList const* getDrawLists() const { return m_drawLists.data(); }

        inline void Clear() {
            m_data.clear();
            m_drawLists.clear();
        }
    };

    struct Im3dContext {
        std::shared_ptr<Im3d::Context> m_context;

        inline Im3d::Context* operator->() {
            return m_context.get();
        }

        inline Im3d::Context const* operator->() const {
            return m_context.get();
        }
    };

    class IIm3dProvider {
    public:
        // Thread-safe
        virtual Im3dData const& GetIm3dData() const = 0;
    };
}