#pragma once

#include <entt/meta/meta.hpp>

#include <unordered_map>
#include <optional>

#include "module.hpp"

namespace okami {
    void RegisterMeta();

    struct ComponentMetaData {
        bool b_defaultAddHandler = true;
        bool b_defaultRemoveHandler = true;
        bool b_defaultUpdateHandler = true;

        inline bool NeedsDefaultHandler() const {
            return b_defaultAddHandler || b_defaultRemoveHandler || b_defaultUpdateHandler;
        }
    };

    struct MetaData {
        std::optional<ComponentMetaData> m_componentMetaData;

        inline bool IsComponent() const {
            return m_componentMetaData.has_value();
        }

        inline static MetaData ComponentDefault() {
            MetaData metaData;
            metaData.m_componentMetaData = ComponentMetaData();
            return metaData;
        }
    };

    /*
        Implements default
    */
    struct MetaDataModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}